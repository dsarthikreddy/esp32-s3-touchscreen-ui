#pragma once

static int calendarOffset;
static int photoIndex;
static String photoPaths[24];
static int photoCount;
static bool photosScanned;

static void glassText(const String& value, int x, int y, uint16_t color = C_WHITE) {
  canvas.setTextDatum(lgfx::middle_center);
  canvas.setTextColor(color);
  canvas.drawString(value, x, y);
}

// ESP32 core 2.x uses a 32-bit time_t. The month grid uses Gregorian arithmetic
// independently of Unix time, so browsing works past the 2038 clock boundary.
static bool leapYear(int year) {
  return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

static int firstWeekday(int year, int month) {
  const int offsets[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (month < 3) --year;
  return (year + year / 4 - year / 100 + year / 400 + offsets[month - 1] + 1) % 7;
}

static bool currentLocalTime(tm& local) {
  time_t now = time(nullptr);
  return now >= 1704067200 && localtime_r(&now, &local) != nullptr;
}

static int currentMonthIndex(const tm& today, bool synced) {
  return synced ? (today.tm_year + 1900) * 12 + today.tm_mon : 2026 * 12;
}

static void moveCalendar(int step) {
  tm today{};
  bool synced = currentLocalTime(today);
  int current = currentMonthIndex(today, synced);
  calendarOffset = constrain(current + calendarOffset + step, 2000 * 12, 2099 * 12 + 11) - current;
  uiDirty = true;
}

static void drawCalendarPanel() {
  drawSubHeader("Calendar", "< Apps", "Today", C_BLUE);
  drawFrostGlassCard(14, 47, 452, 260, 24, true, C_WHITE);
  const char* months[] = {"January","February","March","April","May","June","July",
                         "August","September","October","November","December"};
  const char* weekdays[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
  const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  tm today{};
  bool synced = currentLocalTime(today);
  int index = constrain(currentMonthIndex(today, synced) + calendarOffset, 2000 * 12, 2099 * 12 + 11);
  int year = index / 12, month = index % 12;
  int numberOfDays = days[month] + (month == 1 && leapYear(year));
  canvas.setFont(&fonts::FreeSerifBold12pt7b);
  glassText(String(months[month]) + " " + year, 240, 69);
  glassText("<<", 36, 69, C_CYAN);
  glassText("<", 82, 69, C_CYAN);
  glassText(">", 398, 69, C_CYAN);
  glassText(">>", 444, 69, C_CYAN);
  canvas.setFont(&fonts::FreeSerif9pt7b);
  char clock[60] = "Waiting for internet time";
  if (synced) strftime(clock, sizeof(clock), "%A, %d %B %Y  %H:%M:%S", &today);
  glassText(clock, 240, 96, C_CYAN);
  for (int i = 0; i < 7; ++i) glassText(weekdays[i], 48 + 64 * i, 122, C_SOFT_GRAY);
  int first = firstWeekday(year, month + 1);
  for (int day = 1; day <= numberOfDays; ++day) {
    int cell = first + day - 1;
    int x = 48 + cell % 7 * 64, y = 148 + cell / 7 * 26;
    bool selected = synced && year == today.tm_year + 1900 && month == today.tm_mon && day == today.tm_mday;
    if (selected) canvas.fillRoundRect(x - 16, y - 11, 32, 22, 8, C_CYAN);
    glassText(String(day), x, y, selected ? C_INK : C_WHITE);
  }
}

static void drawWorldClockPanel() {
  drawSubHeader("World Clock", "< Home", "Calendar", C_BLUE);
  struct City { const char* name; const char* tz; };
  const City cities[] = {
    {"Hyderabad", "IST-5:30"},
    {"London", "GMT0BST,M3.5.0/1,M10.5.0/2"},
    {"New York", "EST5EDT,M3.2.0/2,M11.1.0/2"},
    {"Los Angeles", "PST8PDT,M3.2.0/2,M11.1.0/2"},
    {"Paris", "CET-1CEST,M3.5.0/2,M10.5.0/3"},
    {"Dubai", "GST-4"},
    {"Singapore", "SGT-8"},
    {"Tokyo", "JST-9"},
    {"Sydney", "AEST-10AEDT,M10.1.0/2,M4.1.0/3"},
    {"Auckland", "NZST-12NZDT,M9.5.0/2,M4.1.0/3"}
  };
  const char* saved = getenv("TZ");
  String oldTZ = saved ? saved : "";
  bool hadTZ = saved != nullptr;
  time_t now = time(nullptr);
  for (size_t i = 0; i < 10; ++i) {
    int x = 14 + (i % 2) * 230, y = 44 + (i / 2) * 52;
    drawFrostGlassCard(x, y, 222, 49, 14, true, C_WHITE);
    canvas.setFont(&fonts::FreeSerifBold9pt7b);
    panelLabel(cities[i].name, x + 12, y + 14, 190, C_WHITE);
    canvas.setFont(&fonts::FreeSerif9pt7b);
    char clock[32] = "Awaiting time sync";
    if (now >= 1704067200) {
      setenv("TZ", cities[i].tz, 1);
      tzset();
      tm local{};
      if (localtime_r(&now, &local)) strftime(clock, sizeof(clock), "%a %H:%M:%S", &local);
    }
    panelLabel(clock, x + 12, y + 34, 190, C_CYAN);
  }
  if (hadTZ) setenv("TZ", oldTZ.c_str(), 1);
  else unsetenv("TZ");
  tzset();
}

static void scanPhotos() {
  photoCount = photoIndex = 0;
  photosScanned = true;
  if (!LittleFS.begin(false)) return;
  File folder = LittleFS.open("/photos");
  if (!folder || !folder.isDirectory()) return;
  while (photoCount < 24) {
    File entry = folder.openNextFile();
    if (!entry) break;
    String path = entry.path();
    String extension = path;
    extension.toLowerCase();
    if (!entry.isDirectory() && (extension.endsWith(".jpg") || extension.endsWith(".jpeg"))) {
      photoPaths[photoCount++] = path;
    }
  }
}

static void drawPhotosPanel() {
  if (!photosScanned) scanPhotos();
  drawSubHeader("Photos", "< Apps", "Reload", C_BLUE);
  drawFrostGlassCard(14, 44, 452, 262, 22, true, C_WHITE);
  canvas.setFont(&fonts::FreeSerifBold12pt7b);
  if (!photoCount) {
    glassText("No photos yet", 240, 144);
    canvas.setFont(&fonts::FreeSerif9pt7b);
    glassText("Add JPEGs to the /photos folder in LittleFS", 240, 178, C_CYAN);
    return;
  }
  if (!canvas.drawJpgFile(LittleFS, photoPaths[photoIndex].c_str(), 240, 157,
                          424, 206, 0, 0, 1.0f, 0.0f, lgfx::middle_center)) {
    glassText("This JPEG could not be opened", 240, 158);
  }
  canvas.setFont(&fonts::FreeSerif9pt7b);
  glassText("<", 38, 281, C_CYAN);
  glassText(">", 442, 281, C_CYAN);
  glassText(String(photoIndex + 1) + " / " + photoCount, 240, 281);
}
