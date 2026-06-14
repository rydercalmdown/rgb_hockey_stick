HA_URL      := http://homeassistant.local:8123
HA_TOKEN    ?= $(shell grep HA_TOKEN .env 2>/dev/null | cut -d= -f2)
HA_AUTO_ID  := hockey_stick_follows_hue

WLED_VERSION := 16.0.0
WLED_BIN     := build/WLED_$(WLED_VERSION)_ESP8266.bin
WLED_URL     := https://github.com/wled/WLED/releases/download/v$(WLED_VERSION)/WLED_$(WLED_VERSION)_ESP8266.bin
WLED_FS_BIN  := build/wled_fs.bin
WLED_FS_ADDR := 0x300000
ESPTOOL      := ~/.platformio/packages/tool-esptoolpy/esptool.py
MKLITTLEFS   := ~/.platformio/packages/tool-mklittlefs/mklittlefs
FLASH_PORT   := /dev/cu.SLAB_USBtoUART

.PHONY: build clean flash wled-flash wled-provision deploy-ha

WEB_SRC := web/ui.html
BUILD_DIR := build
BUILD_INO := $(BUILD_DIR)/hockey_stick.ino
WEB_TOOL := tools/build_web_ui.py
INO_SRC := templates/hockey_stick.ino

build: $(BUILD_INO)

$(BUILD_INO): $(INO_SRC) $(WEB_SRC) $(WEB_TOOL)
	@mkdir -p $(BUILD_DIR)
	python3 $(WEB_TOOL) --html $(WEB_SRC) --ino $(INO_SRC) --out $(BUILD_INO) --env .env

flash:
	cd rainbow_test && pio run --target upload

$(WLED_BIN):
	@mkdir -p build
	curl -L -o $(WLED_BIN) $(WLED_URL)

$(WLED_FS_BIN): home_assistant/wled_data/cfg.json .env
	@mkdir -p build/wled_data
	python3 -c "\
import re, os, json; \
env = dict(l.strip().split('=',1) for l in open('.env') if '=' in l and not l.startswith('#')); \
cfg = open('home_assistant/wled_data/cfg.json').read(); \
cfg = cfg.replace('{{WIFI_SSID}}', env['WIFI_SSID']).replace('{{WIFI_PASSWORD}}', env['WIFI_PASSWORD']); \
open('build/wled_data/cfg.json','w').write(cfg)"
	$(MKLITTLEFS) -c build/wled_data -b 8192 -p 256 -s 0x100000 $(WLED_FS_BIN)

wled-flash: $(WLED_BIN)
	pip3 install pyserial -q
	python3 $(ESPTOOL) --port $(FLASH_PORT) --baud 460800 write_flash 0x0 $(WLED_BIN)
	python3 tools/wled_provision.py $(FLASH_PORT)
	pio device monitor --port $(FLASH_PORT) --baud 115200 --filter printable

wled-provision:
	bash tools/wled_connect.sh

deploy-ha:
	@if [ -z "$(HA_TOKEN)" ]; then echo "Set HA_TOKEN in .env or run: make deploy-ha HA_TOKEN=yourtoken"; exit 1; fi
	curl -s -X POST $(HA_URL)/api/config/automation/config/$(HA_AUTO_ID) \
	  -H "Authorization: Bearer $(HA_TOKEN)" \
	  -H "Content-Type: application/json" \
	  -d @home_assistant/automations.yaml && echo "deployed"

clean:
	rm -rf $(BUILD_DIR)