/*
 * cuteOS v2.0 — Complete Standalone Glassy Smart OS for ESP32-S3-N16R8 + 3.5" ILI9488 Touch
 * Wi-Fi     : Configured via secrets.h | Font: Times New Roman (FreeSerif)
 * Wallpaper : Real Mount Everest North Face Photo (OPI PSRAM) + Pre-Baked Liquid Glass
 * Features  : White Mountain Boot Logo, 5-Tile Live News w/ Real Wi-Fi Photos + Article Reader,
 *             Live Adjustable Screen Brightness, Real Local Estate Map (Street + Satellite),
 *             Live Local Weather & 3-Day Forecast, Wi-Fi RF Radar, ADC Oscilloscope, RGB & GPIO,
 *             Calculator, Focus Timer, Dynamic Island, Customizable Widgets, Saved Touch Calibration
 */

#include <LittleFS.h>
#include <LovyanGFX.hpp>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <time.h>
#include <math.h>
#include "PhoneLink.h"

#if __has_include("secrets.h")
  #include "secrets.h"
#else
  #include "secrets.example.h"
#endif

// ============================================================================
// 1. WI-FI, LOCATION & NEWS FEED CONFIGURATION
// ============================================================================
const char* WIFI_SSID = SECRET_WIFI_SSID;
const char* WIFI_PASS = SECRET_WIFI_PASS;

const double HOME_LAT  = SECRET_HOME_LAT;
const double HOME_LON  = SECRET_HOME_LON;
const char*  AREA_NAME = SECRET_AREA_NAME;
const char*  CITY_NAME = SECRET_CITY_NAME;

const int NUM_NEWS_TOPICS = 5;
const char* TOPIC_NAMES[NUM_NEWS_TOPICS] = {"Top", "India", "Tech", "Science", "Sport"};
const char* TOPIC_URLS[NUM_NEWS_TOPICS]  = {
  "https://feeds.bbci.co.uk/news/rss.xml",
  "https://feeds.bbci.co.uk/news/world/asia/india/rss.xml",
  "https://feeds.bbci.co.uk/news/technology/rss.xml",
  "https://feeds.bbci.co.uk/news/science_and_environment/rss.xml",
  "https://feeds.bbci.co.uk/sport/rss.xml"
};

// ============================================================================
// 2. VERIFIED HARDWARE PINOUT (ESP32-S3-N16R8 + 14-PIN RED PCB ILI9488)
// ============================================================================
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9488 _panel_instance;
  lgfx::Bus_SPI       _bus_instance;
  lgfx::Touch_XPT2046 _touch_instance;
public:
  LGFX(void) {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host    = SPI2_HOST;
      cfg.spi_mode    = 0;
      cfg.freq_write  = 20000000;
      cfg.freq_read   = 16000000;
      cfg.spi_3wire   = false;
      cfg.use_lock    = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk    = 40;
      cfg.pin_mosi    = 39;
      cfg.pin_miso    = 38;
      cfg.pin_dc      = 41;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs           = 1;
      cfg.pin_rst          = 3;
      cfg.pin_busy         = -1;
      cfg.panel_width      = 320;
      cfg.panel_height     = 480;
      cfg.offset_x         = 0;
      cfg.offset_y         = 0;
      cfg.offset_rotation  = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.readable         = false;
      cfg.invert           = false;
      cfg.rgb_order        = false;
      cfg.dlen_16bit       = false;
      cfg.bus_shared       = true;
      _panel_instance.config(cfg);
    }
    {
      auto cfg = _touch_instance.config();
      cfg.x_min           = 300;
      cfg.x_max           = 3900;
      cfg.y_min           = 200;
      cfg.y_max           = 3900;
      cfg.pin_int         = -1;
      cfg.bus_shared      = true;
      cfg.offset_rotation = 0;
      cfg.spi_host        = SPI2_HOST;
      cfg.freq            = 1500000;
      cfg.pin_sclk        = 40;
      cfg.pin_mosi        = 39;
      cfg.pin_miso        = 38;
      cfg.pin_cs          = 42;
      _touch_instance.config(cfg);
      _panel_instance.setTouch(&_touch_instance);
    }
    setPanel(&_panel_instance);
  }
};

LGFX tft;
LGFX_Sprite bgSprite(&tft);
LGFX_Sprite frostSprite(&tft);
LGFX_Sprite darkGlass(&tft);
LGFX_Sprite mapSprite(&tft);
LGFX_Sprite canvas(&tft);
Preferences prefs;

const int NUM_NEWS_ITEMS = 5;
LGFX_Sprite* newsImg[NUM_NEWS_ITEMS] = {NULL, NULL, NULL, NULL, NULL};

const int BACKLIGHT_PWM_PIN = 14; // Optional HW PWM if Pin 8 moved to GPIO 14 (SW brightness works on 3V3!)
const int ADC_SCOPE_PIN     = 4;
const int OUT_GPIOS[3]      = {5, 6, 7};
bool gpioStates[3]          = {false, false, false};

#define ONBOARD_RGB_PIN 48

void setOnboardRGB(uint8_t r, uint8_t g, uint8_t b) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  rgbLedWrite(ONBOARD_RGB_PIN, r, g, b);
#else
  neopixelWrite(ONBOARD_RGB_PIN, r, g, b);
#endif
}

// ============================================================================
// 3. COLOR PALETTE & SYSTEM CONSTANTS
// ============================================================================
uint16_t makeRgb(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

uint16_t blend565(uint16_t fg, uint16_t bg, uint8_t alpha) {
  uint16_t fr = (fg >> 11) & 0x1F, fg_ = (fg >> 5) & 0x3F, fb = fg & 0x1F;
  uint16_t br = (bg >> 11) & 0x1F, bg_ = (bg >> 5) & 0x3F, bb = bg & 0x1F;
  return (uint16_t)((((fr * alpha + br * (255 - alpha)) >> 8) << 11) |
                    (((fg_ * alpha + bg_ * (255 - alpha)) >> 8) << 5) |
                    ((fb * alpha + bb * (255 - alpha)) >> 8));
}

const uint16_t C_WHITE       = 0xFFFF;
const uint16_t C_SNOW        = 0xF7DF;
const uint16_t C_SOFT_GRAY   = 0xE75E;
const uint16_t C_BORDER      = 0xD6DC;
const uint16_t C_INK         = 0x18E4;
const uint16_t C_INK_SUB     = 0x5B2E;
const uint16_t C_DARK_PILL   = 0x10A4;

const uint16_t C_BLUE        = 0x13DF;
const uint16_t C_CYAN        = 0x0E7F;
const uint16_t C_GREEN       = 0x262B;
const uint16_t C_GOLD        = 0xFE21;
const uint16_t C_ORANGE      = 0xFBAA;
const uint16_t C_RED         = 0xF9CA;
const uint16_t C_ROSE        = 0xF96E;
const uint16_t C_VIOLET      = 0x9A1E;
const uint16_t C_TEAL        = 0x05D9;

const uint16_t C_TINT_BLUE   = 0xE77F;
const uint16_t C_TINT_GREEN  = 0xE7BC;
const uint16_t C_TINT_GOLD   = 0xFFBC;
const uint16_t C_TINT_ROSE   = 0xFF5E;
const uint16_t C_TINT_VIOLET = 0xF75F;

#define SCR_HOME     0
#define SCR_NEWS     1   // Swipe Left -> Right : 5-Tile Live News w/ Real Photos
#define SCR_ARTICLE  2   // Full News Article Reader
#define SCR_CONTROL  3   // Swipe Down          : Brightness Slider & Quick Tiles
#define SCR_APPS     4   // Swipe Up            : Apple Squircle App Drawer
#define SCR_TILES    5   // Swipe Right -> Left : 4 Tech Tiles & Notifications
#define SCR_WIDGETS  6   // + Widgets Customizer
#define APP_MAP      7
#define APP_WEATHER  8
#define APP_RADAR    9
#define APP_SCOPE    10
#define APP_RGB      11
#define APP_CALC     12
#define APP_TIMER    13
#define SCR_PHONE    14
#define SCR_MUSIC    15
#define SCR_WORLD    16
#define APP_CALENDAR 17
#define APP_PHOTOS   18
#define SCR_MARKETS  19

#define W_WEATHER    0
#define W_MINIMAP    1
#define W_NEWS       2
#define W_RADAR      3
#define W_GPIO       4
#define W_TIMER      5
#define W_CORE       6
#define NUM_WIDGETS  7

// ============================================================================
// 4. GLOBAL STATE
// ============================================================================
int  homeSlot[2]       = {W_WEATHER, W_MINIMAP};
int  editSlotIndex     = 0;
int  currentScreen     = SCR_HOME;
int  currentTile       = 0;
const int NUM_TILES    = 4;
bool uiDirty           = true;
bool islandExpanded    = false;

int  screenBrightness  = 100;
bool spriteIsSwapped   = false;
bool draggingBright    = false;

bool   wifiOnline      = false;
bool   ntpSynced       = false;
bool   everestLoaded   = false;
bool   mapLoaded       = false;
bool   mapSatellite    = false;
bool   newsLoaded      = false;
String liveTime        = "12:00";
String liveDate        = SECRET_REGION_SUB;

struct NewsArticle {
  String title;
  String desc;
  String pubDate;
  bool   hasImg;
};
NewsArticle newsItems[NUM_NEWS_ITEMS];
int newsTopicIdx       = 0;
int selectedArticle    = 0;

int    wTemp = 26, wHum = 54, wWind = 11, wCode = 1;
String wCond = "Partly Cloudy";
String fDay[3]  = {"Tmrw", "Day 2", "Day 3"};
int    fHigh[3] = {30, 31, 29};
int    fLow[3]  = {18, 19, 17};
int    fCode[3] = {0, 2, 61};
unsigned long lastWeatherFetch = 0;

int    chStrength[14] = {0};
String topSSIDs[4]    = {"Tap Scan RF above", "", "", ""};
int    topRSSI[4]     = {0, 0, 0, 0};
int    topCh[4]       = {0, 0, 0, 0};
int    foundNets      = 0;

const int SCOPE_PTS   = 80;
int    scopeBuf[SCOPE_PTS];
bool   scopePaused    = false;
float  scopeLastVolt  = 0.0f;
unsigned long lastScopeTick = 0;

uint8_t rgbR = 18, rgbG = 122, rgbB = 255;
int     rgbPreset     = 1;
bool    rgbBreathing  = false;

String calcDisplay    = "0";
double calcAcc        = 0.0;
char   calcOp         = 0;
bool   calcNewInput   = true;

int  timerPresetSec   = 25 * 60;
int  timerRemainSec   = 25 * 60;
bool timerRunning     = false;
unsigned long lastTimerTick = 0;

struct Notification {
  String app;
  String title;
  String body;
  uint16_t col;
  bool active;
};
const int MAX_NOTIFS  = 5;
Notification notifs[MAX_NOTIFS];

String toastMsg       = "";
unsigned long toastEnd = 0;

bool     touchActive        = false;
bool     touchRingDrawn     = false;
int      touchStartX        = 0, touchStartY = 0;
int      touchLastX         = 0, touchLastY  = 0;
float    smoothX            = 0.0f, smoothY  = 0.0f;
unsigned long lastTouchSeen = 0;

// Forward declarations (plain types only)
void renderCurrentScreen(bool pushNow);
void switchScreen(int next);
void runAndSaveTouchCalibration();

// ============================================================================
// 5. BRIGHTNESS, SCALING & DRAWING HELPERS
// ============================================================================
void applyHardwarePWM() {
  int pwm = constrain((screenBrightness * 255) / 100, 35, 255);
  analogWrite(BACKLIGHT_PWM_PIN, pwm);
}

void applySoftwareBrightnessRect(int x0, int y0, int w, int h) {
  if (screenBrightness >= 98) return;
  uint16_t* buf = (uint16_t*)canvas.getBuffer();
  if (!buf) return;
  if (x0 < 0) { w += x0; x0 = 0; }
  if (y0 < 0) { h += y0; y0 = 0; }
  if (x0 + w > 480) w = 480 - x0;
  if (y0 + h > 320) h = 320 - y0;
  if (w <= 0 || h <= 0) return;

  uint32_t scale = (uint32_t)((screenBrightness * 256) / 100);
  for (int y = y0; y < y0 + h; y++) {
    uint16_t* row = buf + (y * 480) + x0;
    for (int i = 0; i < w; i++) {
      uint16_t v = row[i];
      if (spriteIsSwapped) v = (uint16_t)((v >> 8) | (v << 8));
      uint32_t r = (((v >> 11) & 0x1F) * scale) >> 8;
      uint32_t g = (((v >> 5)  & 0x3F) * scale) >> 8;
      uint32_t b = ((v         & 0x1F) * scale) >> 8;
      uint16_t nv = (uint16_t)((r << 11) | (g << 5) | b);
      if (spriteIsSwapped) nv = (uint16_t)((nv >> 8) | (nv << 8));
      row[i] = nv;
    }
  }
}

void pushFullFrame() {
  applySoftwareBrightnessRect(0, 0, 480, 320);
  canvas.pushSprite(0, 0);
}

void pushPartial(int x, int y, int w, int h) {
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > 480) w = 480 - x;
  if (y + h > 320) h = 320 - y;
  if (w <= 0 || h <= 0) return;
  applySoftwareBrightnessRect(x, y, w, h);
  tft.setClipRect(x, y, w, h);
  canvas.pushSprite(0, 0);
  tft.clearClipRect();
}

void blitScaledNewsImg(int idx, int dx, int dy, int dw, int dh) {
  if (idx < 0 || idx >= NUM_NEWS_ITEMS || !newsImg[idx] || dw <= 0 || dh <= 0) return;
  uint16_t* sBuf = (uint16_t*)newsImg[idx]->getBuffer();
  uint16_t* dBuf = (uint16_t*)canvas.getBuffer();
  if (!sBuf || !dBuf) return;
  int sw = newsImg[idx]->width();
  int sh = newsImg[idx]->height();
  for (int y = 0; y < dh; y++) {
    int ty = dy + y;
    if (ty < 0 || ty >= 320) continue;
    int sy = (y * sh) / dh;
    uint16_t* sRow = sBuf + sy * sw;
    uint16_t* dRow = dBuf + ty * 480;
    for (int x = 0; x < dw; x++) {
      int tx = dx + x;
      if (tx < 0 || tx >= 480) continue;
      dRow[tx] = sRow[(x * sw) / dw];
    }
  }
}

void drawThickLine(int x0, int y0, int x1, int y1, int thick, uint16_t col) {
  for (int d = -(thick / 2); d <= thick / 2; d++) {
    canvas.drawLine(x0, y0 + d, x1, y1 + d, col);
    canvas.drawLine(x0 + d, y0, x1 + d, y1, col);
  }
}

void drawMapRoad(int x0, int y0, int x1, int y1, int halfW, uint16_t col) {
  for (int d = -halfW; d <= halfW; d++) {
    mapSprite.drawLine(x0, y0 + d, x1, y1 + d, col);
    mapSprite.drawLine(x0 + d, y0, x1 + d, y1, col);
  }
}

void showToast(const String& msg, int durationMs) {
  toastMsg = msg;
  toastEnd = millis() + (unsigned long)durationMs;
  uiDirty  = true;
}

void pushNotif(const String& app, const String& title, const String& body, uint16_t col) {
  for (int i = MAX_NOTIFS - 1; i > 0; i--) notifs[i] = notifs[i - 1];
  notifs[0].app    = app;
  notifs[0].title  = title;
  notifs[0].body   = body;
  notifs[0].col    = col;
  notifs[0].active = true;
  uiDirty          = true;
}

void applyRgb(uint8_t r, uint8_t g, uint8_t b) {
  rgbR = r; rgbG = g; rgbB = b;
  setOnboardRGB(r / 4, g / 4, b / 4);
}

String wmoToText(int code) {
  if (code == 0)  return "Clear Sky";
  if (code <= 2)  return "Partly Cloudy";
  if (code == 3)  return "Overcast";
  if (code <= 48) return "Misty Fog";
  if (code <= 67) return "Rain Showers";
  if (code <= 77) return "Snowfall";
  if (code <= 82) return "Heavy Rain";
  return "Thunderstorm";
}

void drawWrappedText(const String& text, int x, int y, int maxW, int maxLines, int lineH, uint16_t col, int fontStyle) {
  if (fontStyle == 1)      canvas.setFont(&fonts::FreeSerifBold9pt7b);
  else if (fontStyle == 2) canvas.setFont(&fonts::FreeSerifBold12pt7b);
  else                     canvas.setFont(&fonts::FreeSerif9pt7b);
  canvas.setTextDatum(lgfx::top_left);
  canvas.setTextColor(col);

  int curY = y, lineNum = 0, len = (int)text.length(), pos = 0;
  while (pos < len && lineNum < maxLines) {
    while (pos < len && text[pos] == ' ') pos++;
    if (pos >= len) break;

    String line = "";
    int lastGood = pos, scan = pos;
    while (scan < len) {
      int sp = text.indexOf(' ', scan);
      if (sp < 0) sp = len;
      String word = text.substring(scan, sp);
      String cand = (line.length() == 0) ? word : (line + " " + word);
      if (canvas.textWidth(cand) <= maxW) {
        line = cand;
        lastGood = sp;
        scan = sp + 1;
      } else {
        if (line.length() == 0) {
          while (word.length() > 1 && canvas.textWidth(word) > maxW) word.remove(word.length() - 1);
          line = word;
          lastGood = scan + (int)word.length();
        }
        break;
      }
    }
    if (lineNum == maxLines - 1 && lastGood < len) {
      while (line.length() > 2 && canvas.textWidth(line + "..") > maxW) line.remove(line.length() - 1);
      line += "..";
    }
    canvas.drawString(line, x, curY);
    curY += lineH;
    lineNum++;
    pos = lastGood;
  }
}

// ============================================================================
// 6. WHITE MOUNTAIN BOOT LOGO WITH CLOUDS & GOLDEN SUN
// ============================================================================
void drawBootSplash(const char* statusText, int progressPct) {
  for (int y = 0; y < 320; y += 2) {
    float t = (float)y / 320.0f;
    tft.fillRect(0, y, 480, 2, makeRgb((uint8_t)(14 + t * 30), (uint8_t)(78 + t * 86), (uint8_t)(186 + t * 56)));
  }

  int cx = 240, cy = 108;
  tft.fillCircle(cx, cy + 4, 68, makeRgb(10, 44, 118));
  tft.fillCircle(cx, cy, 66, makeRgb(24, 124, 240));
  tft.drawCircle(cx, cy, 66, C_WHITE);
  tft.drawCircle(cx, cy, 65, C_WHITE);

  // Golden Sun with Rays
  int sx = cx + 26, sy = cy - 28;
  tft.fillCircle(sx, sy, 16, blend565(C_GOLD, makeRgb(24, 124, 240), 115));
  tft.fillCircle(sx, sy, 12, C_GOLD);
  for (int a = 0; a < 360; a += 45) {
    float rad = a * 0.017453f;
    int rx0 = sx + (int)(cosf(rad) * 15.0f), ry0 = sy + (int)(sinf(rad) * 15.0f);
    int rx1 = sx + (int)(cosf(rad) * 20.0f), ry1 = sy + (int)(sinf(rad) * 20.0f);
    tft.drawLine(rx0, ry0, rx1, ry1, C_GOLD);
    tft.drawLine(rx0 + 1, ry0, rx1 + 1, ry1, C_GOLD);
  }

  // White Twin-Peak Mountain Logo
  tft.fillTriangle(cx + 16, cy - 16, cx - 10, cy + 32, cx + 48, cy + 32, makeRgb(224, 238, 254));
  tft.fillTriangle(cx - 10, cy - 36, cx - 50, cy + 32, cx + 30, cy + 32, C_WHITE);
  tft.fillTriangle(cx - 10, cy - 36, cx - 2,  cy + 32, cx + 30, cy + 32, makeRgb(208, 228, 248));

  // Fluffy White Clouds (Top-Left + Base)
  tft.fillCircle(cx - 38, cy - 22, 9, C_WHITE);
  tft.fillCircle(cx - 28, cy - 26, 11, C_WHITE);
  tft.fillCircle(cx - 18, cy - 22, 8, C_WHITE);
  tft.fillRoundRect(cx - 46, cy - 22, 36, 9, 4, C_WHITE);

  tft.fillCircle(cx - 30, cy + 30, 13, C_WHITE);
  tft.fillCircle(cx - 12, cy + 28, 16, C_WHITE);
  tft.fillCircle(cx + 10, cy + 30, 15, C_WHITE);
  tft.fillCircle(cx + 30, cy + 31, 12, C_WHITE);
  tft.fillRoundRect(cx - 42, cy + 28, 84, 15, 7, C_WHITE);
  tft.drawCircle(cx, cy, 66, C_WHITE);

  tft.setFont(&fonts::FreeSerifBold24pt7b);
  tft.setTextDatum(lgfx::middle_center);
  tft.setTextColor(C_WHITE);
  tft.drawString("cuteOS", 240, 208);

  tft.setFont(&fonts::FreeSerif9pt7b);
  tft.setTextColor(makeRgb(228, 242, 255));
  tft.drawString(statusText, 240, 248);

  tft.fillSmoothRoundRect(110, 270, 260, 12, 6, makeRgb(16, 62, 148));
  int pw = constrain((progressPct * 256) / 100, 12, 256);
  tft.fillSmoothRoundRect(112, 272, pw, 8, 4, C_WHITE);
}

void updateBootProgress(const char* statusText, int progressPct) {
  tft.fillRect(30, 234, 420, 28, makeRgb(36, 144, 228));
  tft.setFont(&fonts::FreeSerif9pt7b);
  tft.setTextDatum(lgfx::middle_center);
  tft.setTextColor(C_WHITE);
  tft.drawString(statusText, 240, 248);

  tft.fillSmoothRoundRect(110, 270, 260, 12, 6, makeRgb(16, 62, 148));
  int pw = constrain((progressPct * 256) / 100, 12, 256);
  tft.fillSmoothRoundRect(112, 272, pw, 8, 4, C_WHITE);
}

// ============================================================================
// 7. HTTPS DOWNLOADER, REAL MOUNT EVEREST PHOTO & GLASS BUFFERS
// ============================================================================
void rebuildGlassBuffers() {
  for (int y = 0; y < 320; y += 4) {
    for (int x = 0; x < 480; x += 4) {
      uint16_t p1 = bgSprite.readPixel(x, y);
      uint16_t p2 = bgSprite.readPixel((x + 3 < 480) ? x + 3 : x, (y + 3 < 320) ? y + 3 : y);
      uint16_t blur = blend565(p1, p2, 128);
      frostSprite.fillRect(x, y, 4, 4, blend565(C_WHITE, blur, 198));
      darkGlass.fillRect(x, y, 4, 4, blend565(C_DARK_PILL, blur, 168));
    }
  }
}

void buildPhotorealisticEverestFallback() {
  for (int y = 0; y < 245; y++) {
    float t = (float)y / 245.0f;
    bgSprite.drawFastHLine(0, y, 480, makeRgb((uint8_t)(28 + t * 150), (uint8_t)(72 + t * 140), (uint8_t)(152 + t * 88)));
  }
  for (int i = 0; i < 165; i++) {
    int px = 248 + i, py = 54 + (int)(sinf(i * 0.04f) * 4.0f) + (i / 18), thick = 6 + (i / 14);
    for (int dy = -thick; dy <= thick; dy++) {
      int yy = py + dy;
      if (yy < 0 || yy >= 240) continue;
      float fade = (1.0f - (float)abs(dy) / (float)(thick + 1)) * (1.0f - (float)i / 170.0f);
      uint8_t a = (uint8_t)(fade * 210.0f);
      if (a > 8) bgSprite.drawPixel(px, yy, blend565(C_WHITE, bgSprite.readPixel(px, yy), a));
    }
  }
  for (int x = 0; x < 480; x++) {
    float yEv = 52.0f + fabsf((float)(x - 246)) * 0.88f + sinf(x * 0.09f) * 5.0f + cosf(x * 0.23f) * 2.5f;
    float yL  = 112.0f + fabsf((float)(x - 90)) * 0.62f + sinf(x * 0.11f) * 6.0f;
    float yR  = 118.0f + fabsf((float)(x - 410)) * 0.68f + cosf(x * 0.10f) * 5.0f;
    int ridgeY = (int)fminf(yEv, fminf(yL, yR));
    if (ridgeY > 235) ridgeY = 235;
    int spineX = 246 + (int)(sinf((float)ridgeY * 0.08f) * 14.0f);
    for (int y = ridgeY; y < 320; y++) {
      float depth = (float)(y - ridgeY) / 195.0f;
      if (depth > 1.0f) depth = 1.0f;
      float noise = sinf(x * 0.16f + y * 0.11f) * cosf(x * 0.07f - y * 0.14f);
      bool sunlit = (x < spineX + (y - ridgeY) / 5);
      bool snow = (noise > -0.18f) || (y - ridgeY < 26);
      uint16_t col = sunlit
        ? (snow ? blend565(makeRgb(252,253,255), makeRgb(196,214,238), (uint8_t)(depth * 130))
                : blend565(makeRgb(96,108,128),  makeRgb(52,62,82),    (uint8_t)(depth * 150)))
        : (snow ? blend565(makeRgb(168,194,230), makeRgb(98,126,168),  (uint8_t)(depth * 160))
                : blend565(makeRgb(38,48,72),    makeRgb(22,30,48),    (uint8_t)(depth * 150)));
      if (y > 230) col = blend565(makeRgb(218, 228, 242), col, (uint8_t)(((y - 230) * 150) / 90));
      bgSprite.drawPixel(x, y, col);
    }
  }
  rebuildGlassBuffers();
}

bool downloadBufferHTTPS(const String& url, uint8_t** outBuf, size_t* outLen, size_t maxBytes) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setUserAgent("cuteOS-ESP32S3/2.0");
  http.setTimeout(6500);

  if (!http.begin(client, url)) return false;
  int code = http.GET();
  if (code != 200) { http.end(); return false; }

  int total = http.getSize();
  if (total > (int)maxBytes) { http.end(); return false; }

  size_t allocSize = (total > 0) ? (size_t)total : maxBytes;
  uint8_t* buf = (uint8_t*)ps_malloc(allocSize);
  if (!buf) { http.end(); return false; }

  WiFiClient* stream = http.getStreamPtr();
  size_t received = 0;
  unsigned long startMs = millis();
  while (http.connected() && (total < 0 || (int)received < total) && millis() - startMs < 6500) {
    size_t avail = stream->available();
    if (avail) {
      size_t toRead = avail;
      if (received + toRead > allocSize) toRead = allocSize - received;
      if (toRead == 0) break;
      int n = stream->readBytes(buf + received, toRead);
      if (n > 0) received += (size_t)n;
    } else {
      delay(4);
    }
  }
  http.end();

  if (received < 256 || (total >= 0 && received != (size_t)total)) {
    free(buf);
    return false;
  }
  *outBuf = buf;
  *outLen = received;
  return true;
}

bool downloadRealEverestPhoto() {
  if (WiFi.status() != WL_CONNECTED) return false;
  const char* urls[2] = {
    "https://commons.wikimedia.org/wiki/Special:FilePath/Everest_North_Face_toward_Base_Camp_Tibet_Luca_Galuzzi_2006.jpg?width=480",
    "https://commons.wikimedia.org/wiki/Special:FilePath/Mount_Everest_from_Kala_Patthar.jpg?width=480"
  };
  for (int i = 0; i < 2; i++) {
    uint8_t* jpgBuf = NULL;
    size_t jpgLen = 0;
    if (downloadBufferHTTPS(String(urls[i]), &jpgBuf, &jpgLen, 160000)) {
      bool decoded = bgSprite.drawJpg(jpgBuf, (uint32_t)jpgLen, 0, 0);
      free(jpgBuf);
      if (!decoded) continue;
      rebuildGlassBuffers();
      everestLoaded = true;
      return true;
    }
  }
  return false;
}

// ============================================================================
// 8. LIVE NEWS RSS + REAL PHOTO ENGINE (5 LATEST STORIES PER TOPIC)
// ============================================================================
String cleanXmlText(String s) {
  s.replace("<![CDATA[", ""); s.replace("]]>", "");
  s.replace("&amp;", "&");    s.replace("&quot;", "\"");
  s.replace("&#39;", "'");    s.replace("&apos;", "'");
  s.replace("&lt;", "<");     s.replace("&gt;", ">");
  s.replace("&rsquo;", "'");  s.replace("&lsquo;", "'");
  s.replace("&rdquo;", "\""); s.replace("&ldquo;", "\"");
  s.replace("&ndash;", "-");  s.replace("&mdash;", "-");
  s.replace("\r", " ");       s.replace("\n", " ");
  int lt = s.indexOf('<');
  while (lt >= 0) {
    int gt = s.indexOf('>', lt);
    if (gt < 0) break;
    s.remove(lt, gt - lt + 1);
    lt = s.indexOf('<');
  }
  s.trim();
  return s;
}

String extractXmlTag(const String& item, const char* openTag, const char* closeTag) {
  int s = item.indexOf(openTag);
  if (s < 0) return "";
  s += (int)strlen(openTag);
  int e = item.indexOf(closeTag, s);
  if (e < 0) return "";
  return cleanXmlText(item.substring(s, e));
}

String extractImageUrl(const String& item) {
  const char* keys[3] = {"<media:thumbnail", "<media:content", "<enclosure"};
  for (int k = 0; k < 3; k++) {
    int p = item.indexOf(keys[k]);
    if (p < 0) continue;
    int u = item.indexOf("url=\"", p);
    if (u < 0) continue;
    u += 5;
    int e = item.indexOf('"', u);
    if (e > u) {
      String url = item.substring(u, e);
      url.replace("&amp;", "&");
      if (url.endsWith(".jpg.webp")) url.remove(url.length() - 5);
      url.replace("/standard/976/",  "/standard/240/");
      url.replace("/standard/640/",  "/standard/240/");
      url.replace("/standard/480/",  "/standard/240/");
      url.replace("/standard/1024/", "/standard/240/");
      return url;
    }
  }
  return "";
}

void drawNewsPlaceholder(int idx, uint16_t accent) {
  LGFX_Sprite* sp = newsImg[idx];
  if (!sp) return;
  for (int y = 0; y < 135; y++) sp->drawFastHLine(0, y, 240, blend565(accent, C_DARK_PILL, (uint8_t)(165 - y)));
  sp->fillCircle(195, 34, 42, blend565(C_WHITE, accent, 55));
  sp->fillCircle(45, 110, 32, blend565(C_CYAN, accent, 65));
  sp->setFont(&fonts::FreeSerifBold12pt7b);
  sp->setTextDatum(lgfx::middle_center);
  sp->setTextColor(C_WHITE);
  sp->drawString(TOPIC_NAMES[newsTopicIdx], 120, 68);
}

void initOfflineNewsPlaceholders() {
  uint16_t cols[5] = {C_BLUE, C_ROSE, C_TEAL, C_VIOLET, C_ORANGE};
  for (int i = 0; i < NUM_NEWS_ITEMS; i++) {
    newsItems[i].title   = "Tap Refresh for Live News";
    newsItems[i].desc    = String("Connect to ") + WIFI_SSID + " Wi-Fi to load the 5 latest headlines with real images.";
    newsItems[i].pubDate = "Live Feed";
    newsItems[i].hasImg  = false;
    drawNewsPlaceholder(i, cols[i]);
  }
}

bool fetchLiveNews(int topicIdx) {
  if (topicIdx < 0 || topicIdx >= NUM_NEWS_TOPICS) topicIdx = 0;
  newsTopicIdx = topicIdx;
  if (WiFi.status() != WL_CONNECTED) return false;

  uint8_t* xmlBuf = NULL;
  size_t xmlLen = 0;
  bool ok = downloadBufferHTTPS(String(TOPIC_URLS[topicIdx]), &xmlBuf, &xmlLen, 48000);
  if (!ok) ok = downloadBufferHTTPS(String("https://feeds.bbci.co.uk/news/world/rss.xml"), &xmlBuf, &xmlLen, 48000);
  if (!ok || !xmlBuf) return false;

  String xml;
  xml.reserve(xmlLen + 1);
  for (size_t i = 0; i < xmlLen; i++) xml += (char)xmlBuf[i];
  free(xmlBuf);

  uint16_t cols[5] = {C_BLUE, C_ROSE, C_TEAL, C_VIOLET, C_ORANGE};
  int found = 0, searchPos = 0;

  while (found < NUM_NEWS_ITEMS) {
    int s = xml.indexOf("<item>", searchPos);
    if (s < 0) break;
    int e = xml.indexOf("</item>", s);
    if (e < 0) break;
    searchPos = e + 7;

    String item  = xml.substring(s, e);
    String title = extractXmlTag(item, "<title>", "</title>");
    if (title.length() < 4) continue;

    String desc = extractXmlTag(item, "<description>", "</description>");
    String pub  = extractXmlTag(item, "<pubDate>", "</pubDate>");
    if (pub.length() > 22) pub = pub.substring(0, 22);
    String imgU = extractImageUrl(item);

    newsItems[found].title   = title;
    newsItems[found].desc    = (desc.length() > 0) ? desc : title;
    newsItems[found].pubDate = (pub.length() > 0) ? pub : String(TOPIC_NAMES[topicIdx]);
    newsItems[found].hasImg  = false;
    drawNewsPlaceholder(found, cols[found]);

    if (imgU.startsWith("http")) {
      uint8_t* iBuf = NULL;
      size_t iLen = 0;
      if (downloadBufferHTTPS(imgU, &iBuf, &iLen, 70000)) {
        bool isPng = (iLen > 4 && iBuf[0] == 0x89 && iBuf[1] == 0x50);
        newsItems[found].hasImg = isPng
          ? newsImg[found]->drawPng(iBuf, (uint32_t)iLen, 0, 0)
          : newsImg[found]->drawJpg(iBuf, (uint32_t)iLen, 0, 0);
        free(iBuf);
      }
    }
    found++;
  }
  if (found > 0) { newsLoaded = true; return true; }
  return false;
}

// ============================================================================
// 9. REAL LOCAL AREA MAP (STREET OSM + REAL SATELLITE ESRI)
// ============================================================================
void buildOfflineAreaMap() {
  mapSprite.fillScreen(makeRgb(242, 245, 250));
  mapSprite.fillSmoothRoundRect(24,  28,  130, 88, 16, makeRgb(206, 236, 206));
  mapSprite.fillSmoothRoundRect(336, 188, 126, 98, 16, makeRgb(206, 236, 206));
  mapSprite.fillSmoothRoundRect(352, 24,  112, 72, 14, makeRgb(204, 228, 250));
  drawMapRoad(0, 260, 480, 120, 6, makeRgb(255, 226, 148));
  drawMapRoad(145, 0, 230, 320, 5, C_WHITE);
  drawMapRoad(295, 0, 345, 320, 4, C_WHITE);
  for (int r = 0; r < 5; r++) mapSprite.fillRect(150, 92 + r * 32, 190, 5, C_WHITE);
  for (int c = 0; c < 4; c++) mapSprite.fillRect(168 + c * 52, 88, 5, 145, C_WHITE);
  mapSprite.setFont(&fonts::FreeSerif9pt7b);
  mapSprite.setTextDatum(lgfx::middle_center);
  mapSprite.setTextColor(makeRgb(82, 92, 108));
  mapSprite.drawString(SECRET_MAP_ROAD_1, 90, 224);
  mapSprite.drawString(SECRET_MAP_ROAD_2, 395, 168);
  mapSprite.drawString(SECRET_MAP_ROAD_3, 125, 126);
}

constexpr int MAP_ZOOM = 17;
constexpr int MAP_TILE = 256;
constexpr int MAP_WORLD = MAP_TILE * (1 << MAP_ZOOM);
constexpr size_t MAP_CACHE_SIZE = 10;
struct MapTile {
  int x, y;
  bool satellite;
  uint8_t* data;
  size_t length;
  uint32_t used;
};
MapTile mapCache[MAP_CACHE_SIZE]{};
uint32_t tileAccess = 0;
int mapCenterX = -1, mapCenterY = -1;
bool draggingMap = false;
int lastMapPreviewX = 0, lastMapPreviewY = 0;
unsigned long lastMapPreview = 0;

void centerMap(double latitude, double longitude) {
  double latRad = constrain(latitude, -85.05, 85.05) * M_PI / 180.0;
  mapCenterX = (int)((longitude + 180.0) * MAP_WORLD / 360.0);
  mapCenterY = (int)((1.0 - asinh(tan(latRad)) / M_PI) * MAP_WORLD / 2);
}

bool paintMapTile(int tileX, int tileY, int x, int y, bool satellite) {
  MapTile* slot = &mapCache[0];
  for (auto& tile : mapCache) {
    if (tile.data && tile.x == tileX && tile.y == tileY && tile.satellite == satellite) {
      tile.used = ++tileAccess;
      return satellite ? mapSprite.drawJpg(tile.data, tile.length, x, y)
                       : mapSprite.drawPng(tile.data, tile.length, x, y);
    }
    if (!tile.data || (slot->data && tile.used < slot->used)) slot = &tile;
  }
  uint8_t* data = nullptr;
  size_t length = 0;
  String prefix = satellite
    ? "https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/17/"
    : "https://tile.openstreetmap.org/17/";
  String url = satellite ? prefix + tileY + "/" + tileX : prefix + tileX + "/" + tileY + ".png";
  if (!downloadBufferHTTPS(url, &data, &length, 120000)) return false;
  bool decoded = satellite ? mapSprite.drawJpg(data, length, x, y)
                           : mapSprite.drawPng(data, length, x, y);
  if (!decoded) {
    free(data);
    return false;
  }
  free(slot->data);
  *slot = {tileX, tileY, satellite, data, length, ++tileAccess};
  return true;
}

bool downloadAreaRealMap(bool useSatellite) {
  mapLoaded = false;
  mapSatellite = useSatellite;
  if (mapCenterX < 0) {
    if (HOME_LAT == 0.0 && HOME_LON == 0.0) centerMap(17.3850, 78.4867);
    else centerMap(HOME_LAT, HOME_LON);
  }
  if (WiFi.status() != WL_CONNECTED) { buildOfflineAreaMap(); return false; }
  int left = mapCenterX - 240;
  int top = mapCenterY - 160;
  int firstX = (int)floor((double)left / MAP_TILE);
  int firstY = (int)floor((double)top / MAP_TILE);
  int tilesPerSide = 1 << MAP_ZOOM;
  mapSprite.fillScreen(makeRgb(236, 240, 246));
  int okTiles = 0;
  for (int row = firstY; row * MAP_TILE < top + 320; ++row) {
    if (row < 0 || row >= tilesPerSide) continue;
    for (int col = firstX; col * MAP_TILE < left + 480; ++col) {
      int wrapped = (col % tilesPerSide + tilesPerSide) % tilesPerSide;
      if (paintMapTile(wrapped, row, col * MAP_TILE - left, row * MAP_TILE - top, useSatellite)) ++okTiles;
    }
  }
  if (okTiles) { mapLoaded = true; return true; }
  buildOfflineAreaMap();
  return false;
}

// ============================================================================
// 10. GLASSY CARDS & BRIGHT APPLE-STYLE SQUIRCLE ICONS
// ============================================================================
void restoreRoundedCorners(int x, int y, int w, int h, int r) {
  for (int cy = 0; cy < r; cy++) {
    float dy = (float)(r - cy) - 0.5f;
    int inset = r - (int)sqrtf((float)(r * r) - dy * dy);
    if (inset > 0) {
      for (int i = 0; i < inset; i++) {
        if (y + cy >= 0 && y + cy < 320) {
          if (x + i >= 0 && x + i < 480) canvas.drawPixel(x + i, y + cy, bgSprite.readPixel(x + i, y + cy));
          if (x + w - 1 - i >= 0 && x + w - 1 - i < 480) canvas.drawPixel(x + w - 1 - i, y + cy, bgSprite.readPixel(x + w - 1 - i, y + cy));
        }
        int by = y + h - 1 - cy;
        if (by >= 0 && by < 320) {
          if (x + i >= 0 && x + i < 480) canvas.drawPixel(x + i, by, bgSprite.readPixel(x + i, by));
          if (x + w - 1 - i >= 0 && x + w - 1 - i < 480) canvas.drawPixel(x + w - 1 - i, by, bgSprite.readPixel(x + w - 1 - i, by));
        }
      }
    }
  }
}

void drawCleanCard(int x, int y, int w, int h, int r, uint16_t fillCol, uint16_t borderCol) {
  canvas.fillSmoothRoundRect(x + 2, y + 4, w, h, r, makeRgb(14, 20, 34));
  canvas.fillSmoothRoundRect(x, y, w, h, r, fillCol);
  canvas.drawRoundRect(x, y, w, h, r, borderCol);
}

void drawFrostGlassCard(int x, int y, int w, int h, int r, bool darkStyle, uint16_t rimCol) {
  canvas.fillSmoothRoundRect(x + 2, y + 4, w, h, r, makeRgb(10, 14, 26));
  canvas.setClipRect(x, y, w, h);
  if (darkStyle) darkGlass.pushSprite(&canvas, 0, 0);
  else           frostSprite.pushSprite(&canvas, 0, 0);
  canvas.clearClipRect();
  restoreRoundedCorners(x, y, w, h, r);
  canvas.drawRoundRect(x, y, w, h, r, rimCol);
  if (w > 2 * r) canvas.drawFastHLine(x + r, y + 1, w - 2 * r, C_WHITE);
}

void drawAppleSquircle(int cx, int cy, int s, uint16_t colTop, uint16_t colBot) {
  int x = cx - s / 2, y = cy - s / 2, r = (s * 30) / 100;
  canvas.fillSmoothRoundRect(x + 2, y + 4, s, s, r, makeRgb(14, 20, 34));
  canvas.fillSmoothRoundRect(x, y, s, s, r, colBot);
  canvas.fillSmoothRoundRect(x, y, s, s / 2 + 4, r, colTop);
  canvas.fillRect(x + 2, y + s / 2 - 4, s - 4, 10, blend565(colTop, colBot, 128));
  canvas.fillSmoothRoundRect(x + 6, y + 3, s - 12, s / 4, r / 2, blend565(C_WHITE, colTop, 88));
  canvas.drawRoundRect(x, y, s, s, r, blend565(C_WHITE, colTop, 145));
}

void drawIconNews(int cx, int cy, int s) {
  drawAppleSquircle(cx, cy, s, C_ROSE, makeRgb(188, 18, 66));
  int hs = s / 4;
  canvas.fillSmoothRoundRect(cx - hs - 3, cy - hs - 1, hs * 2 + 6, hs * 2 + 2, 4, C_WHITE);
  canvas.fillRect(cx - hs, cy - hs + 2, hs, hs - 2, C_ROSE);
  canvas.fillRect(cx + 2, cy - hs + 3, hs - 2, 3, C_INK_SUB);
  canvas.fillRect(cx + 2, cy - hs + 8, hs - 2, 3, C_INK_SUB);
  canvas.fillRect(cx - hs, cy + 3, hs * 2, 3, C_INK_SUB);
  canvas.fillRect(cx - hs, cy + 8, hs * 2 - 4, 3, C_INK_SUB);
}

void drawIconMap(int cx, int cy, int s) {
  drawAppleSquircle(cx, cy, s, C_WHITE, C_SOFT_GRAY);
  int d = s / 4;
  canvas.fillSmoothRoundRect(cx - d - 4, cy - d - 2, d + 4, d * 2 + 4, 4, C_TINT_GREEN);
  canvas.fillSmoothRoundRect(cx + 1, cy - d - 2, d + 4, d * 2 + 4, 4, C_TINT_BLUE);
  drawThickLine(cx - d - 3, cy + d, cx + d + 3, cy - d, 3, C_GOLD);
  canvas.fillTriangle(cx - 7, cy - 2, cx + 7, cy - 2, cx, cy + 11, C_RED);
  canvas.fillCircle(cx, cy - 5, 8, C_RED);
  canvas.fillCircle(cx, cy - 5, 3, C_WHITE);
}

void drawIconWeather(int cx, int cy, int s) {
  drawAppleSquircle(cx, cy, s, C_CYAN, C_BLUE);
  canvas.fillCircle(cx + 7, cy - 7, s / 6, C_GOLD);
  canvas.fillCircle(cx - 6, cy + 4, s / 6, C_WHITE);
  canvas.fillCircle(cx + 5, cy + 5, s / 7, C_WHITE);
  canvas.fillRoundRect(cx - s / 4 - 2, cy + 3, s / 2 + 4, s / 6 + 2, 4, C_WHITE);
}

void drawIconControl(int cx, int cy, int s) {
  drawAppleSquircle(cx, cy, s, makeRgb(72, 92, 138), C_DARK_PILL);
  canvas.fillRoundRect(cx - 14, cy - 8, 28, 4, 2, C_CYAN);
  canvas.fillCircle(cx + 6, cy - 6, 5, C_WHITE);
  canvas.fillRoundRect(cx - 14, cy + 6, 28, 4, 2, C_GOLD);
  canvas.fillCircle(cx - 6, cy + 8, 5, C_WHITE);
}

void drawIconApps(int cx, int cy, int s) {
  drawAppleSquircle(cx, cy, s, C_VIOLET, makeRgb(96, 30, 178));
  for (int r = 0; r < 2; r++)
    for (int c = 0; c < 2; c++)
      canvas.fillSmoothRoundRect(cx - 12 + c * 14, cy - 12 + r * 14, 10, 10, 3, ((r + c) == 0) ? C_WHITE : ((r == 0) ? C_GOLD : ((c == 0) ? C_CYAN : C_WHITE)));
}

void drawIconRadar(int cx, int cy, int s) {
  drawAppleSquircle(cx, cy, s, C_GREEN, makeRgb(14, 138, 62));
  canvas.fillArc(cx, cy + 10, s / 3 + 2, s / 3 - 2, 225, 315, C_WHITE);
  canvas.fillArc(cx, cy + 10, s / 5 + 1, s / 5 - 3, 225, 315, C_GOLD);
  canvas.fillCircle(cx, cy + 10, 4, C_WHITE);
}

void drawIconScope(int cx, int cy, int s) {
  drawAppleSquircle(cx, cy, s, C_GOLD, C_ORANGE);
  int prevX = cx - s / 3, prevY = cy;
  for (int i = 1; i <= 10; i++) {
    int nx = cx - s / 3 + (i * (2 * s / 3)) / 10;
    int ny = cy - (int)(sinf(i * 0.65f) * (s * 0.24f));
    drawThickLine(prevX, prevY, nx, ny, 2, C_WHITE);
    prevX = nx; prevY = ny;
  }
}

void drawIconRGB(int cx, int cy, int s) {
  drawAppleSquircle(cx, cy, s, C_WHITE, C_SOFT_GRAY);
  int cr = s / 3;
  canvas.fillArc(cx, cy, cr, 5, 0,   90,  C_GOLD);
  canvas.fillArc(cx, cy, cr, 5, 90,  180, C_GREEN);
  canvas.fillArc(cx, cy, cr, 5, 180, 270, C_BLUE);
  canvas.fillArc(cx, cy, cr, 5, 270, 360, C_ROSE);
  canvas.fillCircle(cx, cy, 5, C_WHITE);
}

void drawIconCalc(int cx, int cy, int s) {
  drawAppleSquircle(cx, cy, s, C_TEAL, makeRgb(10, 108, 152));
  canvas.fillRoundRect(cx - 12, cy - 8, 9, 2, 1, C_WHITE);
  canvas.fillRoundRect(cx - 8,  cy - 12, 2, 9, 1, C_WHITE);
  canvas.fillRoundRect(cx + 3,  cy - 8, 9, 2, 1, C_GOLD);
  canvas.fillCircle(cx + 7, cy + 7, 7, C_ORANGE);
  canvas.fillRoundRect(cx + 3,  cy + 5, 8, 2, 1, C_WHITE);
  canvas.fillRoundRect(cx + 3,  cy + 8, 8, 2, 1, C_WHITE);
  canvas.drawLine(cx - 11, cy + 4, cx - 4, cy + 11, C_WHITE);
  canvas.drawLine(cx - 4,  cy + 4, cx - 11, cy + 11, C_WHITE);
}

void drawIconTimer(int cx, int cy, int s) {
  drawAppleSquircle(cx, cy, s, C_ROSE, C_VIOLET);
  canvas.drawCircle(cx, cy + 2, s / 4, C_WHITE);
  canvas.fillRoundRect(cx - 5, cy - s / 4 - 4, 10, 3, 1, C_GOLD);
  drawThickLine(cx, cy + 2, cx, cy - 6, 2, C_WHITE);
  drawThickLine(cx, cy + 2, cx + 6, cy + 5, 2, C_GOLD);
}

void drawWeatherMiniGraphic(int cx, int cy, int code) {
  if (code == 0) {
    canvas.fillCircle(cx, cy, 11, C_GOLD);
    canvas.drawCircle(cx, cy, 14, C_ORANGE);
  } else if (code <= 2) {
    canvas.fillCircle(cx + 6, cy - 6, 9, C_GOLD);
    canvas.fillCircle(cx - 5, cy + 4, 8, C_BLUE);
    canvas.fillCircle(cx + 5, cy + 4, 7, C_BLUE);
    canvas.fillRoundRect(cx - 13, cy + 3, 26, 9, 4, C_BLUE);
  } else if (code <= 48) {
    canvas.fillCircle(cx - 5, cy, 9, C_INK_SUB);
    canvas.fillCircle(cx + 5, cy - 2, 10, C_BLUE);
    canvas.fillRoundRect(cx - 14, cy, 28, 10, 5, C_BLUE);
  } else {
    canvas.fillCircle(cx - 5, cy - 3, 8, C_BLUE);
    canvas.fillCircle(cx + 5, cy - 4, 9, C_BLUE);
    canvas.fillRoundRect(cx - 13, cy - 3, 26, 9, 4, C_BLUE);
    drawThickLine(cx - 6, cy + 8, cx - 9, cy + 14, 2, C_CYAN);
    drawThickLine(cx + 1, cy + 8, cx - 2, cy + 14, 2, C_CYAN);
    drawThickLine(cx + 8, cy + 8, cx + 5, cy + 14, 2, C_BLUE);
  }
}

void drawRingGauge(int cx, int cy, int rOut, int rIn, int pct, uint16_t col, const char* label) {
  pct = constrain(pct, 0, 100);
  canvas.fillArc(cx, cy, rOut, rIn, 0, 360, C_SOFT_GRAY);
  if (pct > 0) {
    float endDeg = 270.0f + (pct * 360.0f) / 100.0f;
    if (endDeg <= 360.0f) canvas.fillArc(cx, cy, rOut, rIn, 270, (int)endDeg, col);
    else { canvas.fillArc(cx, cy, rOut, rIn, 270, 360, col); canvas.fillArc(cx, cy, rOut, rIn, 0, (int)(endDeg - 360.0f), col); }
  }
  canvas.setFont(&fonts::FreeSerifBold9pt7b);
  canvas.setTextDatum(lgfx::middle_center);
  canvas.setTextColor(C_INK);
  canvas.drawString(String(pct) + "%", cx, cy - 3);
  canvas.setFont(&fonts::FreeSerif9pt7b);
  canvas.setTextColor(col);
  canvas.drawString(label, cx, cy + 13);
}

// ============================================================================
// 11. DYNAMIC ISLAND, SUB-HEADER & MODULAR WIDGETS
// ============================================================================
void drawDynamicIsland() {
  if (!islandExpanded) {
    int iw = 206, ih = 28, ix = 137, iy = 6;
    canvas.fillSmoothRoundRect(ix, iy, iw, ih, 14, C_DARK_PILL);
    canvas.drawRoundRect(ix, iy, iw, ih, 14, wifiOnline ? C_CYAN : C_GOLD);
    canvas.fillCircle(ix + 16, iy + 14, 4, wifiOnline ? C_GREEN : C_GOLD);
    canvas.setFont(&fonts::FreeSerifBold9pt7b);
    canvas.setTextDatum(lgfx::middle_left);
    canvas.setTextColor(C_WHITE);
    canvas.drawString("cuteOS", ix + 26, iy + 14);
    canvas.setTextDatum(lgfx::middle_right);
    if (timerRunning) {
      char tb[10];
      snprintf(tb, sizeof(tb), "%02d:%02d", timerRemainSec / 60, timerRemainSec % 60);
      canvas.setTextColor(C_GOLD);
      canvas.drawString(tb, ix + iw - 12, iy + 14);
    } else {
      canvas.setTextColor(C_CYAN);
      canvas.drawString(String(wTemp) + " C", ix + iw - 12, iy + 14);
    }
  } else {
    int iw = 356, ih = 84, ix = 62, iy = 6;
    canvas.fillSmoothRoundRect(ix + 2, iy + 5, iw, ih, 24, makeRgb(10, 14, 24));
    canvas.fillSmoothRoundRect(ix, iy, iw, ih, 24, C_DARK_PILL);
    canvas.drawRoundRect(ix, iy, iw, ih, 24, C_CYAN);
    canvas.setFont(&fonts::FreeSerifBold12pt7b);
    canvas.setTextDatum(lgfx::middle_left);
    canvas.setTextColor(C_WHITE);
    canvas.drawString("cuteOS Live Hub", ix + 18, iy + 24);
    canvas.setFont(&fonts::FreeSerif9pt7b);
    canvas.setTextDatum(lgfx::middle_right);
    canvas.setTextColor(wifiOnline ? C_GREEN : C_GOLD);
    canvas.drawString(wifiOnline ? (String(WIFI_SSID) + " Online") : String("Offline"), ix + iw - 18, iy + 24);

    canvas.fillSmoothRoundRect(ix + 14,  iy + 46, 76, 28, 14, C_ROSE);
    canvas.fillSmoothRoundRect(ix + 98,  iy + 46, 76, 28, 14, C_BLUE);
    canvas.fillSmoothRoundRect(ix + 182, iy + 46, 76, 28, 14, C_GOLD);
    canvas.fillSmoothRoundRect(ix + 266, iy + 46, 76, 28, 14, makeRgb(56, 64, 82));

    canvas.setFont(&fonts::FreeSerifBold9pt7b);
    canvas.setTextDatum(lgfx::middle_center);
    canvas.setTextColor(C_WHITE);     canvas.drawString("News",   ix + 52,  iy + 60);
    canvas.drawString("Map",          ix + 136, iy + 60);
    canvas.setTextColor(C_DARK_PILL); canvas.drawString("Bright", ix + 220, iy + 60);
    canvas.setTextColor(C_WHITE);     canvas.drawString("Close",  ix + 304, iy + 60);
  }
}

void drawSubHeader(const char* title, const char* backLabel, const char* rightBtn, uint16_t accent) {
  drawCleanCard(12, 6, 94, 28, 14, C_WHITE, C_BORDER);
  canvas.setFont(&fonts::FreeSerifBold9pt7b);
  canvas.setTextDatum(lgfx::middle_center);
  canvas.setTextColor(C_INK);
  canvas.drawString(backLabel, 59, 20);

  canvas.fillSmoothRoundRect(124, 6, 232, 28, 14, C_DARK_PILL);
  canvas.drawRoundRect(124, 6, 232, 28, 14, accent);
  canvas.setTextColor(C_WHITE);
  canvas.drawString(title, 240, 20);

  if (strlen(rightBtn) > 0) {
    drawCleanCard(372, 6, 96, 28, 14, accent, accent);
    canvas.setTextColor(C_WHITE);
    canvas.drawString(rightBtn, 420, 20);
  }
}

void drawModularWidget(int x, int y, int w, int h, int wType) {
  if (wType == W_WEATHER) {
    drawCleanCard(x, y, w, h, 22, C_WHITE, C_BORDER);
    drawWeatherMiniGraphic(x + 36, y + h / 2 - 4, wCode);
    canvas.setFont(&fonts::FreeSerifBold18pt7b);
    canvas.setTextDatum(lgfx::middle_left);
    canvas.setTextColor(C_INK);
    canvas.drawString(String(wTemp) + " C", x + 66, y + h / 2 - 12);
    canvas.setFont(&fonts::FreeSerif9pt7b);
    canvas.setTextColor(C_BLUE);
    canvas.drawString(wCond.substring(0, 14), x + 66, y + h / 2 + 16);
    canvas.setTextDatum(lgfx::middle_right);
    canvas.setTextColor(C_ORANGE);
    canvas.drawString(String(fHigh[0]) + "/" + String(fLow[0]), x + w - 12, y + 20);
  } else if (wType == W_MINIMAP) {
    drawCleanCard(x, y, w, h, 22, C_WHITE, C_WHITE);
    canvas.setClipRect(x + 4, y + 4, w - 8, h - 8);
    mapSprite.pushSprite(&canvas, x - 240 + w / 2, y - 160 + h / 2);
    canvas.clearClipRect();
    canvas.drawRoundRect(x + 3, y + 3, w - 6, h - 6, 19, C_WHITE);
    int px = x + w / 2, py = y + h / 2 - 6;
    canvas.fillCircle(px, py, 10, blend565(C_RED, C_WHITE, 120));
    canvas.fillCircle(px, py, 6, C_RED);
    canvas.fillCircle(px, py, 2, C_WHITE);
    canvas.fillSmoothRoundRect(x + 10, y + h - 24, w - 20, 18, 9, C_WHITE);
    canvas.setFont(&fonts::FreeSerifBold9pt7b);
    canvas.setTextDatum(lgfx::middle_center);
    canvas.setTextColor(C_INK);
    canvas.drawString(SECRET_MAP_WIDGET_LBL, x + w / 2, y + h - 15);
  } else if (wType == W_NEWS) {
    drawCleanCard(x, y, w, h, 22, C_WHITE, C_BORDER);
    blitScaledNewsImg(0, x + 6, y + 6, 72, h - 12);
    canvas.drawRoundRect(x + 6, y + 6, 72, h - 12, 8, C_WHITE);
    canvas.setFont(&fonts::FreeSerifBold9pt7b);
    canvas.setTextDatum(lgfx::top_left);
    canvas.setTextColor(C_ROSE);
    canvas.drawString("LIVE NEWS", x + 84, y + 8);
    drawWrappedText(newsItems[0].title, x + 84, y + 24, w - 92, 3, 17, C_INK, 0);
  } else if (wType == W_RADAR) {
    drawCleanCard(x, y, w, h, 22, C_TINT_GREEN, C_GREEN);
    canvas.setFont(&fonts::FreeSerifBold9pt7b);
    canvas.setTextDatum(lgfx::middle_left);
    canvas.setTextColor(C_GREEN);
    canvas.drawString("Wi-Fi RF Radar", x + 14, y + 18);
    for (int i = 1; i <= 11; i++) {
      int bh = constrain(chStrength[i] / 2, 5, h - 38);
      canvas.fillRoundRect(x + 14 + (i - 1) * 17, y + h - 10 - bh, 11, bh, 4, (bh > 24) ? C_RED : C_GREEN);
    }
  } else if (wType == W_GPIO) {
    drawCleanCard(x, y, w, h, 22, C_WHITE, C_BORDER);
    canvas.setFont(&fonts::FreeSerifBold9pt7b);
    canvas.setTextDatum(lgfx::middle_left);
    canvas.setTextColor(C_INK);
    canvas.drawString("GPIO 5 / 6 / 7", x + 14, y + 18);
    for (int i = 0; i < 3; i++) {
      int bx = x + 12 + i * 64;
      canvas.fillSmoothRoundRect(bx, y + 32, 56, h - 42, 14, gpioStates[i] ? C_GREEN : C_SOFT_GRAY);
      canvas.setTextDatum(lgfx::middle_center);
      canvas.setTextColor(gpioStates[i] ? C_WHITE : C_INK);
      canvas.drawString(String("IO") + String(OUT_GPIOS[i]), bx + 28, y + 32 + (h - 42) / 2);
    }
  } else if (wType == W_TIMER) {
    drawCleanCard(x, y, w, h, 22, C_TINT_ROSE, C_ROSE);
    char tb[10];
    snprintf(tb, sizeof(tb), "%02d:%02d", timerRemainSec / 60, timerRemainSec % 60);
    canvas.setFont(&fonts::FreeSerifBold18pt7b);
    canvas.setTextDatum(lgfx::middle_center);
    canvas.setTextColor(C_INK);
    canvas.drawString(tb, x + w / 2, y + h / 2 - 6);
    canvas.setFont(&fonts::FreeSerif9pt7b);
    canvas.setTextColor(C_ROSE);
    canvas.drawString(timerRunning ? "Focus Active" : "Tap Timer", x + w / 2, y + h - 14);
  } else {
    drawCleanCard(x, y, w, h, 22, C_TINT_BLUE, C_BLUE);
    canvas.setFont(&fonts::FreeSerifBold9pt7b);
    canvas.setTextDatum(lgfx::middle_left);
    canvas.setTextColor(C_BLUE);
    canvas.drawString("ESP32-S3 Core", x + 14, y + 18);
    canvas.setTextColor(C_INK);
    float psMb = ESP.getFreePsram() / (1024.0f * 1024.0f);
    canvas.drawString(String("Temp: ") + String(temperatureRead(), 1) + " C", x + 14, y + 42);
    canvas.drawString(String("PSRAM: ") + String(psMb, 1) + " MB Free", x + 14, y + 64);
  }
}

// ============================================================================
// 12. SCREEN RENDERERS (HOME, 5-TILE LIVE NEWS, READER, BRIGHTNESS, APPS)
// ============================================================================
void drawHomeHeroCard() {
  drawFrostGlassCard(12, 42, 232, 184, 24, false, C_WHITE);
  String dispTime = liveTime;
  if (!ntpSynced) {
    unsigned long s = millis() / 1000;
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", 12 + (int)((s / 3600) % 12), (int)((s / 60) % 60));
    dispTime = String(buf);
  }
  canvas.setFont(&fonts::FreeSerifBold24pt7b);
  canvas.setTextDatum(lgfx::middle_center);
  canvas.setTextColor(C_INK);
  canvas.drawString(dispTime, 128, 88);
  canvas.setFont(&fonts::FreeSerifBold9pt7b);
  canvas.setTextColor(C_BLUE);
  canvas.drawString(liveDate, 128, 128);

  canvas.fillSmoothRoundRect(22, 150, 212, 64, 18, C_WHITE);
  canvas.drawRoundRect(22, 150, 212, 64, 18, C_BORDER);
  canvas.fillCircle(42, 182, 12, C_TINT_ROSE);
  canvas.fillCircle(42, 180, 5, C_RED);
  canvas.setTextDatum(lgfx::middle_left);
  canvas.setTextColor(C_INK);
  canvas.drawString(AREA_NAME, 60, 171);
  canvas.setFont(&fonts::FreeSerif9pt7b);
  canvas.setTextColor(C_INK_SUB);
  canvas.drawString(CITY_NAME, 60, 194);
}

void drawHomeScreen() {
  drawCleanCard(12, 6, 114, 28, 14, C_ROSE, C_ROSE);
  canvas.setFont(&fonts::FreeSerifBold9pt7b);
  canvas.setTextDatum(lgfx::middle_center);
  canvas.setTextColor(C_WHITE);
  canvas.drawString("Live News >", 69, 20);

  drawCleanCard(354, 6, 114, 28, 14, C_VIOLET, C_VIOLET);
  canvas.drawString("+ Widgets", 411, 20);

  drawDynamicIsland();
  drawHomeHeroCard();
  drawModularWidget(254, 42,  214, 92, homeSlot[0]);
  drawModularWidget(254, 142, 214, 84, homeSlot[1]);

  drawFrostGlassCard(22, 234, 436, 76, 26, false, C_WHITE);
  const int dockX[5] = {72, 156, 240, 324, 408};
  drawIconNews(dockX[0],    272, 54);
  drawIconMap(dockX[1],     272, 54);
  drawIconWeather(dockX[2], 272, 54);
  drawIconControl(dockX[3], 272, 54);
  drawIconApps(dockX[4],    272, 54);
}

void drawNewsPanelScreen() {
  drawSubHeader("LIVE NEWS FEED", "< Home", "Refresh", C_ROSE);
  for (int i = 0; i < NUM_NEWS_TOPICS; i++) {
    int tx = 12 + i * 92;
    bool sel = (i == newsTopicIdx);
    canvas.fillSmoothRoundRect(tx, 39, 86, 24, 12, sel ? C_ROSE : C_WHITE);
    canvas.drawRoundRect(tx, 39, 86, 24, 12, sel ? C_WHITE : C_BORDER);
    canvas.setFont(&fonts::FreeSerifBold9pt7b);
    canvas.setTextDatum(lgfx::middle_center);
    canvas.setTextColor(sel ? C_WHITE : C_INK);
    canvas.drawString(TOPIC_NAMES[i], tx + 43, 51);
  }

  // Top Row: 2 Featured Full-Photo News Tiles (Stories 1 & 2)
  for (int i = 0; i < 2; i++) {
    int bx = 12 + i * 232, by = 68, bw = 224, bh = 118;
    drawCleanCard(bx, by, bw, bh, 18, C_DARK_PILL, C_WHITE);
    blitScaledNewsImg(i, bx + 2, by + 2, bw - 4, bh - 4);
    restoreRoundedCorners(bx, by, bw, bh, 16);
    for (int sy = by + 68; sy < by + bh - 2; sy += 2)
      for (int sx = bx + 4; sx < bx + bw - 4; sx += 4)
        canvas.fillRect(sx, sy, 4, 2, blend565(C_DARK_PILL, canvas.readPixel(sx, sy), 205));
    canvas.drawRoundRect(bx, by, bw, bh, 16, C_WHITE);
    canvas.fillSmoothRoundRect(bx + 8, by + 8, 24, 18, 9, C_ROSE);
    canvas.setFont(&fonts::FreeSerifBold9pt7b);
    canvas.setTextDatum(lgfx::middle_center);
    canvas.setTextColor(C_WHITE);
    canvas.drawString(String(i + 1), bx + 20, by + 17);
    drawWrappedText(newsItems[i].title, bx + 8, by + 72, bw - 16, 2, 20, C_WHITE, 1);
  }

  // Bottom Row: 3 Photo + Headline News Tiles (Stories 3, 4 & 5)
  for (int i = 0; i < 3; i++) {
    int idx = i + 2;
    int bx = 12 + i * 154, by = 192, bw = 148, bh = 120;
    drawCleanCard(bx, by, bw, bh, 16, C_WHITE, C_WHITE);
    blitScaledNewsImg(idx, bx + 3, by + 3, bw - 6, 66);
    canvas.drawRoundRect(bx, by, bw, bh, 16, C_WHITE);
    canvas.fillSmoothRoundRect(bx + 7, by + 7, 22, 16, 8, C_BLUE);
    canvas.setFont(&fonts::FreeSerifBold9pt7b);
    canvas.setTextDatum(lgfx::middle_center);
    canvas.setTextColor(C_WHITE);
    canvas.drawString(String(idx + 1), bx + 18, by + 15);
    drawWrappedText(newsItems[idx].title, bx + 7, by + 73, bw - 14, 2, 19, C_INK, 1);
  }
}

void drawNewsArticleScreen() {
  char hdr[28];
  snprintf(hdr, sizeof(hdr), "%s NEWS (%d/5)", TOPIC_NAMES[newsTopicIdx], selectedArticle + 1);
  drawSubHeader(hdr, "< News", "Next >", C_ROSE);

  drawCleanCard(12, 42, 456, 268, 22, C_WHITE, C_BORDER);
  canvas.fillSmoothRoundRect(22, 52, 228, 130, 14, C_DARK_PILL);
  blitScaledNewsImg(selectedArticle, 24, 54, 224, 126);
  canvas.drawRoundRect(22, 52, 228, 130, 14, C_BORDER);

  canvas.fillSmoothRoundRect(260, 52, 82, 22, 11, C_TINT_ROSE);
  canvas.setFont(&fonts::FreeSerifBold9pt7b);
  canvas.setTextDatum(lgfx::middle_center);
  canvas.setTextColor(C_ROSE);
  canvas.drawString(TOPIC_NAMES[newsTopicIdx], 301, 63);
  drawWrappedText(newsItems[selectedArticle].title, 260, 82, 196, 4, 22, C_INK, 1);
  canvas.drawFastHLine(22, 190, 436, C_SOFT_GRAY);
  drawWrappedText(newsItems[selectedArticle].desc, 24, 198, 432, 3, 22, C_INK_SUB, 0);

  canvas.setFont(&fonts::FreeSerif9pt7b);
  canvas.setTextDatum(lgfx::middle_center);
  canvas.setTextColor(C_BLUE);
  canvas.drawString(newsItems[selectedArticle].pubDate, 240, 290);

  canvas.fillSmoothRoundRect(24,  274, 86, 28, 14, C_SOFT_GRAY);
  canvas.fillSmoothRoundRect(370, 274, 86, 28, 14, C_ROSE);
  canvas.setFont(&fonts::FreeSerifBold9pt7b);
  canvas.setTextColor(C_INK);   canvas.drawString("< Prev", 67, 288);
  canvas.setTextColor(C_WHITE); canvas.drawString("Next >", 413, 288);
}

void drawBrightnessSlider(int x, int y, int w, int h) {
  drawFrostGlassCard(x, y, w, h, 22, true, C_WHITE);
  int fillW = constrain(((screenBrightness - 20) * (w - 8)) / 80, 48, w - 8);
  canvas.fillSmoothRoundRect(x + 4, y + 4, fillW, h - 8, 18, C_GOLD);
  canvas.fillSmoothRoundRect(x + 4, y + 4, fillW, (h - 8) / 2, 16, blend565(C_WHITE, C_GOLD, 90));

  int sx = x + 26, sy = y + h / 2;
  canvas.fillCircle(sx, sy, 6, C_DARK_PILL);
  for (int a = 0; a < 360; a += 45) {
    float rad = a * 0.017453f;
    canvas.drawLine(sx + (int)(cosf(rad) * 8.0f), sy + (int)(sinf(rad) * 8.0f),
                    sx + (int)(cosf(rad) * 12.0f), sy + (int)(sinf(rad) * 12.0f), C_DARK_PILL);
  }
  canvas.setFont(&fonts::FreeSerifBold12pt7b);
  canvas.setTextDatum(lgfx::middle_left);
  canvas.setTextColor(screenBrightness > 48 ? C_DARK_PILL : C_WHITE);
  canvas.drawString("Screen Brightness", x + 48, y + h / 2);
  canvas.setTextDatum(lgfx::middle_right);
  canvas.setTextColor(screenBrightness > 88 ? C_DARK_PILL : C_WHITE);
  canvas.drawString(String(screenBrightness) + "%", x + w - 16, y + h / 2);
}

void drawControlScreen() {
  drawSubHeader("CONTROL CENTER", "< Home", "cuteOS", C_BLUE);
  drawBrightnessSlider(14, 42, 452, 50);

  char rfSub[20];
  snprintf(rfSub, sizeof(rfSub), "%d Found", foundNets);
  String wifiSub = wifiOnline ? (String(WIFI_SSID) + " Live") : String("Reconnect");
  const char* titles[8] = {"Wi-Fi", "Everest BG", SECRET_MAP_TILE_LBL, "RGB Lamp", "RF Scan", "Calib Touch", "GPIO Reset", "Reboot OS"};
  const char* subs[8]   = {
    wifiSub.c_str(),
    everestLoaded ? "Real Photo" : "Fetch Photo",
    mapSatellite ? "Satellite" : "Street View",
    rgbBreathing ? "Breathing" : "Tap Cycle",
    rfSub, "Save Flash", "All LOW", "Restart"
  };
  uint16_t bgs[8] = {wifiOnline ? C_BLUE : C_WHITE, everestLoaded ? C_TEAL : C_WHITE, C_GREEN, C_ROSE, C_VIOLET, C_ORANGE, C_WHITE, C_RED};
  bool darkText[8] = {!wifiOnline, !everestLoaded, false, false, false, false, true, false};

  for (int i = 0; i < 8; i++) {
    int c = i % 4, r = i / 4, x = 14 + c * 115, y = 104 + r * 106;
    drawCleanCard(x, y, 106, 96, 22, bgs[i], darkText[i] ? C_BORDER : C_WHITE);
    canvas.fillCircle(x + 53, y + 26, 13, darkText[i] ? C_TINT_BLUE : blend565(C_WHITE, bgs[i], 75));
    canvas.fillCircle(x + 53, y + 26, 5,  darkText[i] ? C_BLUE : C_WHITE);
    canvas.setFont(&fonts::FreeSerifBold9pt7b);
    canvas.setTextDatum(lgfx::middle_center);
    canvas.setTextColor(darkText[i] ? C_INK : C_WHITE);
    canvas.drawString(titles[i], x + 53, y + 58);
    canvas.setFont(&fonts::FreeSerif9pt7b);
    canvas.setTextColor(darkText[i] ? C_INK_SUB : C_SNOW);
    canvas.drawString(subs[i], x + 53, y + 79);
  }
}

void drawAppsScreen() {
  drawSubHeader("cuteOS APPS", "< Home", "+ Widgets", C_VIOLET);
  drawFrostGlassCard(12, 42, 456, 268, 26, false, C_WHITE);
  const char* names[] = {"Live News", "Map", "Weather", "RF Radar", "Scope", "RGB & IO",
                         "Calculator", "Timer", "Calendar", "Photos", "Phone", "World Clock"};
  const char* symbols[] = {"CAL", "JPG", "BLE", "UTC"};
  for (int i = 0; i < 12; ++i) {
    int cx = 74 + (i % 4) * 111, cy = 75 + (i / 4) * 86;
    switch (i) {
      case 0: drawIconNews(cx, cy, 46); break;
      case 1: drawIconMap(cx, cy, 46); break;
      case 2: drawIconWeather(cx, cy, 46); break;
      case 3: drawIconRadar(cx, cy, 46); break;
      case 4: drawIconScope(cx, cy, 46); break;
      case 5: drawIconRGB(cx, cy, 46); break;
      case 6: drawIconCalc(cx, cy, 46); break;
      case 7: drawIconTimer(cx, cy, 46); break;
      default:
        drawAppleSquircle(cx, cy, 46, C_VIOLET, C_BLUE);
        canvas.setFont(&fonts::FreeSerifBold9pt7b);
        canvas.setTextDatum(lgfx::middle_center);
        canvas.setTextColor(C_WHITE);
        canvas.drawString(symbols[i - 8], cx, cy);
    }
    canvas.setFont(&fonts::FreeSerifBold9pt7b);
    canvas.setTextDatum(lgfx::middle_center);
    canvas.setTextColor(C_INK);
    canvas.drawString(names[i], cx, cy + 34);
  }
}

void drawTilesScreen() {
  char titleBuf[28];
  if (currentTile == 3) snprintf(titleBuf, sizeof(titleBuf), "NOTIFICATIONS");
  else                  snprintf(titleBuf, sizeof(titleBuf), "TECH TILE %d / %d", currentTile + 1, NUM_TILES);
  drawSubHeader(titleBuf, "< Home", (currentTile == 3) ? "Clear All" : "Next >", C_BLUE);

  if (currentTile == 0) {
    drawModularWidget(14, 44, 240, 242, W_MINIMAP);
    drawCleanCard(264, 44, 202, 242, 24, C_WHITE, C_BORDER);
    canvas.setFont(&fonts::FreeSerifBold9pt7b);
    canvas.setTextDatum(lgfx::middle_center);
    canvas.setTextColor(C_BLUE); canvas.drawString(CITY_NAME, 365, 70);
    drawWeatherMiniGraphic(365, 114, wCode);
    canvas.setFont(&fonts::FreeSerifBold24pt7b);
    canvas.setTextColor(C_INK);  canvas.drawString(String(wTemp) + " C", 365, 174);
    canvas.setFont(&fonts::FreeSerif9pt7b);
    canvas.setTextColor(C_INK_SUB); canvas.drawString(wCond, 365, 214);
    canvas.setTextColor(C_ORANGE);  canvas.drawString(String("H:") + String(fHigh[0]) + "  L:" + String(fLow[0]) + " C", 365, 248);
  } else if (currentTile == 1) {
    drawCleanCard(14, 44, 452, 242, 24, C_WHITE, C_BORDER);
    canvas.setFont(&fonts::FreeSerifBold12pt7b);
    canvas.setTextDatum(lgfx::middle_left);  canvas.setTextColor(C_INK);   canvas.drawString("2.4 GHz Wi-Fi RF Spectrum", 30, 72);
    canvas.setFont(&fonts::FreeSerifBold9pt7b);
    canvas.setTextDatum(lgfx::middle_right); canvas.setTextColor(C_GREEN); canvas.drawString(String(foundNets) + " Networks", 448, 72);
    uint16_t barCols[5] = {C_BLUE, C_TEAL, C_GREEN, C_ORANGE, C_ROSE};
    for (int ch = 1; ch <= 13; ch++) {
      int bh = constrain(chStrength[ch] * 2, 8, 150), bx = 28 + (ch - 1) * 33;
      canvas.fillSmoothRoundRect(bx, 250 - bh, 22, bh, 8, barCols[(ch - 1) % 5]);
      canvas.setFont(&fonts::FreeSerif9pt7b);
      canvas.setTextDatum(lgfx::middle_center); canvas.setTextColor(C_INK_SUB); canvas.drawString(String(ch), bx + 11, 266);
    }
  } else if (currentTile == 2) {
    drawCleanCard(14, 44, 452, 108, 24, C_WHITE, C_BORDER);
    canvas.setFont(&fonts::FreeSerifBold12pt7b);
    canvas.setTextDatum(lgfx::middle_left); canvas.setTextColor(C_BLUE); canvas.drawString("ESP32-S3-N16R8 Engine", 30, 70);
    canvas.setFont(&fonts::FreeSerif9pt7b); canvas.setTextColor(C_INK);
    float psFree = ESP.getFreePsram() / (1024.0f * 1024.0f);
    canvas.drawString(String("Dual LX7 240MHz | Core Temp: ") + String(temperatureRead(), 1) + " C", 30, 102);
    canvas.drawString(String("OPI PSRAM Free: ") + String(psFree, 2) + " MB | Heap: " + String(ESP.getFreeHeap() / 1024) + " KB", 30, 128);
    for (int i = 0; i < 3; i++) {
      int bx = 14 + i * 154;
      drawCleanCard(bx, 162, 144, 124, 24, gpioStates[i] ? C_GREEN : C_WHITE, gpioStates[i] ? C_GREEN : C_BORDER);
      canvas.setFont(&fonts::FreeSerifBold12pt7b);
      canvas.setTextDatum(lgfx::middle_center);
      canvas.setTextColor(gpioStates[i] ? C_WHITE : C_INK); canvas.drawString(String("GPIO ") + String(OUT_GPIOS[i]), bx + 72, 206);
      canvas.setFont(&fonts::FreeSerifBold9pt7b);
      canvas.setTextColor(gpioStates[i] ? C_TINT_GOLD : C_INK_SUB); canvas.drawString(gpioStates[i] ? "3.3V HIGH" : "0V LOW", bx + 72, 244);
    }
  } else {
    int drawn = 0;
    for (int i = 0; i < MAX_NOTIFS && drawn < 4; i++) {
      if (!notifs[i].active) continue;
      int y = 44 + drawn * 61;
      drawCleanCard(14, y, 452, 54, 18, C_WHITE, C_BORDER);
      canvas.fillCircle(42, y + 27, 14, notifs[i].col);
      canvas.setFont(&fonts::FreeSerifBold9pt7b);
      canvas.setTextDatum(lgfx::middle_center); canvas.setTextColor(C_WHITE); canvas.drawString(notifs[i].app.substring(0, 1), 42, y + 27);
      canvas.setTextDatum(lgfx::middle_left);   canvas.setTextColor(C_INK);   canvas.drawString(notifs[i].title.substring(0, 30), 66, y + 18);
      canvas.setFont(&fonts::FreeSerif9pt7b);   canvas.setTextColor(C_INK_SUB); canvas.drawString(notifs[i].body.substring(0, 38), 66, y + 38);
      drawn++;
    }
    if (drawn == 0) {
      drawCleanCard(70, 108, 340, 100, 24, C_WHITE, C_BORDER);
      canvas.setFont(&fonts::FreeSerifBold18pt7b);
      canvas.setTextDatum(lgfx::middle_center); canvas.setTextColor(C_INK); canvas.drawString("All Caught Up", 240, 158);
    }
  }

  canvas.fillSmoothRoundRect(194, 294, 92, 18, 9, C_DARK_PILL);
  for (int i = 0; i < NUM_TILES; i++) {
    int dx = 212 + i * 18;
    if (i == currentTile) canvas.fillRoundRect(dx - 7, 300, 14, 6, 3, C_CYAN);
    else                  canvas.fillCircle(dx, 303, 3, makeRgb(120, 128, 142));
  }
}

void drawWidgetsScreen() {
  drawSubHeader("CUSTOMIZE WIDGETS", "< Home", editSlotIndex == 0 ? "Slot: TOP" : "Slot: BOT", C_VIOLET);
  const char* names[NUM_WIDGETS] = {"Weather", "Estate Map", "Live News", "RF Radar", "GPIO 5-7", "Timer", "ESP32 Core"};
  uint16_t cols[NUM_WIDGETS]     = {C_BLUE, C_GREEN, C_ROSE, C_TEAL, C_ORANGE, C_RED, C_VIOLET};
  for (int i = 0; i < NUM_WIDGETS; i++) {
    int x = 14 + (i % 4) * 115, y = 48 + (i / 4) * 128;
    bool sel = (homeSlot[editSlotIndex] == i);
    drawCleanCard(x, y, 106, 114, 22, sel ? cols[i] : C_WHITE, sel ? C_WHITE : C_BORDER);
    canvas.setFont(&fonts::FreeSerifBold9pt7b);
    canvas.setTextDatum(lgfx::middle_center);
    canvas.setTextColor(sel ? C_WHITE : C_INK); canvas.drawString(names[i], x + 53, y + 48);
    canvas.setFont(&fonts::FreeSerif9pt7b);
    canvas.setTextColor(sel ? C_TINT_GOLD : cols[i]); canvas.drawString(sel ? "Active" : "Tap Add", x + 53, y + 80);
  }
  int x = 14 + 3 * 115, y = 48 + 128;
  drawCleanCard(x, y, 106, 114, 22, C_DARK_PILL, C_CYAN);
  canvas.setFont(&fonts::FreeSerifBold9pt7b);
  canvas.setTextDatum(lgfx::middle_center);
  canvas.setTextColor(C_WHITE); canvas.drawString("Switch Slot", x + 53, y + 46);
  canvas.setTextColor(C_GOLD);  canvas.drawString(editSlotIndex == 0 ? "Top Slot" : "Bot Slot", x + 53, y + 78);
}

void drawAppMap() {
  canvas.fillScreen(C_SOFT_GRAY);
  mapSprite.pushSprite(&canvas, draggingMap ? lastMapPreviewX : 0,
                       draggingMap ? lastMapPreviewY : 0);
  // Rounded glass controls with the map visible through the tint.
  const int starts[] = {12, 179, 354};
  const int widths[] = {88, 122, 114};
  const char* labels[] = {"< Home", "Hyderabad", mapSatellite ? "Street View" : "Satellite"};
  canvas.setFont(&fonts::FreeSerifBold9pt7b);
  canvas.setTextDatum(lgfx::middle_center);
  for (int i = 0; i < 3; ++i) {
    for (int y = 8; y < 38; ++y) {
      int dy = abs(y - 23);
      int inset = dy < 14 ? 14 - (int)sqrtf(196 - dy * dy) : 14;
      for (int x = starts[i] + inset; x < starts[i] + widths[i] - inset; ++x) {
        canvas.drawPixel(x, y, blend565(C_DARK_PILL, canvas.readPixel(x, y), 180));
      }
    }
    canvas.drawRoundRect(starts[i], 8, widths[i], 30, 14, C_WHITE);
    canvas.setTextColor(C_WHITE);
    canvas.drawString(labels[i], starts[i] + widths[i] / 2, 23);
  }
  drawCleanCard(13, 279, 454, 28, 14, C_DARK_PILL, C_WHITE);
  canvas.setTextColor(C_WHITE);
  canvas.drawString(!mapLoaded ? "Offline map" : (mapSatellite ? "Esri / Maxar / Earthstar Geographics" :
                    "Map data: OpenStreetMap contributors"), 240, 293);
}

void drawAppWeather() {
  drawSubHeader(SECRET_WEATHER_TITLE, "< Apps", "Refresh", C_BLUE);
  drawCleanCard(14, 44, 452, 126, 24, C_WHITE, C_BORDER);
  drawWeatherMiniGraphic(68, 106, wCode);
  canvas.setFont(&fonts::FreeSerifBold24pt7b);
  canvas.setTextDatum(lgfx::middle_left); canvas.setTextColor(C_INK);  canvas.drawString(String(wTemp) + " C", 112, 92);
  canvas.setFont(&fonts::FreeSerifBold12pt7b); canvas.setTextColor(C_BLUE); canvas.drawString(wCond, 112, 136);
  canvas.setFont(&fonts::FreeSerifBold9pt7b);
  canvas.setTextDatum(lgfx::middle_right); canvas.setTextColor(C_INK); canvas.drawString(CITY_NAME, 448, 74);
  canvas.setFont(&fonts::FreeSerif9pt7b);
  canvas.setTextColor(C_TEAL);   canvas.drawString(String("Humidity: ") + String(wHum) + "%", 448, 106);
  canvas.setTextColor(C_ORANGE); canvas.drawString(String("Wind: ") + String(wWind) + " km/h", 448, 136);

  uint16_t tints[3] = {C_TINT_BLUE, C_TINT_GOLD, C_TINT_GREEN}, bCols[3] = {C_BLUE, C_ORANGE, C_GREEN};
  for (int i = 0; i < 3; i++) {
    int bx = 14 + i * 154;
    drawCleanCard(bx, 180, 144, 128, 22, tints[i], bCols[i]);
    canvas.setFont(&fonts::FreeSerifBold12pt7b);
    canvas.setTextDatum(lgfx::middle_center); canvas.setTextColor(bCols[i]); canvas.drawString(fDay[i], bx + 72, 202);
    drawWeatherMiniGraphic(bx + 72, 242, fCode[i]);
    canvas.setFont(&fonts::FreeSerifBold9pt7b); canvas.setTextColor(C_INK); canvas.drawString(String(fHigh[i]) + "C / " + String(fLow[i]) + "C", bx + 72, 286);
  }
}

void drawAppRadar() {
  drawSubHeader("WI-FI RF RADAR", "< Apps", "Scan RF", C_GREEN);
  drawCleanCard(14, 44, 452, 134, 22, C_WHITE, C_BORDER);
  uint16_t cols[5] = {C_BLUE, C_TEAL, C_GREEN, C_ORANGE, C_ROSE};
  for (int ch = 1; ch <= 13; ch++) {
    int bh = constrain(chStrength[ch], 6, 95), bx = 26 + (ch - 1) * 33;
    canvas.fillSmoothRoundRect(bx, 152 - bh, 22, bh, 7, cols[(ch - 1) % 5]);
    canvas.setFont(&fonts::FreeSerif9pt7b);
    canvas.setTextDatum(lgfx::middle_center); canvas.setTextColor(C_INK_SUB); canvas.drawString(String(ch), bx + 11, 166);
  }
  drawCleanCard(14, 188, 452, 122, 22, C_WHITE, C_BORDER);
  for (int i = 0; i < 4; i++) {
    int y = 208 + i * 26;
    if (topSSIDs[i].length() > 0) {
      canvas.setFont(&fonts::FreeSerifBold9pt7b);
      canvas.setTextDatum(lgfx::middle_left); canvas.setTextColor(C_INK); canvas.drawString(topSSIDs[i].substring(0, 22), 28, y);
      if (topRSSI[i] != 0) {
        canvas.setTextDatum(lgfx::middle_right);
        canvas.setTextColor(topRSSI[i] > -68 ? C_GREEN : C_ORANGE);
        canvas.drawString(String("Ch ") + String(topCh[i]) + " | " + String(topRSSI[i]) + " dBm", 450, y);
      }
    }
  }
}

void drawScopeBodyOnly() {
  drawCleanCard(14, 44, 452, 206, 22, C_DARK_PILL, C_CYAN);
  for (int gx = 54; gx < 450; gx += 48) canvas.drawFastVLine(gx, 56, 182, makeRgb(38, 46, 66));
  for (int gy = 74; gy < 236; gy += 40) canvas.drawFastHLine(24, gy, 432, makeRgb(38, 46, 66));
  for (int i = 1; i < SCOPE_PTS; i++) {
    int x1 = 24 + ((i - 1) * 432) / (SCOPE_PTS - 1), x2 = 24 + (i * 432) / (SCOPE_PTS - 1);
    int y1 = 236 - (scopeBuf[i - 1] * 174) / 4095,   y2 = 236 - (scopeBuf[i] * 174) / 4095;
    drawThickLine(x1, y1, x2, y2, 2, C_GREEN);
  }
  drawCleanCard(14, 258, 452, 52, 20, C_WHITE, C_BORDER);
  canvas.setFont(&fonts::FreeSerifBold12pt7b);
  canvas.setTextDatum(lgfx::middle_left);  canvas.setTextColor(C_ORANGE); canvas.drawString(String("GPIO 4: ") + String(scopeLastVolt, 2) + " V", 30, 284);
  canvas.setTextDatum(lgfx::middle_right); canvas.setTextColor(C_BLUE);   canvas.drawString(String("Core: ") + String(temperatureRead(), 1) + " C", 450, 284);
}

void drawAppScope() {
  drawSubHeader("ADC OSCILLOSCOPE", "< Apps", scopePaused ? "Resume" : "Hold", C_ORANGE);
  drawScopeBodyOnly();
}

void drawAppRGB() {
  drawSubHeader("RGB & GPIO STUDIO", "< Apps", rgbBreathing ? "Solid" : "Breathe", C_ROSE);
  drawCleanCard(14, 44, 452, 128, 24, C_WHITE, C_BORDER);
  canvas.setFont(&fonts::FreeSerifBold12pt7b);
  canvas.setTextDatum(lgfx::middle_left); canvas.setTextColor(C_INK); canvas.drawString("Onboard WS2812 RGB (GPIO 48)", 30, 70);
  uint16_t swatches[7] = {C_DARK_PILL, C_BLUE, C_GREEN, C_GOLD, C_ORANGE, C_ROSE, C_VIOLET};
  for (int i = 0; i < 7; i++) {
    int cx = 52 + i * 62;
    canvas.fillCircle(cx, 126, 22, swatches[i]);
    if (rgbPreset == i) canvas.drawCircle(cx, 126, 26, C_INK);
    if (i == 0) { canvas.setFont(&fonts::FreeSerifBold9pt7b); canvas.setTextDatum(lgfx::middle_center); canvas.setTextColor(C_WHITE); canvas.drawString("OFF", cx, 126); }
  }
  for (int i = 0; i < 3; i++) {
    int bx = 14 + i * 154;
    drawCleanCard(bx, 184, 144, 124, 24, gpioStates[i] ? C_GREEN : C_WHITE, gpioStates[i] ? C_GREEN : C_BORDER);
    canvas.setFont(&fonts::FreeSerifBold12pt7b);
    canvas.setTextDatum(lgfx::middle_center);
    canvas.setTextColor(gpioStates[i] ? C_WHITE : C_INK); canvas.drawString(String("GPIO ") + String(OUT_GPIOS[i]), bx + 72, 230);
    canvas.setFont(&fonts::FreeSerifBold9pt7b);
    canvas.setTextColor(gpioStates[i] ? C_TINT_GOLD : C_INK_SUB); canvas.drawString(gpioStates[i] ? "3.3V ON" : "0V OFF", bx + 72, 268);
  }
}

void drawAppCalc() {
  drawSubHeader("CALCULATOR", "< Apps", "Clear", C_TEAL);
  drawCleanCard(14, 42, 452, 56, 20, C_WHITE, C_BORDER);
  canvas.setFont(&fonts::FreeSerifBold18pt7b);
  canvas.setTextDatum(lgfx::middle_right); canvas.setTextColor(C_INK); canvas.drawString(calcDisplay.substring(0, 16), 448, 70);
  const char* keys[4][4] = {{"7","8","9","/"},{"4","5","6","*"},{"1","2","3","-"},{"0",".","=","+"}};
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      int x = 14 + c * 115, y = 106 + r * 52;
      uint16_t bg = C_WHITE, fg = C_INK;
      if (c == 3) { bg = C_TINT_VIOLET; fg = C_VIOLET; }
      if (r == 3 && c == 2) { bg = C_BLUE; fg = C_WHITE; }
      drawCleanCard(x, y, 106, 44, 18, bg, (bg == C_WHITE) ? C_BORDER : bg);
      canvas.setFont(&fonts::FreeSerifBold12pt7b);
      canvas.setTextDatum(lgfx::middle_center); canvas.setTextColor(fg); canvas.drawString(keys[r][c], x + 53, y + 22);
    }
  }
}

void drawAppTimer() {
  drawSubHeader("FOCUS TIMER", "< Apps", "Home", C_ROSE);
  drawCleanCard(14, 44, 250, 264, 26, C_WHITE, C_BORDER);
  int pct = (timerRemainSec * 100) / ((timerPresetSec > 0) ? timerPresetSec : 1);
  drawRingGauge(139, 148, 78, 66, pct, C_ROSE, timerRunning ? "Active" : "Ready");
  char tbuf[10];
  snprintf(tbuf, sizeof(tbuf), "%02d:%02d", timerRemainSec / 60, timerRemainSec % 60);
  canvas.setFont(&fonts::FreeSerifBold24pt7b);
  canvas.setTextDatum(lgfx::middle_center); canvas.setTextColor(C_INK); canvas.drawString(tbuf, 139, 262);

  drawCleanCard(276, 44,  190, 68, 22, timerRunning ? C_RED : C_GREEN, timerRunning ? C_RED : C_GREEN);
  drawCleanCard(276, 122, 190, 54, 20, C_WHITE, C_BORDER);
  drawCleanCard(276, 186, 90,  54, 20, C_TINT_BLUE, C_BLUE);
  drawCleanCard(376, 186, 90,  54, 20, C_TINT_GOLD, C_ORANGE);
  drawCleanCard(276, 250, 190, 58, 20, C_TINT_VIOLET, C_VIOLET);

  canvas.setFont(&fonts::FreeSerifBold12pt7b);
  canvas.setTextColor(C_WHITE); canvas.drawString(timerRunning ? "PAUSE" : "START", 371, 78);
  canvas.setTextColor(C_INK);   canvas.drawString("Reset", 371, 149);
  canvas.setFont(&fonts::FreeSerifBold9pt7b);
  canvas.drawString("5 min", 321, 213); canvas.drawString("15 min", 421, 213); canvas.drawString("25 min Pomodoro", 371, 279);
}

void drawToastOverlay() {
  if (millis() < toastEnd && toastMsg.length() > 0) {
    int tw = constrain((int)toastMsg.length() * 10 + 48, 120, 390), tx = (480 - tw) / 2;
    canvas.fillSmoothRoundRect(tx, 268, tw, 36, 18, C_DARK_PILL);
    canvas.drawRoundRect(tx, 268, tw, 36, 18, C_CYAN);
    canvas.setFont(&fonts::FreeSerifBold9pt7b);
    canvas.setTextDatum(lgfx::middle_center); canvas.setTextColor(C_WHITE); canvas.drawString(toastMsg, 240, 286);
  }
}

// ============================================================================
// 13. WEATHER, NTP TIME, RF RADAR & RENDER ENGINE
// ============================================================================
float extractJsonNum(const String& json, const String& key, int startPos) {
  int idx = json.indexOf(key, startPos);
  if (idx < 0) return 0.0f;
  idx += key.length();
  while (idx < (int)json.length() && (json[idx] == ':' || json[idx] == ' ' || json[idx] == '[')) idx++;
  int end = idx;
  while (end < (int)json.length() && (isdigit(json[end]) || json[end] == '.' || json[end] == '-')) end++;
  return json.substring(idx, end).toFloat();
}

void fetchLocalWeather() {
  if (WiFi.status() != WL_CONNECTED) return;
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http;
  String url = String("https://api.open-meteo.com/v1/forecast?latitude=") + String(HOME_LAT, 5) +
               "&longitude=" + String(HOME_LON, 5) +
               "&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m"
               "&daily=weather_code,temperature_2m_max,temperature_2m_min&timezone=auto&forecast_days=4";
  http.setTimeout(5000);
  if (!http.begin(client, url)) return;
  if (http.GET() == 200) {
    String payload = http.getString();
    int curPos = payload.indexOf("\"current\"");
    if (curPos > 0) {
      wTemp = (int)roundf(extractJsonNum(payload, "\"temperature_2m\"", curPos));
      wHum  = (int)roundf(extractJsonNum(payload, "\"relative_humidity_2m\"", curPos));
      wCode = (int)roundf(extractJsonNum(payload, "\"weather_code\"", curPos));
      wWind = (int)roundf(extractJsonNum(payload, "\"wind_speed_10m\"", curPos));
      wCond = wmoToText(wCode);
    }
    int dailyPos = payload.indexOf("\"daily\"");
    if (dailyPos > 0) {
      int maxPos = payload.indexOf("\"temperature_2m_max\":[", dailyPos);
      int minPos = payload.indexOf("\"temperature_2m_min\":[", dailyPos);
      int wcPos  = payload.indexOf("\"weather_code\":[", dailyPos);
      if (maxPos > 0 && minPos > 0 && wcPos > 0) {
        String sMax = payload.substring(maxPos + 22, payload.indexOf(']', maxPos));
        String sMin = payload.substring(minPos + 22, payload.indexOf(']', minPos));
        String sWc  = payload.substring(wcPos + 16,  payload.indexOf(']', wcPos));
        int p1 = 0, p2 = 0, p3 = 0;
        for (int k = 0; k < 4; k++) {
          int c1 = sMax.indexOf(',', p1), c2 = sMin.indexOf(',', p2), c3 = sWc.indexOf(',', p3);
          float vm = sMax.substring(p1, (c1 < 0) ? (int)sMax.length() : c1).toFloat();
          float vn = sMin.substring(p2, (c2 < 0) ? (int)sMin.length() : c2).toFloat();
          int vw   = sWc.substring(p3,  (c3 < 0) ? (int)sWc.length()  : c3).toInt();
          if (k >= 1 && k <= 3) { fHigh[k - 1] = (int)roundf(vm); fLow[k - 1] = (int)roundf(vn); fCode[k - 1] = vw; }
          if (c1 >= 0) p1 = c1 + 1;
          if (c2 >= 0) p2 = c2 + 1;
          if (c3 >= 0) p3 = c3 + 1;
        }
      }
    }
    lastWeatherFetch = millis();
    uiDirty = true;
  }
  http.end();
}

void updateTimeFromNtp() {
  struct tm ti;
  if (getLocalTime(&ti, 10)) {
    ntpSynced = true;
    char tb[8], db[28];
    strftime(tb, sizeof(tb), "%H:%M", &ti);
    strftime(db, sizeof(db), "%a, %b %d %Y", &ti);
    liveTime = String(tb);
    liveDate = String(db);
    const char* days[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    for (int i = 0; i < 3; i++) fDay[i] = String(days[(ti.tm_wday + i + 1) % 7]);
  }
}

void runRFSpectrumScan() {
  showToast("Scanning 2.4 GHz RF...", 1800);
  renderCurrentScreen(true);
  WiFi.mode(WIFI_STA);
  int n = WiFi.scanNetworks(false, false, false, 120);
  foundNets = (n > 0) ? n : 0;
  for (int i = 0; i < 14; i++) chStrength[i] = 0;
  for (int i = 0; i < 4; i++) { topSSIDs[i] = ""; topRSSI[i] = 0; topCh[i] = 0; }
  for (int i = 0; i < foundNets; i++) {
    int ch = constrain(WiFi.channel(i), 1, 13), rssi = WiFi.RSSI(i);
    int strength = constrain(100 + rssi, 8, 95);
    if (strength > chStrength[ch]) chStrength[ch] = strength;
    if (i < 4) { topSSIDs[i] = WiFi.SSID(i); topRSSI[i] = rssi; topCh[i] = ch; }
  }
  if (foundNets == 0) topSSIDs[0] = "No 2.4 GHz networks detected";
  WiFi.scanDelete();
  showToast(String("RF Scan: ") + String(foundNets) + " networks", 1400);
  uiDirty = true;
}

#include "PhonePanels.h"
#include "AtlasPanels.h"
#include "MarketPanels.h"

void renderCurrentScreen(bool pushNow) {
  if (currentScreen != APP_MAP) bgSprite.pushSprite(&canvas, 0, 0);
  switch (currentScreen) {
    case SCR_HOME:    drawHomeScreen();        break;
    case SCR_NEWS:    drawNewsPanelScreen();   break;
    case SCR_ARTICLE: drawNewsArticleScreen(); break;
    case SCR_CONTROL: drawControlScreen();     break;
    case SCR_APPS:    drawAppsScreen();        break;
    case SCR_TILES:   drawTilesScreen();       break;
    case SCR_WIDGETS: drawWidgetsScreen();     break;
    case APP_MAP:     drawAppMap();            break;
    case APP_WEATHER: drawAppWeather();        break;
    case APP_RADAR:   drawAppRadar();          break;
    case APP_SCOPE:   drawAppScope();          break;
    case APP_RGB:     drawAppRGB();            break;
    case APP_CALC:    drawAppCalc();           break;
    case APP_TIMER:   drawAppTimer();          break;
    case SCR_PHONE:   drawPhonePanel();        break;
    case SCR_MUSIC:   drawMusicPanel();        break;
    case SCR_WORLD:   drawWorldClockPanel();   break;
    case APP_CALENDAR: drawCalendarPanel();   break;
    case APP_PHOTOS:   drawPhotosPanel();     break;
    case SCR_MARKETS:  drawMarketsPanel();    break;
  }
  drawToastOverlay();
  if (pushNow) pushFullFrame();
  uiDirty = false;
}

void switchScreen(int next) {
  currentScreen = next;
  if (next == SCR_MARKETS) loadMarketPage();
  islandExpanded = false;
  renderCurrentScreen(true);
}

void runAndSaveTouchCalibration() {
  uint16_t calData[8];
  tft.fillScreen(C_WHITE);
  tft.setFont(&fonts::FreeSerifBold18pt7b);
  tft.setTextDatum(lgfx::middle_center); tft.setTextColor(C_BLUE); tft.drawString("cuteOS Touch Calibration", 240, 128);
  tft.setFont(&fonts::FreeSerif12pt7b);  tft.setTextColor(C_INK);  tft.drawString("Tap each corner arrow gently with stylus", 240, 168);
  tft.calibrateTouch(calData, C_BLUE, C_WHITE, 18);
  prefs.begin("cuteos", false);
  prefs.putBytes("cal", calData, sizeof(calData));
  prefs.end();
  pushNotif("System", "Touch Calibrated", "Exact accuracy saved to Flash", C_GREEN);
  showToast("Touch Saved Permanently!", 1400);
  renderCurrentScreen(true);
}

// ============================================================================
// 14. SMOOTH TOUCH, BRIGHTNESS SLIDER & GESTURE ENGINE
// ============================================================================
bool inRect(int x, int y, int rx, int ry, int rw, int rh) {
  return (x >= rx && x <= rx + rw && y >= ry && y <= ry + rh);
}

bool getFilteredTouch(int& outX, int& outY) {
  uint16_t x0, y0, x1, y1, x2, y2;
  if (!tft.getTouch(&x0, &y0)) return false;
  delayMicroseconds(70);
  if (!tft.getTouch(&x1, &y1)) { outX = x0; outY = y0; return true; }
  delayMicroseconds(70);
  if (!tft.getTouch(&x2, &y2)) { outX = (x0 + x1) / 2; outY = (y0 + y1) / 2; return true; }
  outX = (x0 > x1) ? ((x1 > x2) ? x1 : ((x0 > x2) ? x2 : x0)) : ((x0 > x2) ? x0 : ((x1 > x2) ? x2 : x1));
  outY = (y0 > y1) ? ((y1 > y2) ? y1 : ((y0 > y2) ? y2 : y0)) : ((y0 > y2) ? y0 : ((y1 > y2) ? y2 : y1));
  return true;
}

void handleCalcKey(const char* k) {
  char ch = k[0];
  if ((ch >= '0' && ch <= '9') || ch == '.') {
    if (calcNewInput) { calcDisplay = (ch == '.') ? "0." : String(ch); calcNewInput = false; }
    else if (calcDisplay.length() < 12 && (ch != '.' || calcDisplay.indexOf('.') < 0)) calcDisplay += ch;
  } else if (ch == '+' || ch == '-' || ch == '*' || ch == '/') {
    calcAcc = calcDisplay.toDouble(); calcOp = ch; calcNewInput = true;
  } else if (ch == '=') {
    double b = calcDisplay.toDouble(), res = b;
    if (calcOp == '+') res = calcAcc + b;
    if (calcOp == '-') res = calcAcc - b;
    if (calcOp == '*') res = calcAcc * b;
    if (calcOp == '/') res = (b != 0.0) ? (calcAcc / b) : 0.0;
    calcDisplay = (fabs(res - round(res)) < 0.00001) ? String((long long)round(res)) : String(res, 4);
    calcOp = 0; calcNewInput = true;
  }
  drawCleanCard(14, 42, 452, 56, 20, C_WHITE, C_BORDER);
  canvas.setFont(&fonts::FreeSerifBold18pt7b);
  canvas.setTextDatum(lgfx::middle_right); canvas.setTextColor(C_INK); canvas.drawString(calcDisplay.substring(0, 16), 448, 70);
  pushPartial(14, 42, 452, 56);
}

void handleHomeWidgetTap(int wType, int rx, int wx) {
  if (wType == W_WEATHER)      switchScreen(APP_WEATHER);
  else if (wType == W_MINIMAP) switchScreen(APP_MAP);
  else if (wType == W_NEWS)    switchScreen(SCR_NEWS);
  else if (wType == W_RADAR)   switchScreen(APP_RADAR);
  else if (wType == W_TIMER)   switchScreen(APP_TIMER);
  else if (wType == W_CORE)    switchScreen(SCR_CONTROL);
  else if (wType == W_GPIO) {
    int idx = constrain((rx - wx - 12) / 64, 0, 2);
    gpioStates[idx] = !gpioStates[idx];
    digitalWrite(OUT_GPIOS[idx], gpioStates[idx] ? HIGH : LOW);
    showToast(String("GPIO ") + String(OUT_GPIOS[idx]) + (gpioStates[idx] ? " -> 3.3V" : " -> 0V"), 1100);
    uiDirty = true;
  }
}

void handleTap(int x, int y) {
  if (currentScreen == SCR_HOME) {
    if (islandExpanded) {
      if (inRect(x, y, 62, 6, 356, 84)) {
        if (inRect(x, y, 76,  50, 76, 32))      { switchScreen(SCR_NEWS); return; }
        else if (inRect(x, y, 160, 50, 76, 32)) { switchScreen(APP_MAP); return; }
        else if (inRect(x, y, 244, 50, 76, 32)) { switchScreen(SCR_CONTROL); return; }
      }
      islandExpanded = false; renderCurrentScreen(true); return;
    }
    if (inRect(x, y, 134, 0, 212, 38)) { islandExpanded = true; renderCurrentScreen(true); return; }
    if (inRect(x, y, 6,   0, 122, 38)) { switchScreen(SCR_NEWS); return; }
    if (inRect(x, y, 350, 0, 126, 38)) { switchScreen(SCR_WIDGETS); return; }
    if (inRect(x, y, 12,  42,  232, 184)) { switchScreen(APP_MAP); return; }
    if (inRect(x, y, 254, 42,  214, 92))  { handleHomeWidgetTap(homeSlot[0], x, 254); return; }
    if (inRect(x, y, 254, 142, 214, 84))  { handleHomeWidgetTap(homeSlot[1], x, 254); return; }
    if (y >= 232) {
      if (x < 114)      switchScreen(SCR_NEWS);
      else if (x < 198) switchScreen(APP_MAP);
      else if (x < 282) switchScreen(APP_WEATHER);
      else if (x < 366) switchScreen(SCR_CONTROL);
      else              switchScreen(SCR_APPS);
    }
    return;
  }

  if (currentScreen == SCR_NEWS) {
    if (inRect(x, y, 6, 2, 104, 36)) { switchScreen(SCR_HOME); return; }
    if (inRect(x, y, 366, 2, 108, 36)) {
      showToast(String("Loading ") + TOPIC_NAMES[newsTopicIdx] + " News...", 3500);
      renderCurrentScreen(true);
      showToast(fetchLiveNews(newsTopicIdx) ? "5 Latest Stories Loaded!" : "Check Wi-Fi Connection", 1300);
      renderCurrentScreen(true);
      return;
    }
    if (y >= 36 && y <= 66) {
      int tIdx = constrain((x - 12) / 92, 0, NUM_NEWS_TOPICS - 1);
      newsTopicIdx = tIdx;
      showToast(String("Fetching ") + TOPIC_NAMES[tIdx] + " News & Photos...", 4000);
      renderCurrentScreen(true);
      showToast(fetchLiveNews(tIdx) ? (String(TOPIC_NAMES[tIdx]) + " News Live!") : "Feed Offline", 1300);
      renderCurrentScreen(true);
      return;
    }
    if (y >= 67 && y <= 188) { selectedArticle = (x < 240) ? 0 : 1; switchScreen(SCR_ARTICLE); return; }
    if (y > 188)             { selectedArticle = 2 + constrain((x - 12) / 154, 0, 2); switchScreen(SCR_ARTICLE); return; }
    return;
  }

  if (currentScreen == SCR_MARKETS) {
    if (inRect(x, y, 6, 2, 104, 38)) { switchScreen(SCR_HOME); return; }
    if (inRect(x, y, 366, 2, 108, 38)) loadMarketPage();
    else if (inRect(x, y, 14, 46, 152, 30)) {
      stockMarkets = !stockMarkets;
      marketPage = 0;
      loadMarketPage();
    } else if (y >= 280 && (x < 100 || x > 380)) {
      marketPage = constrain(marketPage + (x < 100 ? -1 : 1), 0, stockMarkets ? 14 : 24);
      loadMarketPage();
    }
    return;
  }

  if (currentScreen == SCR_WORLD || currentScreen == APP_CALENDAR || currentScreen == APP_PHOTOS) {
    if (inRect(x, y, 6, 2, 104, 38)) {
      switchScreen(currentScreen == SCR_WORLD ? SCR_HOME : SCR_APPS);
      return;
    }
    if (inRect(x, y, 366, 2, 108, 38)) {
      if (currentScreen == SCR_WORLD) switchScreen(APP_CALENDAR);
      else if (currentScreen == APP_CALENDAR) calendarOffset = 0;
      else scanPhotos();
      uiDirty = true;
    } else if (currentScreen == APP_CALENDAR && y >= 48 && y < 91) {
      if (x < 112 || x > 368) {
        int step = (x < 59 || x > 421) ? 12 : 1;
        moveCalendar(x < 112 ? -step : step);
      }
    } else if (currentScreen == APP_PHOTOS && y >= 259 && photoCount) {
      if (x < 96 || x > 384) {
        photoIndex = (photoIndex + photoCount + (x < 96 ? -1 : 1)) % photoCount;
        uiDirty = true;
      }
    }
    return;
  }

  if (currentScreen == SCR_PHONE || currentScreen == SCR_MUSIC) {
    if (inRect(x, y, 6, 2, 104, 38)) { switchScreen(SCR_HOME); return; }
    if (inRect(x, y, 366, 2, 108, 38)) {
      if (currentScreen == SCR_MUSIC) switchScreen(SCR_PHONE);
      else if (PhoneLink::start()) {
        PhoneLink::showPairingCode();
        passkeyPanelVisible = true;
        uiDirty = true;
      } else showToast("Bluetooth initialization failed", 1800);
      return;
    }
    if (currentScreen == SCR_MUSIC && x >= 14 && x < 466 && y >= 156 && y < 300) {
      uint8_t action = 1 + (x - 14) / 152 + (y - 156) / 50 * 3;
      if (mediaAllowed(action)) xQueueSend(PhoneLink::commands, &action, 0);
    }
    return;
  }

  if (currentScreen == SCR_ARTICLE) {
    if (inRect(x, y, 6, 2, 104, 38))   { switchScreen(SCR_NEWS); return; }
    if (inRect(x, y, 366, 2, 108, 38)) { selectedArticle = (selectedArticle + 1) % NUM_NEWS_ITEMS; renderCurrentScreen(true); return; }
    if (y >= 268 && x < 140)           { selectedArticle = (selectedArticle + NUM_NEWS_ITEMS - 1) % NUM_NEWS_ITEMS; renderCurrentScreen(true); return; }
    if (y >= 268 && x > 340)           { selectedArticle = (selectedArticle + 1) % NUM_NEWS_ITEMS; renderCurrentScreen(true); return; }
    return;
  }

  if (currentScreen == APP_MAP) {
    if (inRect(x, y, 6, 2, 100, 42)) { switchScreen(SCR_HOME); return; }
    if (inRect(x, y, 179, 2, 122, 42)) {
      centerMap(17.3850, 78.4867);
      showToast("Loading Hyderabad...", 1800);
      renderCurrentScreen(true);
      downloadAreaRealMap(mapSatellite);
      uiDirty = true;
      return;
    }
    if (inRect(x, y, 348, 2, 128, 42)) {
      bool nextMode = !mapSatellite;
      showToast(nextMode ? "Loading Satellite..." : "Loading Street Map...", 2500);
      renderCurrentScreen(true);
      bool loaded = downloadAreaRealMap(nextMode);
      showToast(loaded ? (mapSatellite ? "Satellite Live" : "Street Map Live") : "Map tiles unavailable", 1300);
      renderCurrentScreen(true);
    }
    return;
  }

  if (inRect(x, y, 6, 2, 104, 38)) {
    if (currentScreen >= APP_MAP) switchScreen(SCR_APPS); else switchScreen(SCR_HOME);
    return;
  }

  if (currentScreen == SCR_CONTROL) {
    if (inRect(x, y, 14, 42, 452, 50)) {
      screenBrightness = constrain(20 + ((x - 20) * 80) / 440, 20, 100);
      applyHardwarePWM();
      prefs.begin("cuteos", false); prefs.putInt("bri", screenBrightness); prefs.end();
      renderCurrentScreen(true);
      return;
    }
    if (y >= 102) {
      int idx = ((y < 208) ? 0 : 4) + constrain((x - 14) / 115, 0, 3);
      if (idx == 0) {
        showToast(String("Connecting to ") + WIFI_SSID + "...", 2000); renderCurrentScreen(true);
        WiFi.begin(WIFI_SSID, WIFI_PASS);
        unsigned long wt = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - wt < 5000) delay(150);
        wifiOnline = (WiFi.status() == WL_CONNECTED);
        if (wifiOnline) { configTime(SECRET_NTP_OFFSET_SEC, 0, "pool.ntp.org", "time.google.com"); updateTimeFromNtp(); fetchLocalWeather(); }
        showToast(wifiOnline ? (String(WIFI_SSID) + " Connected!") : String("Wi-Fi Unavailable"), 1500);
      } else if (idx == 1) {
        showToast("Fetching Everest Photo...", 2200); renderCurrentScreen(true);
        showToast(downloadRealEverestPhoto() ? "Mount Everest Live!" : "Check Wi-Fi Signal", 1400);
      } else if (idx == 2) {
        switchScreen(APP_MAP); return;
      } else if (idx == 3) {
        rgbPreset = (rgbPreset + 1) % 7;
        const uint8_t pal[7][3] = {{0,0,0},{18,122,255},{36,196,92},{255,196,12},{255,118,16},{255,45,112},{156,66,245}};
        applyRgb(pal[rgbPreset][0], pal[rgbPreset][1], pal[rgbPreset][2]);
        showToast(rgbPreset == 0 ? "RGB Lamp OFF" : "RGB Color Changed", 1100);
      } else if (idx == 4) { runRFSpectrumScan(); }
      else if (idx == 5)   { runAndSaveTouchCalibration(); }
      else if (idx == 6)   { for (int i = 0; i < 3; i++) { gpioStates[i] = false; digitalWrite(OUT_GPIOS[i], LOW); } showToast("All GPIOs LOW", 1200); }
      else if (idx == 7)   { showToast("Restarting cuteOS...", 900); renderCurrentScreen(true); delay(400); ESP.restart(); }
      uiDirty = true;
    }
    return;
  }

  if (currentScreen == SCR_APPS) {
    if (inRect(x, y, 366, 2, 108, 38)) { switchScreen(SCR_WIDGETS); return; }
    if (y >= 44 && y < 302) {
      int idx = (y - 44) / 86 * 4 + constrain((x - 18) / 111, 0, 3);
      int targets[] = {SCR_NEWS, APP_MAP, APP_WEATHER, APP_RADAR, APP_SCOPE, APP_RGB,
                       APP_CALC, APP_TIMER, APP_CALENDAR, APP_PHOTOS, SCR_PHONE, SCR_WORLD};
      switchScreen(targets[idx]);
    }
    return;
  }

  if (currentScreen == SCR_TILES) {
    if (inRect(x, y, 366, 2, 108, 38)) {
      if (currentTile == 3) { for (int i = 0; i < MAX_NOTIFS; i++) notifs[i].active = false; showToast("Cleared All", 1100); }
      else                  { currentTile = (currentTile + 1) % NUM_TILES; renderCurrentScreen(true); }
      return;
    }
    if (currentTile == 0) { if (x < 256) switchScreen(APP_MAP); else switchScreen(APP_WEATHER); }
    else if (currentTile == 1) { runRFSpectrumScan(); }
    else if (currentTile == 2 && y >= 160) {
      int idx = constrain((x - 14) / 154, 0, 2);
      gpioStates[idx] = !gpioStates[idx];
      digitalWrite(OUT_GPIOS[idx], gpioStates[idx] ? HIGH : LOW);
      showToast(String("GPIO ") + String(OUT_GPIOS[idx]) + (gpioStates[idx] ? " -> 3.3V" : " -> 0V"), 1100);
      uiDirty = true;
    }
    return;
  }

  if (currentScreen == SCR_WIDGETS) {
    if (inRect(x, y, 366, 2, 108, 38)) { editSlotIndex = 1 - editSlotIndex; uiDirty = true; return; }
    if (y >= 46) {
      int idx = ((y < 174) ? 0 : 4) + constrain((x - 14) / 115, 0, 3);
      if (idx < NUM_WIDGETS) {
        homeSlot[editSlotIndex] = idx;
        prefs.begin("cuteos", false); prefs.putInt("w0", homeSlot[0]); prefs.putInt("w1", homeSlot[1]); prefs.end();
        showToast(editSlotIndex == 0 ? "Top Widget Saved!" : "Bottom Widget Saved!", 1200);
      } else { editSlotIndex = 1 - editSlotIndex; }
      uiDirty = true;
    }
    return;
  }

  if (inRect(x, y, 366, 2, 108, 38)) {
    if (currentScreen == APP_WEATHER) { showToast("Refreshing Weather...", 1600); renderCurrentScreen(true); fetchLocalWeather(); return; }
    if (currentScreen == APP_RADAR)   { runRFSpectrumScan(); return; }
    if (currentScreen == APP_SCOPE)   { scopePaused = !scopePaused; uiDirty = true; return; }
    if (currentScreen == APP_RGB)     { rgbBreathing = !rgbBreathing; showToast(rgbBreathing ? "Breathing ON" : "Solid Mode", 1100); uiDirty = true; return; }
    if (currentScreen == APP_CALC)    { calcDisplay = "0"; calcAcc = 0; calcOp = 0; calcNewInput = true; uiDirty = true; return; }
    switchScreen(SCR_HOME);
    return;
  }

  if (currentScreen == APP_RGB) {
    if (y >= 90 && y <= 165) {
      int idx = constrain((x - 22) / 62, 0, 6);
      rgbPreset = idx;
      const uint8_t pal[7][3] = {{0,0,0},{18,122,255},{36,196,92},{255,196,12},{255,118,16},{255,45,112},{156,66,245}};
      applyRgb(pal[idx][0], pal[idx][1], pal[idx][2]); uiDirty = true;
    } else if (y >= 184) {
      int idx = constrain((x - 14) / 154, 0, 2);
      gpioStates[idx] = !gpioStates[idx];
      digitalWrite(OUT_GPIOS[idx], gpioStates[idx] ? HIGH : LOW);
      showToast(String("GPIO ") + String(OUT_GPIOS[idx]) + (gpioStates[idx] ? " ON" : " OFF"), 1100);
      uiDirty = true;
    }
  } else if (currentScreen == APP_CALC && y >= 104) {
    const char* keys[4][4] = {{"7","8","9","/"},{"4","5","6","*"},{"1","2","3","-"},{"0",".","=","+"}};
    handleCalcKey(keys[constrain((y - 106) / 52, 0, 3)][constrain((x - 14) / 115, 0, 3)]);
  } else if (currentScreen == APP_TIMER) {
    if (inRect(x, y, 276, 44,  190, 68)) { timerRunning = !timerRunning; lastTimerTick = millis(); uiDirty = true; }
    if (inRect(x, y, 276, 122, 190, 54)) { timerRunning = false; timerRemainSec = timerPresetSec; uiDirty = true; }
    if (inRect(x, y, 276, 186, 90,  54)) { timerRunning = false; timerPresetSec = timerRemainSec = 5 * 60; uiDirty = true; }
    if (inRect(x, y, 376, 186, 90,  54)) { timerRunning = false; timerPresetSec = timerRemainSec = 15 * 60; uiDirty = true; }
    if (inRect(x, y, 276, 250, 190, 58)) { timerRunning = false; timerPresetSec = timerRemainSec = 25 * 60; uiDirty = true; }
  }
}

void handleSwipe(int dx, int dy) {
  bool horiz = abs(dx) > abs(dy);
  if (currentScreen == SCR_HOME) {
    if (horiz  && dx > 38)  switchScreen(SCR_NEWS);    // Swipe Left -> Right : 5-Tile Live News
    if (horiz  && dx < -38) switchScreen(SCR_MUSIC);
    if (!horiz && dy > 38)  switchScreen(SCR_CONTROL); // Swipe Down          : Brightness & Controls
    if (!horiz && dy < -38) switchScreen(SCR_APPS);    // Swipe Up            : Apple Squircle Apps
  } else if (currentScreen == SCR_NEWS) {
    if (horiz && dx < -38) switchScreen(SCR_HOME);
    if (horiz && dx > 38) switchScreen(SCR_PHONE);
  } else if (currentScreen == SCR_PHONE) {
    if (horiz && dx < -38) switchScreen(SCR_NEWS);
    if (horiz && dx > 38) switchScreen(SCR_MARKETS);
  } else if (currentScreen == SCR_MARKETS) {
    if (horiz && dx < -38) switchScreen(SCR_PHONE);
  } else if (currentScreen == SCR_MUSIC) {
    if (horiz && dx > 38) switchScreen(SCR_HOME);
    if (horiz && dx < -38) { currentTile = 0; switchScreen(SCR_TILES); }
  } else if (currentScreen == SCR_WORLD) {
    if (horiz && dx > 38) { currentTile = NUM_TILES - 1; switchScreen(SCR_TILES); }
  } else if (currentScreen == APP_CALENDAR) {
    if (horiz) moveCalendar(dx < 0 ? 1 : -1);
  } else if (currentScreen == APP_PHOTOS) {
    if (horiz && photoCount) {
      photoIndex = (photoIndex + photoCount + (dx < 0 ? 1 : -1)) % photoCount;
      uiDirty = true;
    }
  } else if (currentScreen == SCR_ARTICLE) {
    if (horiz && dx < -38)  { selectedArticle = (selectedArticle + 1) % NUM_NEWS_ITEMS; renderCurrentScreen(true); }
    if (horiz && dx > 38)   { selectedArticle = (selectedArticle + NUM_NEWS_ITEMS - 1) % NUM_NEWS_ITEMS; renderCurrentScreen(true); }
    if (!horiz && dy > 45)  switchScreen(SCR_NEWS);
  } else if (currentScreen == SCR_CONTROL) {
    if (!horiz && dy < -38) switchScreen(SCR_HOME);
  } else if (currentScreen == SCR_APPS || currentScreen == SCR_WIDGETS) {
    if (!horiz && dy > 38)  switchScreen(SCR_HOME);
  } else if (currentScreen == SCR_TILES) {
    if (horiz && dx < -38) {
      if (currentTile + 1 == NUM_TILES) switchScreen(SCR_WORLD);
      else { ++currentTile; renderCurrentScreen(true); }
    } else if (horiz && dx > 38) {
      if (currentTile > 0) { --currentTile; renderCurrentScreen(true); }
      else switchScreen(SCR_MUSIC);
    }
  } else {
    if (!horiz && dy > 48) switchScreen(SCR_HOME);
  }
}

void pollTouch() {
  int rx = 0, ry = 0;
  if (getFilteredTouch(rx, ry)) {
    lastTouchSeen = millis();
    if (!touchActive) {
      touchActive = true; touchRingDrawn = false; draggingBright = false;
      smoothX = (float)rx; smoothY = (float)ry;
      touchStartX = touchLastX = rx; touchStartY = touchLastY = ry;
      if (currentScreen == APP_MAP && ry > 42 && ry < 279 &&
          (mapLoaded || WiFi.status() == WL_CONNECTED) && mapCenterX >= 0) {
        draggingMap = true;
        lastMapPreviewX = lastMapPreviewY = 0;
      } else if (currentScreen == SCR_CONTROL && inRect(rx, ry, 14, 42, 452, 50)) draggingBright = true;
      else { tft.drawCircle(rx, ry, 9, C_CYAN); touchRingDrawn = true; }
    } else {
      smoothX = smoothX * 0.4f + (float)rx * 0.6f;
      smoothY = smoothY * 0.4f + (float)ry * 0.6f;
      int ix = (int)smoothX, iy = (int)smoothY;
      if (draggingBright) {
        int nb = constrain(20 + ((ix - 20) * 80) / 440, 20, 100);
        if (abs(nb - screenBrightness) >= 3) { screenBrightness = nb; applyHardwarePWM(); renderCurrentScreen(true); }
      }
      touchLastX = ix; touchLastY = iy;
      if (draggingMap && millis() - lastMapPreview >= 65) {
        lastMapPreview = millis();
        lastMapPreviewX = ix - touchStartX;
        lastMapPreviewY = iy - touchStartY;
        renderCurrentScreen(true);
      }
    }
  } else if (touchActive && (millis() - lastTouchSeen > 18)) {
    touchActive = false;
    if (draggingMap) {
      draggingMap = false;
      int dx = touchLastX - touchStartX, dy = touchLastY - touchStartY;
      if (abs(dx) >= 12 || abs(dy) >= 12) {
        mapCenterX = (mapCenterX - dx + MAP_WORLD) % MAP_WORLD;
        mapCenterY = constrain(mapCenterY - dy, 0, MAP_WORLD - 1);
        downloadAreaRealMap(mapSatellite);
      }
      uiDirty = true;
      return;
    }
    if (draggingBright) {
      draggingBright = false;
      prefs.begin("cuteos", false); prefs.putInt("bri", screenBrightness); prefs.end();
      return;
    }
    if (touchRingDrawn) { pushPartial(touchStartX - 11, touchStartY - 11, 22, 22); touchRingDrawn = false; }
    int dx = touchLastX - touchStartX, dy = touchLastY - touchStartY;
    if (abs(dx) > 38 || abs(dy) > 38) handleSwipe(dx, dy);
    else                              handleTap(touchStartX, touchStartY);
  }
}

// ============================================================================
// 15. SETUP & MAIN LOOP
// ============================================================================
void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  pinMode(BACKLIGHT_PWM_PIN, OUTPUT);
  for (int i = 0; i < 3; i++) { pinMode(OUT_GPIOS[i], OUTPUT); digitalWrite(OUT_GPIOS[i], LOW); }
  for (int i = 0; i < SCOPE_PTS; i++) scopeBuf[i] = 2048;
  applyRgb(18, 122, 255);

  tft.init();
  tft.setRotation(1);

  prefs.begin("cuteos", true);
  screenBrightness = constrain(prefs.getInt("bri", 100), 20, 100);
  homeSlot[0]      = prefs.getInt("w0", W_WEATHER);
  homeSlot[1]      = prefs.getInt("w1", W_MINIMAP);
  resumePhoneLink  = prefs.getBool("phone", false);
  uint16_t savedCal[8];
  bool hasCal      = (prefs.getBytesLength("cal") == sizeof(savedCal));
  if (hasCal) prefs.getBytes("cal", savedCal, sizeof(savedCal));
  prefs.end();
  applyHardwarePWM();

  drawBootSplash("Initializing OPI PSRAM...", 12);

  bgSprite.setPsram(true);    bgSprite.setColorDepth(16);
  frostSprite.setPsram(true); frostSprite.setColorDepth(16);
  darkGlass.setPsram(true);   darkGlass.setColorDepth(16);
  mapSprite.setPsram(true);   mapSprite.setColorDepth(16);
  canvas.setPsram(true);      canvas.setColorDepth(16);

  bool ok = bgSprite.createSprite(480, 320) &&
            frostSprite.createSprite(480, 320) &&
            darkGlass.createSprite(480, 320) &&
            mapSprite.createSprite(480, 320) &&
            canvas.createSprite(480, 320);

  for (int i = 0; i < NUM_NEWS_ITEMS; i++) {
    newsImg[i] = new LGFX_Sprite(&tft);
    newsImg[i]->setPsram(true);
    newsImg[i]->setColorDepth(16);
    if (!newsImg[i]->createSprite(240, 135)) ok = false;
  }

  if (!ok) {
    tft.fillScreen(C_WHITE);
    tft.setFont(&fonts::FreeSerifBold18pt7b); tft.setTextColor(C_RED); tft.drawString("ENABLE OPI PSRAM!", 240, 140);
    tft.setFont(&fonts::FreeSerif12pt7b);     tft.setTextColor(C_INK); tft.drawString("Arduino IDE: Tools -> PSRAM -> OPI PSRAM", 240, 180);
    while (true) delay(1000);
  }

  canvas.drawPixel(0, 0, TFT_RED);
  spriteIsSwapped = (((uint16_t*)canvas.getBuffer())[0] == 0x00F8);

  updateBootProgress("Preparing Alpine Graphics...", 25);
  buildPhotorealisticEverestFallback();
  buildOfflineAreaMap();
  initOfflineNewsPlaceholders();

  if (hasCal) tft.setTouchCalibrate(savedCal);
  else { runAndSaveTouchCalibration(); drawBootSplash("Touch Saved! Connecting Wi-Fi...", 38); }

  updateBootProgress((String("Connecting to ") + WIFI_SSID + " Wi-Fi...").c_str(), 45);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long wt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wt < 8000) delay(150);

  if (WiFi.status() == WL_CONNECTED) {
    wifiOnline = true;
    updateBootProgress("Syncing Time & Weather...", 58);
    configTime(SECRET_NTP_OFFSET_SEC, 0, "pool.ntp.org", "time.google.com");
    delay(350);
    updateTimeFromNtp();
    fetchLocalWeather();

    updateBootProgress("Loading Real Mount Everest Photo...", 70);
    downloadRealEverestPhoto();

    updateBootProgress((String("Loading ") + AREA_NAME + " Map...").c_str(), 82);
    downloadAreaRealMap(false);

    updateBootProgress("Downloading 5 Live News & Images...", 94);
    fetchLiveNews(0);

    pushNotif("Wi-Fi", String(WIFI_SSID) + " Connected", "Everest, Map, Weather & 5 News Live", C_GREEN);
  } else {
    pushNotif("Wi-Fi", "Offline Mode", String("Swipe Down -> Wi-Fi to reconnect ") + WIFI_SSID, C_ORANGE);
  }

  updateBootProgress("Welcome to cuteOS", 100);
  delay(220);
  pushNotif("cuteOS", "Swipe Left -> Right", "Open 5 Live News Tiles with Photos", C_ROSE);
  renderCurrentScreen(true);
  if (resumePhoneLink) PhoneLink::start();
}

void loop() {
  consumePhoneFrames();
  if (passkeyPanelVisible && (int32_t)(PhoneLink::showKeyUntil - millis()) <= 0) {
    passkeyPanelVisible = false;
    if (currentScreen == SCR_PHONE) uiDirty = true;
  }
  pollTouch();

  if (currentScreen == APP_SCOPE && !scopePaused && millis() - lastScopeTick >= 55) {
    lastScopeTick = millis();
    int raw = analogRead(ADC_SCOPE_PIN);
    scopeLastVolt = (raw * 3.3f) / 4095.0f;
    for (int i = 0; i < SCOPE_PTS - 1; i++) scopeBuf[i] = scopeBuf[i + 1];
    scopeBuf[SCOPE_PTS - 1] = raw;
    drawScopeBodyOnly();
    pushPartial(14, 44, 452, 266);
  }

  static unsigned long lastBreathe = 0;
  if (rgbBreathing && millis() - lastBreathe > 45) {
    lastBreathe = millis();
    float s = (sinf(millis() * 0.003f) + 1.0f) * 0.5f;
    setOnboardRGB((uint8_t)(rgbR * s * 0.25f), (uint8_t)(rgbG * s * 0.25f), (uint8_t)(rgbB * s * 0.25f));
  }

  if (timerRunning && (millis() - lastTimerTick >= 1000)) {
    lastTimerTick = millis();
    if (timerRemainSec > 0) {
      timerRemainSec--;
      if (currentScreen == APP_TIMER || currentScreen == SCR_HOME) uiDirty = true;
    } else {
      timerRunning = false;
      pushNotif("Timer", "Focus Complete!", "Great work - session done", C_ROSE);
      showToast("Focus Timer Done!", 2200);
    }
  }

  if (toastMsg.length() > 0 && millis() >= toastEnd) {
    toastMsg = "";
    uiDirty = true;
  }

  static unsigned long lastClockCheck = 0;
  if (wifiOnline && millis() - lastClockCheck > 10000) {
    lastClockCheck = millis();
    String prevTime = liveTime;
    updateTimeFromNtp();
    if (currentScreen == SCR_HOME && !islandExpanded && liveTime != prevTime) {
      drawHomeHeroCard();
      pushPartial(12, 42, 232, 184);
    }
    if (millis() - lastWeatherFetch > 900000UL) fetchLocalWeather();
  }

  static time_t lastPanelTime;
  if (currentScreen == SCR_WORLD || currentScreen == APP_CALENDAR) {
    time_t now = time(nullptr);
    if (now != lastPanelTime) { lastPanelTime = now; uiDirty = true; }
  }
  if (uiDirty && !touchActive) renderCurrentScreen(true);
  delay(2);
}
