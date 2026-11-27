# cuteOS Companion for Android

The companion app bridges a Pixel 9 or another Android 13+ phone to cuteOS over Bluetooth Low Energy. Build or install the APK, then flash the matching firmware from [the repository root](../README.md#2-build-and-upload).

## Install on your Pixel

### Android Studio

1. Extract **`cuteOS.zip`** from the [latest release](https://github.com/dsarthikreddy/esp32-s3-touchscreen-ui/releases/latest). In Android Studio, click **Open** and select **`cuteOS/companion`**, the folder with `settings.gradle` and `app/`. Wait for **Gradle Sync**. If Studio offers to install a missing Android SDK, click **Install**.
2. On the Pixel, open **Settings → About phone**, tap **Build number** seven times, and enter your PIN if asked. Go back to **Settings → System → Developer options** and turn on **USB debugging**.
3. Plug the unlocked Pixel into the computer with a USB data cable. Tap **Allow** on the phone's USB debugging prompt. In Android Studio's top bar, select **Google Pixel 9** and **app**, then click the green **▶ Run** button. Wait for **cuteOS Companion** to open on the phone.
4. If you previously installed the app using this Android Studio setup, use **▶ Run** again to update it.

### APK download

The release includes **`cuteOS-companion.apk`**, a debug build for testing. Use Android Studio if the Pixel blocks the downloaded APK or if you already use an Android Studio build. Keep Play Protect enabled.

## Pair the display

1. On the ESP32 display, swipe up from Home, tap **Phone**, then tap **Pair** to show the six-digit code.
2. Turn on Bluetooth on the Pixel. In the phone app, tap **Connect to cuteOS**, allow **Nearby devices**, and enter the displayed code in Android's pairing prompt.
3. Check the display's **Phone** screen for your Pixel's battery percentage. When you see it, the devices are connected.

Keep the companion's foreground connection service running for battery updates and notification delivery while the phone is idle. Opening the app and tapping **Connect to cuteOS** starts it again after a phone reboot. The display keeps a short list of notifications in memory and clears it when the phone disconnects. Android may redact sensitive notifications. You can stop the link with **Disconnect** or turn off notification access in Android Settings.

On the display, swipe left from Home for **Music**. Play/pause, next/previous, ±15-second seek, and speed controls activate when the active Android player advertises those actions. The volume buttons adjust the phone's media volume. The Phone panel shows the system-reported battery percentage and whether the phone is plugged in.

## Test notifications

1. Connect to the display and open its **Phone** panel. Check that the phone battery percentage is visible.
2. In the Android app, tap **Notification access**, then select and enable **cuteOS Companion** in Android Settings.
3. Tap **Send test notification** and allow the app to post notifications if Android asks. **cuteOS test** should appear in the display's **Phone Notifications** list.
4. Lock the phone and have someone send you a message. Make sure the message produces an Android notification. Check the display again.

The updated companion sends album artwork from players that share it with Android. Start a track and open **Now Playing** on the display. The image can take a few seconds to arrive over Bluetooth. Tracks without shared artwork show a music note.

## Market panels

On the phone, open **Market data keys** and enter a CoinGecko Demo key for crypto or a Twelve Data key for stocks. Keys are kept in the Android app's private settings and sent over HTTPS to their respective providers. The display receives the quote data over the paired BLE connection.

Swipe right through **News → Phone → Markets**. Use the arrows for more symbols and **Refresh** to request the current page. Crypto shows up to 100 assets sorted by market cap, 24-hour change, and a seven-day trend. Stocks cover 60 preset US and Indian symbols and show the latest available quote, percentage change, and a recent daily trend. An asterisk marks a fallback to the latest daily close. Stock exchange coverage, data delays, and refresh limits depend on your provider plan.

## Build from source

Use JDK 17, Gradle 8.11.1, and Android SDK 35. Run these commands from the repository root.

```sh
gradle -p companion assembleDebug lintDebug
adb install -r companion/app/build/outputs/apk/debug/app-debug.apk
```

You can also open `companion/` as a project in Android Studio.
