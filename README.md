# RGB Hockey Stick

An ESP8266-based LED controller for a hockey stick. Two firmware options:

1. **Original firmware** — WiFi-connected, monitors the NHL API and reacts to live game events (goals, game start/end). Includes a web UI for manual control.
2. **WLED firmware** — Runs [WLED](https://kno.wled.ge) on the stick and integrates with Home Assistant, so the stick can mirror any other light (color, brightness, on/off).

---

## Hardware

- ESP8266 (NodeMCU or similar)
- WS2812B/NeoPixel LED strip (100 LEDs)
- LED data line on GPIO 4 (D2 on NodeMCU)

---

## Setup

### 1. Configure WiFi

```bash
cp .env.example .env
```

Edit `.env` with your WiFi credentials. This file is gitignored and never committed.

### 2. Find your serial port

Plug in the ESP8266 and check which port it appears on:

```bash
ls /dev/cu.*       # macOS
ls /dev/ttyUSB*    # Linux
```

The default in the Makefile is `/dev/cu.SLAB_USBtoUART` (common for NodeMCU on macOS). Override it on any `make` command if yours differs:

```bash
make wled-flash FLASH_PORT=/dev/ttyUSB0
```

---

## WLED + Home Assistant Integration

Flashes [WLED](https://kno.wled.ge) onto the stick and wires it into Home Assistant so it mirrors another light automatically.

### How it works

```
Hue bridge  →  Home Assistant  →  WLED (hockey stick)
  (event stream, ~0.3s)    (automation, ~0.1s)
```

State changes (on/off, color, brightness) on the source light propagate to the stick in roughly half a second.

### Flash WLED

```bash
make wled-flash
```

Downloads the WLED binary, flashes it, then provisions WiFi credentials from `.env` by temporarily joining the WLED access point.

### Add to Home Assistant

After flashing, the stick won't auto-discover in HA if it's on a separate VLAN. Add it manually:

**Settings → Integrations → + Add Integration → WLED → enter the stick's IP**

### Deploy the automation

Edit `home_assistant/automations.yaml` to set your source light entity (the light you want the stick to follow), then deploy:

```bash
make deploy-ha HA_TOKEN=your_long_lived_token
```

Create a long-lived token at: **HA → Profile → Long-Lived Access Tokens**

The included automation mirrors color and brightness. If your source light only supports on/off (e.g. a smart plug), simplify the action to just `light.turn_{{ trigger.to_state.state }}`.

---

## Original NHL Firmware

Monitors the NHL API for live games and reacts automatically.

### Features

- **Automatic mode**: displays team colors during a live game, flashes on goals
- **Manual mode**: on/off, wave, flash animations, custom 3-color palette
- **Brightness control**: adjustable (1–255)
- **Web UI**: served from the ESP8266 at its local IP

### Build and flash

```bash
make build    # generates build/hockey_stick.ino
```

Then open `build/hockey_stick.ino` in Arduino IDE and upload to the ESP8266.

### API endpoints

All accept GET (POST also works):

| Endpoint | Description |
|---|---|
| `/lighton` | Turn on (team colors) |
| `/lightoff` | Turn off |
| `/lightflash` | Flash animation |
| `/lightwave` | Wave animation |
| `/lightauto` | Return to automatic mode |
| `/setteam?team=TOR` | Set team (3-letter abbreviation) |
| `/setbrightness?brightness=178` | Set brightness (1–255) |
| `/manualonteams` | Manual ON with team colors |
| `/manualoncustom` | Manual ON with custom colors |
| `/useTeamColors` | Use team colors for manual palette |
| `/useCustomColors` | Use custom colors for manual palette |
| `/setmanualcolors?c1=%23FF0000&c2=%2300FF00&c3=%230000FF` | Set custom colors (hex) |
| `/wave/on/` | Start red/black wave |
| `/wave/off/` | Stop red/black wave |

### Arduino dependencies

Install via Library Manager:

- ESP8266WiFi
- ESP8266WebServer
- ESP8266HTTPClient
- WiFiClientSecure
- ArduinoJson
- Adafruit NeoPixel
- EEPROM

---

## Project Structure

```
rgb_hockey_stick/
├── templates/
│   └── hockey_stick.ino        # Original firmware template
├── web/
│   └── ui.html                 # Web UI for original firmware
├── tools/
│   ├── build_web_ui.py         # Build script for original firmware
│   ├── wled_connect.sh         # Provisions WiFi credentials to WLED via AP
│   └── wled_provision.py       # Improv Wi-Fi Serial provisioner (fallback)
├── home_assistant/
│   ├── automations.yaml        # HA automation to mirror a light to the stick
│   └── wled_data/
│       └── cfg.json            # WLED config template (GPIO, LED count, hostname)
├── rainbow_test/               # Minimal rainbow sketch for hardware testing
│   ├── rainbow_test.ino
│   ├── platformio.ini
│   └── src/main.cpp
├── build/                      # Generated output (gitignored)
├── .env                        # WiFi credentials (gitignored)
├── .env.example
├── Makefile
└── README.md
```

### Make targets

| Target | Description |
|---|---|
| `make build` | Build original firmware into `build/hockey_stick.ino` |
| `make flash` | Flash rainbow test sketch (hardware check) |
| `make wled-flash` | Flash WLED firmware and provision WiFi |
| `make wled-provision` | Re-provision WiFi without reflashing |
| `make deploy-ha HA_TOKEN=...` | Deploy HA automation |
| `make clean` | Remove build artifacts |

---

Created by [Ryder Calm Down](https://rydercalmdown.com)
