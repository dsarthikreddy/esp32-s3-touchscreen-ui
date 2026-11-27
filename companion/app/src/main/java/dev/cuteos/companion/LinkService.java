package dev.cuteos.companion;

import android.annotation.SuppressLint;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothStatusCodes;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.media.AudioManager;
import android.media.MediaMetadata;
import android.media.session.MediaController;
import android.media.session.MediaSessionManager;
import android.media.session.PlaybackState;
import android.os.BatteryManager;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.ParcelUuid;
import android.os.SystemClock;
import android.util.Base64;
import java.io.ByteArrayOutputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayDeque;
import java.util.Arrays;
import java.util.List;
import java.util.UUID;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

@SuppressLint("MissingPermission")
public final class LinkService extends Service {
    private static final UUID SERVICE = uuid(0);
    private static final UUID INBOX = uuid(1);
    private static final UUID COMMAND = uuid(2);
    private static final int NOTIFICATION_ID = 1;
    private static volatile LinkService running;

    private final Handler handler = new Handler(Looper.getMainLooper());
    private final ArrayDeque<byte[]> pending = new ArrayDeque<>();
    private BluetoothLeScanner scanner;
    private BluetoothGatt gatt;
    private BluetoothGattCharacteristic inbox;
    private BluetoothGattCharacteristic command;
    private boolean scanning;
    private boolean ready;
    private boolean busy;
    private byte[] frame;
    private int offset;
    private int sentBytes;
    private int battery = -1;
    private boolean plugged;
    private String lastMedia = "";
    private String lastArtworkKey = "";
    private boolean lastArtworkAvailable;
    private String artworkData = "";
    private boolean artworkHeaderPending;
    private int artworkOffset;
    private int artworkRecordsSinceRead;
    private static final int ART_SIZE = 72;
    private static final int ART_MAX_BYTES = 2200;
    private static final int ART_CHUNK = 176;
    private int lastBattery = -2;
    private boolean lastPlugged;
    private Markets markets;
    private final ExecutorService marketRequests = Executors.newSingleThreadExecutor();
    private int latestMarketRequest;
    private boolean marketBusy;

    private static UUID uuid(int index) {
        return UUID.fromString("702d201" + index + "-f69e-48e6-ab05-05ece629beb5");
    }

    static boolean isConnected() {
        LinkService service = running;
        return service != null && service.ready;
    }

    static void forward(String app, CharSequence title, CharSequence message) {
        LinkService service = running;
        if (service != null) service.handler.post(() -> service.enqueue(
                "N\t" + field(app, 40) + "\t" + field(title, 55) + "\t" + field(message, 100)));
    }

    private static String field(CharSequence text, int maxBytes) {
        if (text == null) return "";
        String value = text.toString().replaceAll("[\\p{Cntrl}]", " ").trim();
        byte[] bytes = value.getBytes(StandardCharsets.UTF_8);
        if (bytes.length <= maxBytes) return value;
        int length = maxBytes;
        while ((bytes[length] & 0xc0) == 0x80) length--;
        return new String(bytes, 0, length, StandardCharsets.UTF_8);
    }

    @Override public IBinder onBind(Intent intent) { return null; }

    @Override public void onCreate() {
        super.onCreate();
        NotificationManager notifications = getSystemService(NotificationManager.class);
        notifications.createNotificationChannel(new NotificationChannel(
                "link", "Display connection", NotificationManager.IMPORTANCE_LOW));
        startForeground(NOTIFICATION_ID, notification("Looking for your display"));
        IntentFilter filter = new IntentFilter(Intent.ACTION_BATTERY_CHANGED);
        filter.addAction(BluetoothDevice.ACTION_BOND_STATE_CHANGED);
        registerReceiver(events, filter, Context.RECEIVER_EXPORTED);
        markets = new Markets(this);
        running = this;
        handler.post(tick);
    }

    @Override public int onStartCommand(Intent intent, int flags, int id) {
        if (gatt == null) scan();
        return START_NOT_STICKY;
    }

    @Override public void onDestroy() {
        running = null;
        unregisterReceiver(events);
        handler.removeCallbacksAndMessages(null);
        marketRequests.shutdownNow();
        stopScan();
        if (gatt != null) gatt.close();
        gatt = null;
        super.onDestroy();
    }

    private Notification notification(String status) {
        PendingIntent open = PendingIntent.getActivity(this, 0, new Intent(this, MainActivity.class),
                PendingIntent.FLAG_IMMUTABLE);
        return new Notification.Builder(this, "link").setContentTitle("cuteOS Companion")
                .setContentText(status).setSmallIcon(android.R.drawable.ic_dialog_info)
                .setContentIntent(open).setOngoing(true).build();
    }

    private void status(String message) {
        getSystemService(NotificationManager.class).notify(NOTIFICATION_ID, notification(message));
    }

    private void scan() {
        if (scanning || gatt != null) return;
        scanner = getSystemService(BluetoothManager.class).getAdapter().getBluetoothLeScanner();
        if (scanner == null) {
            handler.postDelayed(retry, 4000);
            return;
        }
        scanning = true;
        scanner.startScan(List.of(new ScanFilter.Builder().setServiceUuid(new ParcelUuid(SERVICE)).build()),
                new ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_POWER).build(), scanCallback);
        handler.postDelayed(scanTimeout, 20000);
    }

    private final Runnable retry = this::scan;
    private final Runnable scanTimeout = () -> {
        stopScan();
        if (gatt == null) handler.postDelayed(retry, 4000);
    };

    private void stopScan() {
        handler.removeCallbacks(scanTimeout);
        if (scanning) scanner.stopScan(scanCallback);
        scanning = false;
    }

    private final ScanCallback scanCallback = new ScanCallback() {
        @Override public void onScanResult(int type, ScanResult result) {
            handler.post(() -> {
                if (!scanning || gatt != null) return;
                String preferred = getSharedPreferences("pairing", MODE_PRIVATE).getString("display", "");
                if (!preferred.isEmpty() && !preferred.equals(result.getDevice().getAddress())) return;
                stopScan();
                gatt = result.getDevice().connectGatt(LinkService.this, false, callback,
                        BluetoothDevice.TRANSPORT_LE);
                handler.postDelayed(linkTimeout, 25000);
            });
        }

        @Override public void onScanFailed(int error) {
            handler.post(() -> {
                scanning = false;
                handler.removeCallbacks(scanTimeout);
                handler.postDelayed(retry, 4000);
            });
        }
    };

    private final BroadcastReceiver events = new BroadcastReceiver() {
        @Override public void onReceive(Context context, Intent intent) {
            if (Intent.ACTION_BATTERY_CHANGED.equals(intent.getAction())) {
                int level = intent.getIntExtra(BatteryManager.EXTRA_LEVEL, -1);
                int scale = intent.getIntExtra(BatteryManager.EXTRA_SCALE, 100);
                battery = level < 0 || scale <= 0 ? -1 : level * 100 / scale;
                plugged = intent.getIntExtra(BatteryManager.EXTRA_PLUGGED, 0) != 0;
                return;
            }
            BluetoothDevice device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE, BluetoothDevice.class);
            if (gatt == null || !gatt.getDevice().equals(device)) return;
            int bond = intent.getIntExtra(BluetoothDevice.EXTRA_BOND_STATE, BluetoothDevice.ERROR);
            if (bond == BluetoothDevice.BOND_BONDED && inbox != null) activate();
            if (bond == BluetoothDevice.BOND_NONE && !ready) stopSelf();
        }
    };

    private final Runnable linkTimeout = () -> {
        if (!ready) resetLink();
    };
    private final Runnable operationTimeout = this::resetLink;

    private void resetLink() {
        handler.removeCallbacks(linkTimeout);
        handler.removeCallbacks(operationTimeout);
        handler.removeCallbacks(pump);
        if (gatt != null) gatt.close();
        gatt = null;
        inbox = command = null;
        ready = busy = false;
        frame = null;
        pending.clear();
        artworkHeaderPending = false;
        artworkData = "";
        artworkRecordsSinceRead = 0;
        latestMarketRequest = 0;
        offset = 0;
        status("Looking for your display");
        handler.postDelayed(retry, 3000);
    }

    private void activate() {
        if (ready) return;
        getSharedPreferences("pairing", MODE_PRIVATE).edit()
                .putString("display", gatt.getDevice().getAddress()).apply();
        handler.removeCallbacks(linkTimeout);
        ready = true;
        lastBattery = -2;
        lastMedia = "";
        lastArtworkKey = "";
        lastArtworkAvailable = false;
        status("Display connected");
        handler.postDelayed(pump, 500);
    }

    private final BluetoothGattCallback callback = new BluetoothGattCallback() {
        @Override public void onConnectionStateChange(BluetoothGatt link, int status, int state) {
            handler.post(() -> {
                if (link != gatt) return;
                if (status != BluetoothGatt.GATT_SUCCESS || state == BluetoothProfile.STATE_DISCONNECTED) resetLink();
                else if (state == BluetoothProfile.STATE_CONNECTED && !link.discoverServices()) resetLink();
            });
        }

        @Override public void onServicesDiscovered(BluetoothGatt link, int status) {
            handler.post(() -> {
                if (link != gatt) return;
                var service = link.getService(SERVICE);
                if (status != BluetoothGatt.GATT_SUCCESS || service == null) {
                    resetLink();
                    return;
                }
                inbox = service.getCharacteristic(INBOX);
                command = service.getCharacteristic(COMMAND);
                if (inbox == null || command == null) resetLink();
                else if (link.getDevice().getBondState() == BluetoothDevice.BOND_BONDED) activate();
                else if (link.getDevice().getBondState() == BluetoothDevice.BOND_NONE
                        && !link.getDevice().createBond()) resetLink();
            });
        }

        @Override public void onCharacteristicWrite(BluetoothGatt link, BluetoothGattCharacteristic which, int status) {
            handler.post(() -> {
                if (link != gatt) return;
                handler.removeCallbacks(operationTimeout);
                if (status != BluetoothGatt.GATT_SUCCESS) { resetLink(); return; }
                offset += sentBytes;
                if (offset == frame.length) {
                    frame = null;
                    offset = 0;
                }
                busy = false;
                handler.post(pump);
            });
        }

        @Override public void onCharacteristicRead(BluetoothGatt link, BluetoothGattCharacteristic which,
                byte[] value, int status) {
            handler.post(() -> {
                if (link != gatt) return;
                handler.removeCallbacks(operationTimeout);
                if (status != BluetoothGatt.GATT_SUCCESS) { resetLink(); return; }
                busy = false;
                if (value.length == 1) perform(value[0]);
                handler.postDelayed(pump, 350);
            });
        }
    };

    private void enqueue(String record) {
        if (!ready) return;
        if (pending.size() >= 24) pending.removeFirst();
        pending.addLast((record + "\n").getBytes(StandardCharsets.UTF_8));
    }

    private final Runnable pump = this::pumpGatt;
    private void pumpGatt() {
        if (!ready || busy) return;
        if (frame == null) frame = pending.pollFirst();
        if (frame == null && artworkRecordsSinceRead < 3) frame = nextArtworkRecord();
        if (frame == null) {
            artworkRecordsSinceRead = 0;
            busy = gatt.readCharacteristic(command);
        } else {
            sentBytes = Math.min(20, frame.length - offset);
            byte[] chunk = Arrays.copyOfRange(frame, offset, offset + sentBytes);
            busy = gatt.writeCharacteristic(inbox, chunk, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)
                    == BluetoothStatusCodes.SUCCESS;
        }
        if (busy) handler.postDelayed(operationTimeout, 6000);
        else resetLink();
    }

    private final Runnable tick = this::syncState;
    private void syncState() {
        if (ready) {
            if (lastBattery != battery || lastPlugged != plugged) {
                enqueue("B\t" + battery + "\t" + (plugged ? 1 : 0));
                lastBattery = battery;
                lastPlugged = plugged;
            }
            AudioManager audio = getSystemService(AudioManager.class);
            int maximum = Math.max(1, audio.getStreamMaxVolume(AudioManager.STREAM_MUSIC));
            int volume = 100 * audio.getStreamVolume(AudioManager.STREAM_MUSIC) / maximum;
            MediaController controller = activePlayer();
            MediaMetadata meta = controller == null ? null : controller.getMetadata();
            PlaybackState state = controller == null ? null : controller.getPlaybackState();
            String media = "M\t" + volume + "\t" + field(meta == null ? null :
                    meta.getText(MediaMetadata.METADATA_KEY_TITLE), 74) + "\t" +
                    field(meta == null ? null : meta.getText(MediaMetadata.METADATA_KEY_ARTIST), 62) + "\t" +
                    (state == null ? 0 : state.getActions()) + "\t" +
                    (state != null && state.getState() == PlaybackState.STATE_PLAYING ? 1 : 0);
            if (!media.equals(lastMedia)) {
                enqueue(media);
                lastMedia = media;
            }
            syncArtwork(meta);
        }
        handler.postDelayed(tick, 1000);
    }

    private void syncArtwork(MediaMetadata metadata) {
        String key = metadata == null ? "" : metadata.getString(MediaMetadata.METADATA_KEY_MEDIA_ID) + "\n"
                + metadata.getString(MediaMetadata.METADATA_KEY_TITLE) + "\n"
                + metadata.getString(MediaMetadata.METADATA_KEY_ARTIST) + "\n"
                + metadata.getString(MediaMetadata.METADATA_KEY_ALBUM);
        Bitmap bitmap = null;
        if (metadata != null) {
            bitmap = metadata.getBitmap(MediaMetadata.METADATA_KEY_ART);
            if (bitmap == null) bitmap = metadata.getBitmap(MediaMetadata.METADATA_KEY_ALBUM_ART);
            if (bitmap == null) bitmap = metadata.getBitmap(MediaMetadata.METADATA_KEY_DISPLAY_ICON);
        }
        boolean available = bitmap != null && !bitmap.isRecycled();
        if (key.equals(lastArtworkKey) && available == lastArtworkAvailable) return;
        lastArtworkKey = key;
        lastArtworkAvailable = available;
        artworkData = available ? encodeArtwork(bitmap) : "";
        artworkOffset = 0;
        artworkHeaderPending = true;
    }

    private static String encodeArtwork(Bitmap bitmap) {
        Bitmap image = Bitmap.createBitmap(ART_SIZE, ART_SIZE, Bitmap.Config.ARGB_8888);
        Bitmap source = bitmap;
        try {
            if (bitmap.getConfig() == Bitmap.Config.HARDWARE) {
                source = bitmap.copy(Bitmap.Config.ARGB_8888, false);
                if (source == null) return "";
            }
            int crop = Math.min(source.getWidth(), source.getHeight());
            int x = (source.getWidth() - crop) / 2;
            int y = (source.getHeight() - crop) / 2;
            Canvas canvas = new Canvas(image);
            canvas.drawColor(Color.BLACK);
            canvas.drawBitmap(source, new Rect(x, y, x + crop, y + crop),
                    new Rect(0, 0, ART_SIZE, ART_SIZE), new Paint(Paint.FILTER_BITMAP_FLAG));
            var output = new ByteArrayOutputStream(ART_MAX_BYTES);
            for (int quality : new int[]{76, 59, 42, 25, 14}) {
                output.reset();
                if (image.compress(Bitmap.CompressFormat.JPEG, quality, output)
                        && output.size() <= ART_MAX_BYTES) {
                    return Base64.encodeToString(output.toByteArray(), Base64.NO_WRAP);
                }
            }
            return "";
        } catch (IllegalArgumentException invalidBitmap) {
            return "";
        } finally {
            image.recycle();
            if (source != null && source != bitmap) source.recycle();
        }
    }

    private byte[] nextArtworkRecord() {
        String record;
        if (artworkHeaderPending) {
            artworkHeaderPending = false;
            record = "A\t" + artworkData.length() + "\n";
        } else if (artworkOffset < artworkData.length()) {
            int next = Math.min(artworkOffset + ART_CHUNK, artworkData.length());
            record = "I\t" + artworkOffset + "\t" + artworkData.substring(artworkOffset, next) + "\n";
            artworkOffset = next;
        } else return null;
        artworkRecordsSinceRead++;
        return record.getBytes(StandardCharsets.UTF_8);
    }

    private MediaController activePlayer() {
        List<MediaController> players;
        try {
            players = getSystemService(MediaSessionManager.class).getActiveSessions(
                    new ComponentName(this, PhoneNotifications.class));
        } catch (SecurityException missingNotificationAccess) {
            return null;
        }
        for (MediaController player : players) {
            var state = player.getPlaybackState();
            if (state != null && state.getState() == PlaybackState.STATE_PLAYING) return player;
        }
        return players.isEmpty() ? null : players.get(0);
    }

    private void requestMarket(int request) {
        latestMarketRequest = request;
        if (marketBusy) return;
        marketBusy = true;
        marketRequests.submit(() -> {
            List<String> records = markets.page(request);
            handler.post(() -> {
                marketBusy = false;
                if (running != this || !ready || latestMarketRequest == 0) return;
                if (latestMarketRequest != request) requestMarket(latestMarketRequest);
                else records.forEach(this::enqueue);
            });
        });
    }

    private void perform(int action) {
        if (action == 0) return;
        if ((action >= 16 && action <= 40) || (action >= 64 && action <= 78)) {
            requestMarket(action);
            return;
        }
        if (action == 4 || action == 5) {
            getSystemService(AudioManager.class).adjustStreamVolume(AudioManager.STREAM_MUSIC,
                    action == 4 ? AudioManager.ADJUST_LOWER : AudioManager.ADJUST_RAISE, 0);
            return;
        }
        MediaController player = activePlayer();
        PlaybackState state = player == null ? null : player.getPlaybackState();
        if (state == null) return;
        var controls = player.getTransportControls();
        long available = state.getActions();
        switch (action) {
            case 1 -> {
                if (state.getState() == PlaybackState.STATE_PLAYING) {
                    if ((available & (PlaybackState.ACTION_PAUSE | PlaybackState.ACTION_PLAY_PAUSE)) != 0)
                        controls.pause();
                } else if ((available & (PlaybackState.ACTION_PLAY | PlaybackState.ACTION_PLAY_PAUSE)) != 0) {
                    controls.play();
                }
            }
            case 2 -> { if ((available & PlaybackState.ACTION_SKIP_TO_PREVIOUS) != 0) controls.skipToPrevious(); }
            case 3 -> { if ((available & PlaybackState.ACTION_SKIP_TO_NEXT) != 0) controls.skipToNext(); }
            case 6, 7 -> {
                if ((available & PlaybackState.ACTION_SEEK_TO) == 0 || state.getPosition() < 0) return;
                long position = state.getPosition();
                if (state.getState() == PlaybackState.STATE_PLAYING) {
                    position += (long) ((SystemClock.elapsedRealtime() -
                            state.getLastPositionUpdateTime()) * state.getPlaybackSpeed());
                }
                MediaMetadata metadata = player.getMetadata();
                long duration = metadata == null ? 0 : metadata.getLong(MediaMetadata.METADATA_KEY_DURATION);
                long target = Math.max(0, position + (action == 6 ? -15000 : 15000));
                controls.seekTo(duration > 0 ? Math.min(target, duration) : target);
            }
            case 8, 9 -> {
                if ((available & PlaybackState.ACTION_SET_PLAYBACK_SPEED) == 0) return;
                float speed = state.getPlaybackSpeed() > 0 ? state.getPlaybackSpeed() : 1f;
                controls.setPlaybackSpeed(Math.min(3f, Math.max(0.5f, speed + (action == 8 ? -0.25f : 0.25f))));
            }
        }
    }
}
