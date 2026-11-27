#pragma once

// ============================================================================
// Sanitized Configuration Template for cuteOS v2.0
// ============================================================================
// Instructions:
//   1. Copy this file to `secrets.h` in the same folder as `cuteOS.ino`.
//   2. Replace the placeholder values below with your own Wi-Fi and location.
//   3. `secrets.h` is listed in `.gitignore` and will not be pushed to GitHub.
// ============================================================================

// Wi-Fi Credentials (2.4 GHz network required by ESP32-S3)
#define SECRET_WIFI_SSID       "YOUR_WIFI_SSID"
#define SECRET_WIFI_PASS       "YOUR_WIFI_PASSWORD"

// Home Coordinates & Location Labels (Open-Meteo Weather & OSM/Esri Map)
#define SECRET_HOME_LAT        0.00000
#define SECRET_HOME_LON        0.00000
#define SECRET_AREA_NAME       "Home Area"
#define SECRET_CITY_NAME       "Your City"
#define SECRET_REGION_SUB      "Your Region"
#define SECRET_MAP_SUBTITLE    "Your City, Region (Zoom 17)"
#define SECRET_MAP_WIDGET_LBL  "Local Area Map"
#define SECRET_MAP_TILE_LBL    "Area Map"
#define SECRET_WEATHER_TITLE   "LOCAL WEATHER"
#define SECRET_MAP_ROAD_1      "Main Road"
#define SECRET_MAP_ROAD_2      "North Park"
#define SECRET_MAP_ROAD_3      "West Avenue"

// Timezone offset from UTC in seconds (e.g., 0 = UTC, 19800 = UTC+05:30 IST)
#define SECRET_NTP_OFFSET_SEC  0
