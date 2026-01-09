#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>
#include "web_ui.generated.h"

// ================== CONFIG ==================
#define WIFI_SSID     "{{WIFI_SSID}}"
#define WIFI_PASSWORD "{{WIFI_PASSWORD}}"

#define LED_PIN       4   // GPIO 4 (D2 on NodeMCU/ESP8266)
#define LED_COUNT     100

// Default team (will be overridden by saved value or web interface)
#define DEFAULT_TEAM   "TOR"   // Toronto Maple Leafs
#define POLL_INTERVAL 15000

// EEPROM addresses
#define EEPROM_SIZE   64
#define EEPROM_TEAM   0  // Stores 3-char team abbreviation + null terminator
#define EEPROM_BRIGHTNESS 4  // Stores brightness (0-255)
#define EEPROM_MANUAL_VALID 5          // 0xA5 when manual settings are valid
#define EEPROM_MANUAL_USE_CUSTOM 6     // 0/1
#define EEPROM_MANUAL_COLORS 8         // 9 bytes: c1 RGB, c2 RGB, c3 RGB
#define DEFAULT_BRIGHTNESS 178  // 70% of 255

// ============================================

// Team color structure (up to 4 colors)
struct TeamColors {
  const char* abbreviation;
  uint8_t colorCount;
  uint8_t colors[4][3];  // RGB values for up to 4 colors
};

// NHL Teams with their logo colors (RGB values)
const TeamColors NHL_TEAMS[] = {
  {"ANA", 4, {{252,76,2},{185,151,91},{193,198,200},{0,0,0}}},               // Ducks
  {"ARI", 3, {{140, 38, 51}, {226, 214, 181}, {0, 0, 0}, {0, 0, 0}}},       // Arizona Coyotes
  {"BOS", 2, {{252, 181, 20}, {17, 17, 17}, {0, 0, 0}, {0, 0, 0}}},         // Boston Bruins
  {"BUF", 3, {{0, 38, 84}, {252, 181, 20}, {255, 255, 255}, {0, 0, 0}}},    // Buffalo Sabres
  {"CGY", 3, {{200, 16, 46}, {255, 184, 28}, {255, 255, 255}, {0, 0, 0}}},  // Calgary Flames
  {"CAR", 4, {{204,0,0},{0,0,0},{162,169,175},{255,255,255}}},              // Hurricanes
  {"CHI", 4, {{207,10,44},{0,0,0},{255,209,0},{255,255,255}}},              // Blackhawks
  {"COL", 3, {{111, 38, 61}, {25, 130, 196}, {255, 255, 255}, {0, 0, 0}}},  // Colorado Avalanche
  {"CBJ", 3, {{0, 38, 84}, {200, 16, 46}, {255, 255, 255}, {0, 0, 0}}},     // Columbus Blue Jackets
  {"DAL", 3, {{0, 104, 71}, {162, 170, 173}, {255, 255, 255}, {0, 0, 0}}},  // Dallas Stars
  {"DET", 2, {{200, 16, 46}, {255, 255, 255}, {0, 0, 0}, {0, 0, 0}}},       // Detroit Red Wings
  {"EDM", 2, {{0, 32, 91}, {252, 76, 2}, {0, 0, 0}, {0, 0, 0}}},            // Edmonton Oilers
  {"FLA", 3, {{200, 16, 46}, {4, 30, 66}, {255, 255, 255}, {0, 0, 0}}},     // Florida Panthers
  {"LAK", 3, {{17, 17, 17}, {162, 170, 173}, {255, 255, 255}, {0, 0, 0}}}, // Los Angeles Kings
  {"MIN", 4, {{21,71,52},{166,25,46},{234,170,0},{255,255,255}}},            // Wild
  {"MTL", 3, {{175, 30, 45}, {25, 33, 104}, {255, 255, 255}, {0, 0, 0}}},   // Montreal Canadiens
  {"NSH", 2, {{255, 184, 28}, {4, 30, 66}, {0, 0, 0}, {0, 0, 0}}},          // Nashville Predators
  {"NJD", 3, {{200, 16, 46}, {255, 255, 255}, {0, 0, 0}, {0, 0, 0}}},       // New Jersey Devils
  {"NYI", 4, {{0, 83, 155}, {244, 125, 48}, {255, 255, 255}, {0, 0, 0}}},   // New York Islanders
  {"NYR", 3, {{0, 56, 168}, {200, 16, 46}, {255, 255, 255}, {0, 0, 0}}},    // New York Rangers
  {"OTT", 3, {{200, 16, 46}, {185, 151, 91}, {255, 255, 255}, {0, 0, 0}}},  // Ottawa Senators
  {"PHI", 3, {{247, 73, 2}, {0, 0, 0}, {255, 255, 255}, {0, 0, 0}}},        // Philadelphia Flyers
  {"PIT", 3, {{0, 0, 0}, {252, 181, 20}, {255, 255, 255}, {0, 0, 0}}},      // Pittsburgh Penguins
  {"SJS", 3, {{0, 109, 117}, {234, 218, 184}, {0, 0, 0}, {0, 0, 0}}},       // San Jose Sharks
  {"SEA", 4, {{0,22,40},{99,113,122},{180,215,230},{215,40,60}}},            // Kraken
  {"STL", 3, {{0, 47, 108}, {252, 181, 20}, {255, 255, 255}, {0, 0, 0}}},   // St. Louis Blues
  {"TBL", 3, {{0,40,104},{255,255,255},{255,209,0}}},                       // Lightning
  {"TOR", 2, {{0, 32, 128}, {255, 255, 255}, {0, 0, 0}, {0, 0, 0}}},        // Toronto Maple Leafs
  {"VAN", 3, {{0, 32, 91}, {4, 28, 44}, {255, 255, 255}, {0, 0, 0}}},       // Vancouver Canucks
  {"VGK", 3, {{185, 151, 91}, {51, 51, 51}, {200, 16, 46}, {0, 0, 0}}},     // Vegas Golden Knights
  {"WSH", 3, {{200, 16, 46}, {4, 30, 66}, {255, 255, 255}, {0, 0, 0}}},     // Washington Capitals
  {"WPG", 3, {{4, 30, 66}, {0, 83, 155}, {255, 255, 255}, {0, 0, 0}}}       // Winnipeg Jets
};

const int NUM_TEAMS = 32;

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
ESP8266WebServer server(80);

// Current selected team (loaded from EEPROM or default)
char selectedTeam[4] = DEFAULT_TEAM;
uint8_t masterBrightness = DEFAULT_BRIGHTNESS;  // 0-255, default 70%

unsigned long lastPoll = 0;
unsigned long goalFlashUntil = 0;

int lastKnownGoals = -1;
bool gameIsLive = false;

// Manual override states
enum ManualMode {
  MANUAL_NONE,    // Use automatic API-based control
  MANUAL_ON,      // Force lights on
  MANUAL_OFF,     // Force lights off
  MANUAL_FLASH,   // Force flash animation
  MANUAL_WAVE     // Wave animation mode
};
ManualMode manualMode = MANUAL_NONE;
unsigned long manualFlashUntil = 0;

// Wave animation state
float wavePosition = 0.0;  // Current wave position (0 to LED_COUNT)
unsigned long lastWaveUpdate = 0;

// Red/black wave state
bool redBlackWaveActive = false;

// Manual custom palette (3 colors)
bool manualUseCustomColors = false;
uint8_t manualColors[3][3] = {{255, 0, 0}, {0, 255, 0}, {0, 128, 255}}; // default RGB

// Current team colors (set in setup)
uint32_t teamColor1;
uint32_t teamColor2;
uint32_t teamColor3;
uint32_t teamColor4;
int teamColorCount = 0;

// Forward declarations
uint32_t blendColors(uint32_t color1, uint32_t color2, float ratio);
void saveManualSettingsToEEPROM();
void loadManualSettingsFromEEPROM();
bool parseHexColor(const String& input, uint8_t* r, uint8_t* g, uint8_t* b);
String rgbToHex(uint8_t r, uint8_t g, uint8_t b);
void setSoftGlow(uint32_t c1, uint32_t c2);
void getActiveManualPalette(uint32_t* colorsOut, int* countOut);
void setRedBlackWave();
String buildTeamOptionsHTML();

// ------------------------------------------------

void saveTeamToEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < 4; i++) {
    EEPROM.write(EEPROM_TEAM + i, selectedTeam[i]);
  }
  EEPROM.commit();
  EEPROM.end();
}

// ------------------------------------------------

void saveBrightnessToEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(EEPROM_BRIGHTNESS, masterBrightness);
  EEPROM.commit();
  EEPROM.end();
}

// ------------------------------------------------

void saveManualSettingsToEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(EEPROM_MANUAL_VALID, 0xA5);
  EEPROM.write(EEPROM_MANUAL_USE_CUSTOM, manualUseCustomColors ? 1 : 0);

  int addr = EEPROM_MANUAL_COLORS;
  for (int i = 0; i < 3; i++) {
    EEPROM.write(addr++, manualColors[i][0]);
    EEPROM.write(addr++, manualColors[i][1]);
    EEPROM.write(addr++, manualColors[i][2]);
  }

  EEPROM.commit();
  EEPROM.end();
}

// ------------------------------------------------

void loadManualSettingsFromEEPROM() {
  EEPROM.begin(EEPROM_SIZE);

  uint8_t valid = EEPROM.read(EEPROM_MANUAL_VALID);
  if (valid == 0xA5) {
    manualUseCustomColors = EEPROM.read(EEPROM_MANUAL_USE_CUSTOM) == 1;

    int addr = EEPROM_MANUAL_COLORS;
    for (int i = 0; i < 3; i++) {
      manualColors[i][0] = EEPROM.read(addr++);
      manualColors[i][1] = EEPROM.read(addr++);
      manualColors[i][2] = EEPROM.read(addr++);
    }
  }

  EEPROM.end();
}

// ------------------------------------------------

void loadTeamFromEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  bool valid = true;
  char loaded[4];
  for (int i = 0; i < 3; i++) {
    loaded[i] = EEPROM.read(EEPROM_TEAM + i);
    if (loaded[i] < 'A' || loaded[i] > 'Z') {
      valid = false;
      break;
    }
  }
  loaded[3] = '\0';
  
  if (valid) {
    // Verify it's a valid team abbreviation
    for (int i = 0; i < NUM_TEAMS; i++) {
      if (strcmp(NHL_TEAMS[i].abbreviation, loaded) == 0) {
        strcpy(selectedTeam, loaded);
        EEPROM.end();
        return;
      }
    }
  }
  // If invalid, use default
  strcpy(selectedTeam, DEFAULT_TEAM);
  EEPROM.end();
}

// ------------------------------------------------

void loadBrightnessFromEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  uint8_t loaded = EEPROM.read(EEPROM_BRIGHTNESS);
  if (loaded > 0 && loaded <= 255) {
    masterBrightness = loaded;
  } else {
    masterBrightness = DEFAULT_BRIGHTNESS;
  }
  EEPROM.end();
  strip.setBrightness(masterBrightness);
}

// ------------------------------------------------

bool parseHexColor(const String& input, uint8_t* r, uint8_t* g, uint8_t* b) {
  String s = input;
  s.trim();
  if (s.length() == 0) return false;
  if (s[0] == '#') s = s.substring(1);
  if (s.length() != 6) return false;

  char buf[7];
  s.toCharArray(buf, sizeof(buf));
  char* end = nullptr;
  long value = strtol(buf, &end, 16);
  if (end == buf || *end != '\0') return false;

  *r = (value >> 16) & 0xFF;
  *g = (value >> 8) & 0xFF;
  *b = value & 0xFF;
  return true;
}

String rgbToHex(uint8_t r, uint8_t g, uint8_t b) {
  char out[8];
  snprintf(out, sizeof(out), "#%02X%02X%02X", r, g, b);
  return String(out);
}

void setSoftGlow(uint32_t c1, uint32_t c2) {
  for (int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, (i < LED_COUNT / 2) ? c1 : c2);
  }
  strip.show();
}

void getActiveManualPalette(uint32_t* colorsOut, int* countOut) {
  if (manualUseCustomColors) {
    *countOut = 3;
    colorsOut[0] = strip.Color(manualColors[0][0], manualColors[0][1], manualColors[0][2]);
    colorsOut[1] = strip.Color(manualColors[1][0], manualColors[1][1], manualColors[1][2]);
    colorsOut[2] = strip.Color(manualColors[2][0], manualColors[2][1], manualColors[2][2]);
    colorsOut[3] = 0;
    return;
  }

  *countOut = teamColorCount;
  colorsOut[0] = teamColor1;
  colorsOut[1] = teamColor2;
  colorsOut[2] = teamColor3;
  colorsOut[3] = teamColor4;
}

// ------------------------------------------------

void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

// ------------------------------------------------

void setupTeamColors() {
  for (int i = 0; i < NUM_TEAMS; i++) {
    if (strcmp(NHL_TEAMS[i].abbreviation, selectedTeam) == 0) {
      teamColorCount = NHL_TEAMS[i].colorCount;
      teamColor1 = strip.Color(NHL_TEAMS[i].colors[0][0], NHL_TEAMS[i].colors[0][1], NHL_TEAMS[i].colors[0][2]);
      teamColor2 = strip.Color(NHL_TEAMS[i].colors[1][0], NHL_TEAMS[i].colors[1][1], NHL_TEAMS[i].colors[1][2]);
      if (teamColorCount >= 3) {
        teamColor3 = strip.Color(NHL_TEAMS[i].colors[2][0], NHL_TEAMS[i].colors[2][1], NHL_TEAMS[i].colors[2][2]);
      }
      if (teamColorCount >= 4) {
        teamColor4 = strip.Color(NHL_TEAMS[i].colors[3][0], NHL_TEAMS[i].colors[3][1], NHL_TEAMS[i].colors[3][2]);
      }
      return;
    }
  }
  // Default to first two colors if team not found
  teamColorCount = 2;
  teamColor1 = strip.Color(128, 128, 128);
  teamColor2 = strip.Color(255, 255, 255);
}

// ------------------------------------------------

void setSoftGameGlow() {
  setSoftGlow(teamColor1, teamColor2);
}

// ------------------------------------------------

void flashGoal() {
  static bool state = false;
  state = !state;

  // Alternate between first two colors
  uint32_t color = state ? teamColor1 : teamColor2;
  for (int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, color);
  }
  strip.show();
}

// ------------------------------------------------

void flashManual() {
  static int idx = 0;
  uint32_t colors[4] = {0, 0, 0, 0};
  int count = 0;
  getActiveManualPalette(colors, &count);
  if (count <= 0) return;

  idx = (idx + 1) % count;
  uint32_t color = colors[idx];
  for (int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, color);
  }
  strip.show();
}

// ------------------------------------------------

void clearLEDs() {
  strip.clear();
  strip.show();
}

// ------------------------------------------------

void setWaveAnimation() {
  unsigned long now = millis();
  
  // Calculate wave speed: 0.3 m/s on standard LED strip (60 LEDs/meter) = 18 LEDs/second
  // With 100 LEDs, that's about 5.5 seconds per full cycle
  // Update every ~50ms for smooth animation
  if (now - lastWaveUpdate > 50) {
    lastWaveUpdate = now;
    
    // Move wave position forward (18 LEDs per second = 0.9 LEDs per 50ms update)
    wavePosition += 0.9;
    if (wavePosition >= LED_COUNT) {
      wavePosition = 0.0;
    }
  }
  
  // Create wave pattern using active manual palette (team or custom)
  uint32_t colors[4] = {0, 0, 0, 0};
  int colorCount = 0;
  getActiveManualPalette(colors, &colorCount);
  if (colorCount <= 0) return;

  // Use a sine wave to blend between colors smoothly
  for (int i = 0; i < LED_COUNT; i++) {
    float pos = (float)i - wavePosition;
    if (pos < 0) pos += LED_COUNT;
    
    // Create wave pattern - cycle through team colors
    float wave = (sin(pos * 2.0 * PI / (LED_COUNT / colorCount)) + 1.0) / 2.0;
    
    uint32_t color;
    if (colorCount == 2) {
      // Blend between color 0 and 1
      color = blendColors(colors[0], colors[1], wave);
    } else if (colorCount >= 3) {
      // Cycle through all colors
      int colorIndex = (int)(pos / (LED_COUNT / colorCount)) % colorCount;
      int nextColorIndex = (colorIndex + 1) % colorCount;
      float blend = fmod(pos / (LED_COUNT / colorCount), 1.0);
      
      uint32_t c1 = colors[colorIndex];
      uint32_t c2 = colors[nextColorIndex];
      color = blendColors(c1, c2, blend);
    } else {
      color = colors[0];
    }
    
    strip.setPixelColor(i, color);
  }
  strip.show();
}

// ------------------------------------------------

void setRedBlackWave() {
  unsigned long now = millis();
  
  // Update wave position every ~50ms for smooth animation
  if (now - lastWaveUpdate > 50) {
    lastWaveUpdate = now;
    
    // Move wave position forward (18 LEDs per second = 0.9 LEDs per 50ms update)
    wavePosition += 0.9;
    if (wavePosition >= LED_COUNT) {
      wavePosition = 0.0;
    }
  }
  
  // Red and black colors
  uint32_t red = strip.Color(255, 0, 0);
  uint32_t black = strip.Color(0, 0, 0);
  
  // Create wave pattern alternating red and black
  for (int i = 0; i < LED_COUNT; i++) {
    float pos = (float)i - wavePosition;
    if (pos < 0) pos += LED_COUNT;
    
    // Create wave pattern using sine wave (0 to 1)
    float wave = (sin(pos * 2.0 * PI / (LED_COUNT / 2.0)) + 1.0) / 2.0;
    
    // Blend between red and black
    uint32_t color = blendColors(red, black, wave);
    strip.setPixelColor(i, color);
  }
  strip.show();
}

// ------------------------------------------------

uint32_t blendColors(uint32_t color1, uint32_t color2, float ratio) {
  uint8_t r1 = (uint8_t)(color1 >> 16);
  uint8_t g1 = (uint8_t)(color1 >> 8);
  uint8_t b1 = (uint8_t)color1;
  
  uint8_t r2 = (uint8_t)(color2 >> 16);
  uint8_t g2 = (uint8_t)(color2 >> 8);
  uint8_t b2 = (uint8_t)color2;
  
  uint8_t r = (uint8_t)(r1 + (r2 - r1) * ratio);
  uint8_t g = (uint8_t)(g1 + (g2 - g1) * ratio);
  uint8_t b = (uint8_t)(b1 + (b2 - b1) * ratio);
  
  return strip.Color(r, g, b);
}

// ------------------------------------------------

String getHTML() {
  String html = FPSTR(WEB_UI_HTML);
  html.reserve(9000);

  html.replace("{{IP}}", WiFi.localIP().toString());
  html.replace("{{MODE_LABEL}}", (manualMode == MANUAL_NONE) ? "Automatic" : "Manual");
  html.replace("{{SELECTED_TEAM}}", String(selectedTeam));
  html.replace("{{TEAM_OPTIONS}}", buildTeamOptionsHTML());
  html.replace("{{BRIGHTNESS}}", String(masterBrightness));
  html.replace("{{BRIGHTNESS_PERCENT}}", String((masterBrightness * 100) / 255));

  // This placeholder is used in JS and must be an unquoted boolean literal: true/false
  html.replace("{{MANUAL_USE_CUSTOM}}", manualUseCustomColors ? "true" : "false");
  html.replace("{{C1}}", rgbToHex(manualColors[0][0], manualColors[0][1], manualColors[0][2]));
  html.replace("{{C2}}", rgbToHex(manualColors[1][0], manualColors[1][1], manualColors[1][2]));
  html.replace("{{C3}}", rgbToHex(manualColors[2][0], manualColors[2][1], manualColors[2][2]));

  return html;
}

String buildTeamOptionsHTML() {
  String options;
  options.reserve(1300);
  for (int i = 0; i < NUM_TEAMS; i++) {
    const char* abbr = NHL_TEAMS[i].abbreviation;
    options += "<option value='";
    options += abbr;
    options += "'";
    if (strcmp(abbr, selectedTeam) == 0) {
      options += " selected";
    }
    options += ">";
    options += abbr;
    options += "</option>";
  }
  return options;
}

// ------------------------------------------------

void handleRoot() {
  server.send(200, "text/html", getHTML());
}

// ------------------------------------------------

void handleSetTeam() {
  if (server.hasArg("team")) {
    String newTeam = server.arg("team");
    newTeam.toUpperCase();
    newTeam.trim();
    
    // Validate team
    bool valid = false;
    for (int i = 0; i < NUM_TEAMS; i++) {
      if (strcmp(NHL_TEAMS[i].abbreviation, newTeam.c_str()) == 0) {
        valid = true;
        break;
      }
    }
    
    if (valid && newTeam.length() == 3) {
      strcpy(selectedTeam, newTeam.c_str());
      saveTeamToEEPROM();
      setupTeamColors();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Invalid team abbreviation");
    }
  } else {
    server.send(400, "text/plain", "No team specified");
  }
}

// ------------------------------------------------

void handleLightOn() {
  manualMode = MANUAL_ON;
  server.send(200, "text/plain", "OK");
}

// ------------------------------------------------

void handleLightOff() {
  manualMode = MANUAL_OFF;
  server.send(200, "text/plain", "OK");
}

// ------------------------------------------------

void handleLightFlash() {
  manualMode = MANUAL_FLASH;
  manualFlashUntil = millis() + 10000; // 10 seconds
  server.send(200, "text/plain", "OK");
}

// ------------------------------------------------

void handleLightAuto() {
  manualMode = MANUAL_NONE;
  server.send(200, "text/plain", "OK");
}

// ------------------------------------------------

void handleSetBrightness() {
  if (server.hasArg("brightness")) {
    int brightness = server.arg("brightness").toInt();
    if (brightness >= 1 && brightness <= 255) {
      masterBrightness = brightness;
      strip.setBrightness(masterBrightness);
      saveBrightnessToEEPROM();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Invalid brightness");
    }
  } else {
    server.send(400, "text/plain", "No brightness specified");
  }
}

// ------------------------------------------------

void handleLightWave() {
  manualMode = MANUAL_WAVE;
  wavePosition = 0.0;  // Reset wave position
  server.send(200, "text/plain", "OK");
}

// ------------------------------------------------

void handleManualOnTeam() {
  manualUseCustomColors = false;
  saveManualSettingsToEEPROM();
  manualMode = MANUAL_ON;
  server.send(200, "text/plain", "OK");
}

void handleManualOnCustom() {
  manualUseCustomColors = true;
  saveManualSettingsToEEPROM();
  manualMode = MANUAL_ON;
  server.send(200, "text/plain", "OK");
}

void handleUseTeamColors() {
  manualUseCustomColors = false;
  saveManualSettingsToEEPROM();
  server.send(200, "text/plain", "OK");
}

void handleUseCustomColors() {
  manualUseCustomColors = true;
  saveManualSettingsToEEPROM();
  server.send(200, "text/plain", "OK");
}

void handleWaveOn() {
  redBlackWaveActive = true;
  wavePosition = 0.0;  // Reset wave position
  server.send(200, "text/plain", "OK");
}

// ------------------------------------------------

void handleWaveOff() {
  redBlackWaveActive = false;
  server.send(200, "text/plain", "OK");
}

// ------------------------------------------------

void handleSetManualColors() {
  if (!server.hasArg("c1") || !server.hasArg("c2") || !server.hasArg("c3")) {
    server.send(400, "text/plain", "Missing color args (c1,c2,c3)");
    return;
  }

  uint8_t r, g, b;

  if (!parseHexColor(server.arg("c1"), &r, &g, &b)) {
    server.send(400, "text/plain", "Invalid c1");
    return;
  }
  manualColors[0][0] = r; manualColors[0][1] = g; manualColors[0][2] = b;

  if (!parseHexColor(server.arg("c2"), &r, &g, &b)) {
    server.send(400, "text/plain", "Invalid c2");
    return;
  }
  manualColors[1][0] = r; manualColors[1][1] = g; manualColors[1][2] = b;

  if (!parseHexColor(server.arg("c3"), &r, &g, &b)) {
    server.send(400, "text/plain", "Invalid c3");
    return;
  }
  manualColors[2][0] = r; manualColors[2][1] = g; manualColors[2][2] = b;

  manualUseCustomColors = true;
  saveManualSettingsToEEPROM();
  server.send(200, "text/plain", "OK");
}

// ------------------------------------------------

void handleAPI() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure(); // Skip certificate validation for Heroku
  HTTPClient http;
  http.begin(client, "https://nhl-score-api.herokuapp.com/api/scores/latest");

  int httpCode = http.GET();
  if (httpCode != 200) {
    http.end();
    return;
  }

  String payload = http.getString();
  http.end();

  StaticJsonDocument<25000> doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) return;

  JsonArray games = doc["games"];
  bool foundGame = false;

  for (JsonObject game : games) {
    const char* home = game["teams"]["home"]["abbreviation"];
    const char* away = game["teams"]["away"]["abbreviation"];

    if (strcmp(home, selectedTeam) == 0 || strcmp(away, selectedTeam) == 0) {
      foundGame = true;

      const char* state = game["status"]["state"];
      gameIsLive = strcmp(state, "LIVE") == 0;

      int goals = game["scores"][selectedTeam];

      if (lastKnownGoals == -1) {
        lastKnownGoals = goals;
      } else if (goals > lastKnownGoals) {
        goalFlashUntil = millis() + 10000; // 10 seconds
        lastKnownGoals = goals;
      }

      break;
    }
  }

  if (!foundGame) {
    gameIsLive = false;
    lastKnownGoals = -1;
  }
}

// ------------------------------------------------

void setup() {
  Serial.begin(9600);
  strip.begin();
  strip.show();

  // Load saved team and brightness from EEPROM
  loadTeamFromEEPROM();
  loadBrightnessFromEEPROM();
  setupTeamColors();
  loadManualSettingsFromEEPROM();

  // Connect to WiFi
  connectWiFi();
  
  // Print IP address
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/setteam", handleSetTeam);
  server.on("/setbrightness", handleSetBrightness);
  server.on("/lighton", handleLightOn);
  server.on("/lightoff", handleLightOff);
  server.on("/lightflash", handleLightFlash);
  server.on("/lightwave", handleLightWave);
  server.on("/lightauto", handleLightAuto);
  server.on("/manualonteams", handleManualOnTeam);
  server.on("/manualoncustom", handleManualOnCustom);
  server.on("/useTeamColors", handleUseTeamColors);
  server.on("/useCustomColors", handleUseCustomColors);
  server.on("/setmanualcolors", handleSetManualColors);
  server.on("/wave/on/", handleWaveOn);
  server.on("/wave/off/", handleWaveOff);
  
  server.begin();
  Serial.println("Web server started");
}

// ------------------------------------------------

void loop() {
  unsigned long now = millis();
  
  // Handle web server requests
  server.handleClient();

  // Red/black wave takes precedence over everything
  if (redBlackWaveActive) {
    setRedBlackWave();
    delay(50);
    return;
  }

  // Handle manual overrides first
  if (manualMode == MANUAL_ON) {
    if (manualUseCustomColors) {
      uint32_t c1 = strip.Color(manualColors[0][0], manualColors[0][1], manualColors[0][2]);
      uint32_t c2 = strip.Color(manualColors[1][0], manualColors[1][1], manualColors[1][2]);
      setSoftGlow(c1, c2);
    } else {
      setSoftGameGlow();
    }
    delay(500);
    return;
  } else if (manualMode == MANUAL_OFF) {
    clearLEDs();
    delay(500);
    return;
  } else if (manualMode == MANUAL_FLASH) {
    if (now < manualFlashUntil) {
      flashManual();
      delay(250);
      return;
    } else {
      // Flash period ended, return to auto mode
      manualMode = MANUAL_NONE;
    }
  } else if (manualMode == MANUAL_WAVE) {
    setWaveAnimation();
    delay(50);  // Small delay for smooth animation
    return;
  }

  // Automatic mode (MANUAL_NONE)
  // Poll API
  if (now - lastPoll > POLL_INTERVAL) {
    lastPoll = now;
    handleAPI();
  }

  // Goal animation overrides everything
  if (now < goalFlashUntil) {
    flashGoal();
    delay(250);
    return;
  }

  // Game live glow
  if (gameIsLive) {
    setSoftGameGlow();
  } else {
    clearLEDs();
  }

  delay(500);
}
