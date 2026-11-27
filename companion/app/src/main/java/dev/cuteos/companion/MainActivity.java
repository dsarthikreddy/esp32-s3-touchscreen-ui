package dev.cuteos.companion;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothManager;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.provider.Settings;
import android.widget.Button;
import android.widget.EditText;
import android.text.InputType;
import android.widget.LinearLayout;
import android.widget.TextView;

public final class MainActivity extends Activity {
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        LinearLayout content = new LinearLayout(this);
        content.setOrientation(LinearLayout.VERTICAL);
        int padding = Math.round(24 * getResources().getDisplayMetrics().density);
        content.setPadding(padding, padding * 2, padding, padding);
        TextView title = new TextView(this);
        title.setText(R.string.app_name);
        title.setTextSize(28);
        content.addView(title);
        TextView detail = new TextView(this);
        detail.setText(R.string.pairing_help);
        detail.setPadding(0, padding, 0, padding);
        content.addView(detail);
        addButton(content, "Notification access", () ->
            startActivity(new Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS)));
        addButton(content, "Connect to cuteOS", this::connect);
        addButton(content, "Disconnect", () -> stopService(new Intent(this, LinkService.class)));
        addButton(content, "Forget display", () -> {
            stopService(new Intent(this, LinkService.class));
            getSharedPreferences("pairing", MODE_PRIVATE).edit().remove("display").apply();
        });
        addButton(content, "Market data keys", this::marketKeys);
        setContentView(content);
    }

    private void marketKeys() {
        var preferences = getSharedPreferences("markets", MODE_PRIVATE);
        LinearLayout fields = new LinearLayout(this);
        fields.setOrientation(LinearLayout.VERTICAL);
        EditText crypto = keyField("CoinGecko demo API key", preferences.getString("crypto", ""));
        EditText stocks = keyField("Twelve Data API key", preferences.getString("stocks", ""));
        fields.addView(crypto);
        fields.addView(stocks);
        new AlertDialog.Builder(this).setTitle("Market data")
                .setView(fields).setNegativeButton("Cancel", null)
                .setPositiveButton("Save", (dialog, which) -> preferences.edit()
                        .putString("crypto", crypto.getText().toString().trim())
                        .putString("stocks", stocks.getText().toString().trim()).apply()).show();
    }

    private EditText keyField(String description, String value) {
        EditText field = new EditText(this);
        field.setHint(description);
        field.setSingleLine(true);
        field.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD);
        field.setText(value);
        return field;
    }

    private void addButton(LinearLayout parent, String title, Runnable action) {
        Button button = new Button(this);
        button.setText(title);
        button.setOnClickListener(view -> action.run());
        parent.addView(button);
    }

    private void connect() {
        if (checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED
                || checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN) != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(new String[]{Manifest.permission.BLUETOOTH_CONNECT,
                    Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.POST_NOTIFICATIONS}, 1);
            return;
        }
        BluetoothAdapter bluetooth = getSystemService(BluetoothManager.class).getAdapter();
        if (!bluetooth.isEnabled()) {
            startActivity(new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE));
            return;
        }
        startForegroundService(new Intent(this, LinkService.class));
    }

    @Override public void onRequestPermissionsResult(int request, String[] permissions, int[] results) {
        super.onRequestPermissionsResult(request, permissions, results);
        if (request == 1 && results.length >= 2 && results[0] == PackageManager.PERMISSION_GRANTED
                && results[1] == PackageManager.PERMISSION_GRANTED) connect();
    }
}
