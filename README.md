## Hockey Stick (ESP8266)

An ESP8266-based LED controller for a hockey stick that automatically responds to NHL game events and provides manual control modes.

### Features

- **Automatic Mode**: Monitors NHL API for your team's games and automatically:
  - Displays team colors when a game is live
  - Flashes team colors when your team scores a goal
- **Manual Mode**: Full control over LED patterns:
  - Turn lights on/off with team colors or custom colors
  - Wave animations (team colors or custom 3-color palette)
  - Flash animations
  - Custom color picker (3 colors)
- **Red/Black Wave**: Special endpoint for red/black wave animation
- **Brightness Control**: Adjustable master brightness (1-255)

### Hardware

- ESP8266 (NodeMCU or similar)
- WS2812B/NeoPixel LED strip (100 LEDs)
- LED strip connected to GPIO 4 (D2 on NodeMCU)

### Configuration

1. Copy `.env.example` to `.env`:
   ```bash
   cp .env.example .env
   ```

2. Edit `.env` and set your WiFi credentials:
   ```
   WIFI_SSID=YourWiFiNetworkName
   WIFI_PASSWORD=YourWiFiPassword
   ```

The `.env` file is gitignored, so your credentials won't be committed to the repository.

### Building

1. **Configure WiFi** (see Configuration above) - ensure `.env` exists with your credentials
2. **Edit the UI** (optional): modify `web/ui.html` to customize the web interface
3. **Edit the template** (optional): modify `templates/hockey_stick.ino` to change the Arduino code
4. **Build**: run `make build` to generate `build/hockey_stick.ino`
   - The build process reads `.env` and injects WiFi credentials into the template
   - Inlines the HTML from `web/ui.html` as a PROGMEM string
   - Outputs a single self-contained `build/hockey_stick.ino` file
5. **Upload**: drag `build/hockey_stick.ino` into Arduino IDE and upload to your ESP8266

The build process:
- Reads `web/ui.html` and `templates/hockey_stick.ino`
- Replaces `{{WIFI_SSID}}` and `{{WIFI_PASSWORD}}` placeholders from `.env`
- Inlines the HTML as a PROGMEM string (replaces `#include "web_ui.generated.h"`)
- Outputs a single self-contained `build/hockey_stick.ino` file

### Web Interface

The ESP8266 serves a web interface at its IP address (printed to Serial on startup).

**Two Modes:**

1. **Automatic Control**: Set your team, brightness, and enable automatic mode. The stick will monitor NHL games and react automatically.

2. **Manual Control**: 
   - Control lights directly (On/Off/Flash/Wave)
   - Choose between team colors or custom colors
   - Set 3 custom colors for manual animations
   - Wave and flash animations use the selected palette

**Styling**: Uses Bootstrap 5 (loaded via CDN) with toast notifications instead of blocking alerts.

### API Endpoints

All endpoints accept GET requests (POST also works):

- `/wave/on/` - Start red/black wave animation
- `/wave/off/` - Stop red/black wave animation
- `/lighton` - Turn lights on (team colors)
- `/lightoff` - Turn lights off
- `/lightflash` - Start flash animation
- `/lightwave` - Start wave animation
- `/lightauto` - Return to automatic mode
- `/setteam?team=TOR` - Set team (3-letter abbreviation)
- `/setbrightness?brightness=178` - Set brightness (1-255)
- `/manualonteams` - Manual ON with team colors
- `/manualoncustom` - Manual ON with custom colors
- `/useTeamColors` - Use team colors for manual palette
- `/useCustomColors` - Use custom colors for manual palette
- `/setmanualcolors?c1=%23FF0000&c2=%2300FF00&c3=%230000FF` - Set custom colors (hex format)

### Project Structure

```
hockey_stick/
├── templates/
│   └── hockey_stick.ino  # Template source file (with {{WIFI_SSID}} placeholders)
├── web/
│   └── ui.html           # Web UI source (HTML/CSS/JS)
├── tools/
│   └── build_web_ui.py   # Build script
├── build/                # Generated output (gitignored)
│   └── hockey_stick.ino # Ready-to-upload file
├── .env                  # WiFi credentials (gitignored, create from .env.example)
├── .env.example          # Example .env file
├── Makefile              # Build configuration
└── README.md             # This file
```

### Dependencies

Arduino libraries (install via Library Manager):
- ESP8266WiFi
- ESP8266WebServer
- ESP8266HTTPClient
- WiFiClientSecure
- ArduinoJson
- Adafruit NeoPixel
- EEPROM

### License

Created by Ryder Calm Down — [rydercalmdown.com](https://rydercalmdown.com)
