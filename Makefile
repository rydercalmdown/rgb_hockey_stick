.PHONY: build clean

WEB_SRC := web/ui.html
BUILD_DIR := build
BUILD_INO := $(BUILD_DIR)/hockey_stick.ino
WEB_TOOL := tools/build_web_ui.py
INO_SRC := templates/hockey_stick.ino

build: $(BUILD_INO)

$(BUILD_INO): $(INO_SRC) $(WEB_SRC) $(WEB_TOOL)
	@mkdir -p $(BUILD_DIR)
	python3 $(WEB_TOOL) --html $(WEB_SRC) --ino $(INO_SRC) --out $(BUILD_INO) --env .env

clean:
	rm -rf $(BUILD_DIR)