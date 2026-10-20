# Get a YouTube Data API key

Subscriber Counter reads public channel statistics through the free
[YouTube Data API v3](https://developers.google.com/youtube/v3). You need an API key once.
It takes about five minutes and does not cost anything.

## 1. Create a project

1. Open the [Google Cloud Console](https://console.cloud.google.com/) and sign in with any Google account.
2. In the project picker at the top, choose **New project**, give it a name such as `flipper-counter` and create it.

## 2. Enable the YouTube Data API v3

1. Go to **APIs & Services → Library**.
2. Search for **YouTube Data API v3**, open it and click **Enable**.

## 3. Create the key

1. Go to **APIs & Services → Credentials**.
2. Click **Create credentials → API key**. The key starts with `AIza`.

## 4. Restrict the key (recommended)

The key ends up in plain text on your SD card, so limit what it can do:

1. Click the key you just created.
2. Under **API restrictions**, choose **Restrict key** and select only **YouTube Data API v3**.
3. Save.

Do not restrict it to IP addresses or websites – requests come from your computer through the
desktop helper.

## 5. Put it on the Flipper

Open `SD card/apps_data/sub_counter/config.txt` with qFlipper and fill in:

```ini
api_key=AIza...your-key...
channel=@yourchannel
```

`channel` accepts your handle (with or without `@`) or the channel ID that starts with `UC`
(24 characters, shown at [youtube.com/account_advanced](https://www.youtube.com/account_advanced)).

## Quota

Each key gets 10,000 units per day for free. One update costs 1 unit, so even the shortest
interval (1 minute = 1,440 units per day) stays well inside the limit.

## If you shared your key by accident

Delete it under **APIs & Services → Credentials** and create a new one. Anyone who has the key can
use your daily quota.
