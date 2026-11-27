package dev.cuteos.companion;

import android.content.Context;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.SystemClock;
import org.json.JSONArray;
import org.json.JSONObject;
import java.io.ByteArrayOutputStream;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import javax.net.ssl.HttpsURLConnection;

final class Markets {
    private static final String[] STOCKS = {
        "AAPL","MSFT","NVDA","GOOGL","AMZN","META","TSLA","AVGO","AMD","INTC","ORCL","IBM",
        "CRM","ADBE","NFLX","UBER","JPM","BAC","GS","MS","V","MA","WMT","COST",
        "TGT","KO","PEP","MCD","SBUX","NKE","DIS","XOM","CVX","COP","CAT","BA",
        "GE","WFC","UNH","JNJ","PFE","MRK","ABBV","LLY","PG","HD","LOW","UPS",
        "RELIANCE:NSE","TCS:NSE","INFY:NSE","HDFCBANK:NSE","ICICIBANK:NSE","SBIN:NSE",
        "BHARTIARTL:NSE","ITC:NSE","LT:NSE","HINDUNILVR:NSE","MARUTI:NSE","SUNPHARMA:NSE"
    };
    private static final int PAGE_SIZE = 4;
    private static final long FRESH_MS = 120000;
    private final SharedPreferences settings;
    private final long[] fetchedAt = new long[128];
    private final List<List<String>> pages = new ArrayList<>(128);

    Markets(Context context) {
        settings = context.getSharedPreferences("markets", Context.MODE_PRIVATE);
        for (int i = 0; i < 128; ++i) pages.add(List.of());
    }

    List<String> page(int request) {
        boolean stocks = request >= 64;
        int index = request - (stocks ? 64 : 16);
        if (index < 0 || index >= (stocks ? STOCKS.length / PAGE_SIZE : 25)) return List.of();
        if (fetchedAt[request] != 0 && SystemClock.elapsedRealtime() - fetchedAt[request] < FRESH_MS)
            return pages.get(request);
        String key = settings.getString(stocks ? "stocks" : "crypto", "").trim();
        if (key.isEmpty()) return List.of("E\t" + request + "\tSet market API keys in the phone app");

        List<String> records = new ArrayList<>();
        try {
            if (stocks) loadStocks(records, request, index, key);
            else loadCrypto(records, request, index, key);
        } catch (Exception networkOrParseError) {
            if (records.isEmpty()) return List.of("E\t" + request + "\tProvider unavailable or quota exceeded");
        }
        if (records.isEmpty()) return List.of("E\t" + request + "\tNo quotes returned");
        records.add("E\t" + request + "\t" + (stocks ? "Twelve Data / latest daily bar" : "CoinGecko / USD"));
        List<String> result = List.copyOf(records);
        pages.set(request, result);
        fetchedAt[request] = SystemClock.elapsedRealtime();
        return result;
    }

    private static void loadCrypto(List<String> results, int request, int index, String key) throws Exception {
        String body = fetch("https://api.coingecko.com/api/v3/coins/markets?vs_currency=usd"
                + "&order=market_cap_desc&per_page=4&sparkline=true&page=" + (index + 1), key);
        JSONArray assets = new JSONArray(body);
        for (int row = 0; row < assets.length() && row < PAGE_SIZE; ++row) {
            JSONObject asset = assets.getJSONObject(row);
            JSONArray history = asset.getJSONObject("sparkline_in_7d").getJSONArray("price");
            double[] points = new double[Math.min(history.length(), 12)];
            for (int i = 0; i < points.length; ++i) {
                int source = points.length == 1 ? 0 : i * (history.length() - 1) / (points.length - 1);
                points[i] = history.getDouble(source);
            }
            results.add(record(request, row, asset.getString("symbol").toUpperCase(Locale.ROOT), "USD",
                    asset.getDouble("current_price"), asset.optDouble("price_change_percentage_24h", Double.NaN), points));
        }
    }

    private static void loadStocks(List<String> results, int request, int index, String key) throws Exception {
        for (int row = 0; row < PAGE_SIZE; ++row) {
            String symbol = STOCKS[index * PAGE_SIZE + row];
            String address = "https://api.twelvedata.com/time_series?interval=1day&outputsize=12&symbol="
                    + Uri.encode(symbol.replace(":NSE", "")) + (symbol.endsWith(":NSE") ? "&exchange=NSE" : "")
                    + "&apikey=" + Uri.encode(key);
            JSONObject response = new JSONObject(fetch(address, null));
            JSONArray history = response.optJSONArray("values");
            if (history == null || history.length() < 2) {
                results.add("Q\t" + request + "\t" + row + "\t" + symbol + "\tUnavailable\t\t");
                continue;
            }
            double[] points = new double[Math.min(history.length(), 12)];
            for (int i = 0; i < points.length; ++i) {
                points[i] = history.getJSONObject(points.length - i - 1).getDouble("close");
            }
            double last = points[points.length - 1], previous = points[points.length - 2];
            results.add(record(request, row, symbol.replace(":NSE", ""),
                    response.getJSONObject("meta").optString("currency", ""),
                    last, previous > 0 ? (last / previous - 1) * 100 : Double.NaN, points));
        }
    }

    private static String record(int request, int row, String symbol, String currency, double price,
            double change, double[] history) {
        String formatted = String.format(Locale.ROOT, "%s %." + (price < 0.01 ? "8" : "2") + "f", currency, price);
        String percent = Double.isFinite(change) ? String.format(Locale.ROOT, "%+.2f%%", change) : "";
        double min = Double.POSITIVE_INFINITY, max = Double.NEGATIVE_INFINITY;
        for (double point : history) {
            min = Math.min(min, point);
            max = Math.max(max, point);
        }
        StringBuilder sparkline = new StringBuilder();
        for (double point : history) {
            int height = max > min ? (int)Math.round(25 * (point - min) / (max - min)) : 12;
            sparkline.append((char)('A' + height));
        }
        return "Q\t" + request + "\t" + row + "\t" + symbol + "\t" + formatted + "\t" + percent + "\t" + sparkline;
    }

    private static String fetch(String address, String demoKey) throws Exception {
        HttpsURLConnection connection = (HttpsURLConnection) new URL(address).openConnection();
        connection.setConnectTimeout(4500);
        connection.setReadTimeout(4500);
        connection.setInstanceFollowRedirects(false);
        if (demoKey != null) connection.setRequestProperty("x-cg-demo-api-key", demoKey);
        try {
            if (connection.getResponseCode() != 200) throw new java.io.IOException("Provider response");
            try (var input = connection.getInputStream(); var output = new ByteArrayOutputStream()) {
                byte[] buffer = new byte[4096];
                int count;
                while ((count = input.read(buffer)) != -1) {
                    if (output.size() + count > 65536) throw new java.io.IOException("Response too large");
                    output.write(buffer, 0, count);
                }
                return output.toString(StandardCharsets.UTF_8);
            }
        } finally {
            connection.disconnect();
        }
    }
}
