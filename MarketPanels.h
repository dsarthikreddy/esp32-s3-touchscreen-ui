#pragma once

static bool stockMarkets;
static int marketPage;
static int marketActiveRequest;
static String marketMessage = "Pair your phone to load prices";
struct MarketRow { String symbol, price, change, graph; };
static MarketRow marketRows[4];

static void loadMarketPage() {
  for (auto& row : marketRows) row = MarketRow{};
  marketActiveRequest = 0;
  if (phoneReady) {
    uint8_t request = (stockMarkets ? 64 : 16) + marketPage;
    if (xQueueSend(PhoneLink::commands, &request, 0) == pdPASS) {
      marketActiveRequest = request;
      marketMessage = "Loading prices from phone...";
    } else marketMessage = "Link busy. Tap Refresh.";
  } else marketMessage = "Pair your phone to load prices";
  uiDirty = true;
}

static void handleMarketFrame(char* fields[], size_t count) {
  if (atoi(fields[1]) != marketActiveRequest || !marketActiveRequest) return;
  if (strcmp(fields[0], "Q") == 0 && count == 7) {
    int row = atoi(fields[2]);
    if (row < 0 || row >= 4) return;
    marketRows[row] = {fields[3], fields[4], fields[5], fields[6]};
  } else if (strcmp(fields[0], "E") == 0 && count == 3) {
    marketMessage = fields[2];
  }
  if (currentScreen == SCR_MARKETS) uiDirty = true;
}

static void drawMarketsPanel() {
  drawSubHeader(stockMarkets ? "Stocks" : "Crypto", "< Home", "Refresh", C_BLUE);
  drawFrostGlassCard(14, 46, 452, 30, 12, true, C_WHITE);
  canvas.setFont(&fonts::FreeSerif9pt7b);
  glassText(stockMarkets ? "View crypto" : "View stocks", 95, 61, C_CYAN);
  panelLabel(marketMessage, 172, 61, 270, C_WHITE);
  for (int i = 0; i < 4; ++i) {
    int y = 82 + i * 49;
    drawFrostGlassCard(14, y, 452, 45, 12, true, C_WHITE);
    if (!marketRows[i].symbol.length()) continue;
    const auto& row = marketRows[i];
    canvas.setFont(&fonts::FreeSerifBold9pt7b);
    panelLabel(row.symbol, 27, y + 22, 130, C_WHITE);
    canvas.setFont(&fonts::FreeSerif9pt7b);
    panelLabel(row.price, 160, y + 16, 166, C_WHITE);
    uint16_t tint = row.change.startsWith("-") ? C_ROSE : C_CYAN;
    panelLabel(row.change, 160, y + 31, 166, tint);
    int count = row.graph.length();
    for (int k = 1; k < count && count <= 12; ++k) {
      canvas.drawLine(336 + (k - 1) * 110 / (count - 1), y + 35 - constrain(row.graph[k - 1] - 'A', 0, 25),
                       336 + k * 110 / (count - 1), y + 35 - constrain(row.graph[k] - 'A', 0, 25), tint);
    }
  }
  glassText("<", 54, 297, C_CYAN);
  glassText(String(marketPage + 1) + " / " + (stockMarkets ? 15 : 25), 240, 297);
  glassText(">", 426, 297, C_CYAN);
}
