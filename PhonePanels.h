#pragma once

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
    if (strcmp(fields[0], "B") == 0 && count == 3) {
      phoneBattery = constrain(atoi(fields[1]), -1, 100);
      phoneCharging = atoi(fields[2]) != 0;
      phoneReady = true;
      if (!resumePhoneLink && prefs.begin("cuteos", false)) {
        resumePhoneLink = prefs.putBool("phone", true) == 1;
        prefs.end();
      }
    } else if (strcmp(fields[0], "M") == 0 && count == 6) {
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
  drawSubHeader("Phone", "< Home", "Pair", C_BLUE);
  drawFrostGlassCard(14, 44, 452, 262, 24, true, C_WHITE);
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

static void drawMusicPanel() {
  drawSubHeader("Music", "< Home", "Phone", C_BLUE);
  drawFrostGlassCard(14, 45, 452, 106, 24, true, C_WHITE);
  canvas.setFont(&fonts::FreeSerifBold18pt7b);
  panelLabel(phoneTrack.length() ? phoneTrack : "No active track", 29, 75, 420, C_WHITE);
  canvas.setFont(&fonts::FreeSerif9pt7b);
  panelLabel(phoneReady ? phoneArtist : "Pair your phone to control playback", 29, 106, 420, C_CYAN);
  if (phoneReady) panelLabel("Media volume " + String(phoneVolume) + "%", 29, 128, 420, C_WHITE);
  const char* labels[9] = {phonePlaying ? "Pause" : "Play", "Previous", "Next",
                           "Volume -", "Volume +", "-15 sec", "+15 sec", "Slower", "Faster"};
  for (int i = 0; i < 9; ++i) {
    int x = 14 + i % 3 * 152;
    int y = 156 + i / 3 * 50;
    bool enabled = mediaAllowed(i + 1);
    drawFrostGlassCard(x, y, 148, 44, 14, true, enabled ? C_CYAN : C_BORDER);
    canvas.setTextDatum(lgfx::middle_center);
    canvas.setTextColor(enabled ? C_WHITE : C_SOFT_GRAY);
    canvas.drawString(labels[i], x + 74, y + 22);
  }
}
