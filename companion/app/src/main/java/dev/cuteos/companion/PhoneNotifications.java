package dev.cuteos.companion;

import android.app.Notification;
import android.service.notification.NotificationListenerService;
import android.service.notification.StatusBarNotification;

public final class PhoneNotifications extends NotificationListenerService {
    @Override public void onNotificationPosted(StatusBarNotification entry) {
        Notification notification = entry.getNotification();
        boolean own = getPackageName().equals(entry.getPackageName());
        boolean test = MainActivity.TEST_NOTIFICATION_TAG.equals(entry.getTag())
                && entry.getId() == MainActivity.TEST_NOTIFICATION_ID;
        if ((own && !test) || (notification.flags & (Notification.FLAG_GROUP_SUMMARY | Notification.FLAG_ONGOING_EVENT)) != 0
                || notification.visibility == Notification.VISIBILITY_SECRET || notification.extras == null) return;
        CharSequence title = notification.extras.getCharSequence(Notification.EXTRA_TITLE);
        CharSequence text = notification.extras.getCharSequence(Notification.EXTRA_TEXT);
        if (title == null && text == null) return;
        LinkService.forward(entry.getPackageName(), title, text);
    }
}
