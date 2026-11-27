#pragma once

#include <mbedtls/base64.h>

static int phoneBattery = -1;
static bool passkeyPanelVisible;
static bool resumePhoneLink;
static bool phoneCharging;
static bool phoneReady;
static String phoneTrack, phoneArtist;
static int phoneVolume;
static uint32_t phoneActions;
static bool phonePlaying;
static String phoneNotices[4][2];

constexpr int MUSIC_ART_SIZE = 72;
constexpr size_t MUSIC_ART_MAX_JPEG = 2200;
constexpr size_t MUSIC_ART_MAX_ENCODED = 4 * ((MUSIC_ART_MAX_JPEG + 2) / 3);
static char musicArtEncoded[MUSIC_ART_MAX_ENCODED];
static uint8_t musicArtJpeg[MUSIC_ART_MAX_JPEG];
static size_t musicArtLength, musicArtReceived;
static LGFX_Sprite musicArt(&tft);
static bool hasMusicArt;

static void resetMusicArt() {
  musicArtLength = musicArtReceived = 0;
  hasMusicArt = false;
}

static bool receiveMusicArt(char* fields[], size_t count) {
  if (strcmp(fields[0], "A") == 0 && count == 2) {
    bool changed = hasMusicArt;
    resetMusicArt();
    unsigned long length = strtoul(fields[1], nullptr, 10);
    if (length <= MUSIC_ART_MAX_ENCODED && length % 4 == 0) musicArtLength = length;
    return changed;
  }
  if (strcmp(fields[0], "I") != 0 || count != 3 || !musicArtLength) return false;
  size_t offset = strtoul(fields[1], nullptr, 10);
  size_t length = strlen(fields[2]);
  if (offset != musicArtReceived || length > musicArtLength - musicArtReceived) {
    resetMusicArt();
    return false;
  }
  memcpy(musicArtEncoded + offset, fields[2], length);
  musicArtReceived += length;
  if (musicArtReceived != musicArtLength) return false;

  size_t decodedLength = 0;
  int error = mbedtls_base64_decode(musicArtJpeg, sizeof(musicArtJpeg), &decodedLength,
                                   (const uint8_t*)musicArtEncoded, musicArtLength);
  resetMusicArt();
  if (error || decodedLength < 4 || musicArtJpeg[0] != 0xFF || musicArtJpeg[1] != 0xD8) return false;
  if (!musicArt.getBuffer()) {
    musicArt.setPsram(true);
    musicArt.setColorDepth(16);
    if (!musicArt.createSprite(MUSIC_ART_SIZE, MUSIC_ART_SIZE)) return false;
  }
  musicArt.fillScreen(C_DARK_PILL);
  hasMusicArt = musicArt.drawJpg(musicArtJpeg, decodedLength, 0, 0, MUSIC_ART_SIZE, MUSIC_ART_SIZE);
  return hasMusicArt;
}

static void handleMarketFrame(char* fields[], size_t count);

static void consumePhoneFrames() {
  if (!PhoneLink::received) return;
  static uint32_t previousGeneration;
  uint32_t generation = PhoneLink::generation.load();
  bool newLink = generation != previousGeneration;
  previousGeneration = generation;
  if ((!PhoneLink::connected.load() || newLink) && phoneReady) {
    phoneReady = false;
    phoneBattery = -1;
    phoneTrack = phoneArtist = "";
    phoneActions = 0;
    resetMusicArt();
    for (auto& notice : phoneNotices) notice[0] = notice[1] = "";
    if (currentScreen == SCR_PHONE || currentScreen == SCR_MUSIC) uiDirty = true;
  }
  PhoneLink::Frame frame;
  while (xQueueReceive(PhoneLink::received, &frame, 0) == pdPASS) {
    char* fields[7]{};
    size_t count = 1;
    fields[0] = frame.text;
    for (char* c = frame.text; *c && count < 7; ++c) {
      if (*c == '\t') {
        *c = '\0';
        fields[count++] = c + 1;
      }
    }
    if (!PhoneLink::connected.load()) break;
    if (strcmp(fields[0], "A") == 0 || strcmp(fields[0], "I") == 0) {
      if (receiveMusicArt(fields, count) && currentScreen == SCR_MUSIC) uiDirty = true;
      continue;
    }
    if (strcmp(fields[0], "B") == 0 && count == 3) {
      phoneBattery = constrain(atoi(fields[1]), -1, 100);
      phoneCharging = atoi(fields[2]) != 0;
      phoneReady = true;
      if (!resumePhoneLink && prefs.begin("cuteos", false)) {
        resumePhoneLink = prefs.putBool("phone", true) == 1;
        prefs.end();
      }
    } else if (strcmp(fields[0], "M") == 0 && count == 6) {
      if (phoneTrack != fields[2] || phoneArtist != fields[3]) resetMusicArt();
      phoneVolume = constrain(atoi(fields[1]), 0, 100);
      phoneTrack = fields[2];
      phoneArtist = fields[3];
      phoneActions = strtoul(fields[4], nullptr, 10);
      phonePlaying = atoi(fields[5]) != 0;
    } else if (strcmp(fields[0], "N") == 0 && count == 4) {
      for (int i = 3; i > 0; --i) {
        phoneNotices[i][0] = phoneNotices[i - 1][0];
        phoneNotices[i][1] = phoneNotices[i - 1][1];
      }
      phoneNotices[0][0] = *fields[2] ? fields[2] : fields[1];
      phoneNotices[0][1] = fields[3];
    } else if (count >= 3) {
      handleMarketFrame(fields, count);
    }
    if (currentScreen == SCR_PHONE || currentScreen == SCR_MUSIC) uiDirty = true;
  }
}

static String fitPanelText(String text, int width) {
  if (canvas.textWidth(text) <= width) return text;
  do {
    size_t end = text.length() - 1;
    while (end && ((uint8_t)text[end] & 0xC0) == 0x80) --end;
    text.remove(end);
  } while (text.length() && canvas.textWidth(text + "...") > width);
  return text + "...";
}

static void panelLabel(const String& label, int x, int y, int width, uint16_t color) {
  canvas.setTextDatum(lgfx::middle_left);
  canvas.setTextColor(color);
  canvas.drawString(fitPanelText(label, width), x, y);
}

static void drawPhonePanel() {
  drawLiquidHeader("Phone Notifications", "Pair");
  drawLiquidCard(14, 44, 452, 262, 27, C_CYAN);
  canvas.setFont(&fonts::FreeSerifBold12pt7b);
  String status = phoneReady ? "Android connected" : "Phone disconnected";
  if (!phoneReady && PhoneLink::passkey && passkeyPanelVisible) {
    status = "Pairing code " + String(PhoneLink::passkey.load());
  }
  panelLabel(status, 29, 66, 352, C_WHITE);
  canvas.setFont(&fonts::FreeSerif9pt7b);
  if (phoneBattery >= 0) {
    panelLabel(String(phoneBattery) + "%" + (phoneCharging ? " / Plugged in" : ""), 29, 91, 400, C_CYAN);
  } else {
    panelLabel("Open cuteOS Companion on your Android phone", 29, 91, 400, C_CYAN);
  }
  if (phoneReady && !phoneNotices[0][0].length()) {
    panelLabel("No notifications yet", 29, 130, 420, C_WHITE);
    panelLabel("Tap Send test notification in the phone app", 29, 153, 420, C_SOFT_GRAY);
  }
  for (int i = 0; i < 4 && phoneNotices[i][0].length(); ++i) {
    int y = 119 + i * 44;
    canvas.drawFastHLine(29, y - 13, 422, C_BORDER);
    canvas.setFont(&fonts::FreeSerifBold9pt7b);
    panelLabel(phoneNotices[i][0], 29, y, 420, C_WHITE);
    canvas.setFont(&fonts::FreeSerif9pt7b);
    panelLabel(phoneNotices[i][1], 29, y + 18, 420, C_SOFT_GRAY);
  }
}

static bool mediaAllowed(int action) {
  if (!phoneReady) return false;
  switch (action) {
    case 1: return phoneActions & (512 | (phonePlaying ? 2 : 4));
    case 2: return phoneActions & 16;
    case 3: return phoneActions & 32;
    case 4: case 5: return true;
    case 6: case 7: return phoneActions & 256;
    case 8: case 9: return phoneActions & (1UL << 22);
    default: return false;
  }
}

static void drawMusicArtwork(int x, int y) {
  if (hasMusicArt) {
    const auto* pixels = (const uint16_t*)musicArt.getBuffer();
    auto* target = (uint16_t*)canvas.getBuffer();
    for (int row = 0; row < MUSIC_ART_SIZE; ++row) {
      int inset = roundedInset(row, MUSIC_ART_SIZE, 16);
      memcpy(target + (y + row) * 480 + x + inset, pixels + row * MUSIC_ART_SIZE + inset,
             (MUSIC_ART_SIZE - 2 * inset) * sizeof(uint16_t));
    }
    canvas.drawRoundRect(x, y, MUSIC_ART_SIZE, MUSIC_ART_SIZE, 16, C_WHITE);
  } else {
    drawLiquidCard(x, y, MUSIC_ART_SIZE, MUSIC_ART_SIZE, 16, C_VIOLET);
    const int cx = x + MUSIC_ART_SIZE / 2, cy = y + MUSIC_ART_SIZE / 2;
    canvas.drawCircle(cx, cy, 23, blend565(C_CYAN, C_DARK_PILL, 140));
    canvas.fillCircle(cx - 3, cy + 9, 6, C_CYAN);
    canvas.fillRect(cx + 1, cy - 13, 4, 22, C_CYAN);
    canvas.fillRoundRect(cx + 2, cy - 13, 11, 5, 2, C_CYAN);
  }
}

struct MusicButton { uint8_t action; int x, y, width; };
static constexpr MusicButton MUSIC_BUTTONS[] = {
  {2, 14, 156, 146}, {1, 166, 156, 148}, {3, 320, 156, 146},
  {6, 14, 206, 108}, {7, 128, 206, 108}, {8, 242, 206, 108}, {9, 356, 206, 110},
  {4, 14, 256, 222}, {5, 244, 256, 222}
};

static void drawMusicPanel() {
  drawLiquidHeader("Now Playing", "Phone");
  drawLiquidCard(14, 44, 452, 104, 28, C_VIOLET);
  drawMusicArtwork(29, 60);
  canvas.setFont(&fonts::FreeSansBold12pt7b);
  panelLabel(phoneTrack.length() ? phoneTrack : "No track playing", 117, 69, 330, C_WHITE);
  canvas.setFont(&fonts::FreeSans9pt7b);
  panelLabel(phoneReady ? phoneArtist : "Pair your phone to play music", 117, 94, 330, C_CYAN);
  if (phoneReady) {
    panelLabel("Volume " + String(phoneVolume) + "%", 117, 121, 135, C_WHITE);
    canvas.fillRoundRect(265, 118, 178, 6, 3, blend565(C_VIOLET, C_DARK_PILL, 100));
    int width = phoneVolume * 178 / 100;
    if (width > 0) canvas.fillRoundRect(265, 118, width, 6, 3, C_CYAN);
  }

  const char* labels[] = {"Previous", phonePlaying ? "Pause" : "Play", "Next",
                          "Back 15s", "Skip 15s", "Slower", "Faster", "Volume -", "Volume +"};
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  for (size_t i = 0; i < sizeof(MUSIC_BUTTONS) / sizeof(MUSIC_BUTTONS[0]); ++i) {
    const auto& button = MUSIC_BUTTONS[i];
    bool enabled = mediaAllowed(button.action);
    drawLiquidCard(button.x, button.y, button.width, 44, 21,
                   enabled ? (button.action == 1 ? C_VIOLET : C_CYAN) : C_DARK_PILL);
    canvas.setTextDatum(lgfx::middle_center);
    canvas.setTextColor(enabled ? C_WHITE : makeRgb(165, 184, 204));
    canvas.drawString(labels[i], button.x + button.width / 2, button.y + 22);
  }
}

static uint8_t musicActionAt(int x, int y) {
  for (const auto& button : MUSIC_BUTTONS) {
    if (x >= button.x && x < button.x + button.width && y >= button.y && y < button.y + 44) return button.action;
  }
  return 0;
}
