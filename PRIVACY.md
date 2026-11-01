# Privacy Policy

**OBS Vtuber Integration** (the "plugin"), by BearApps / LB-bear-dev
Last updated: 2026-10-30

This plugin runs inside OBS Studio on your own computer. It finds out who is speaking in a Discord voice channel through the Discord desktop app running on that same computer, so OBS can react to it. We do not run any servers for it, and we do not collect, receive or sell any of your data.

## How it gets voice information

The plugin uses an open IPC connection discord uses to talk to programs, such as StreamKit.

StreamKit talks to your local Discord app itself and the plugin reads messages Discord sends to those pages.

## What the plugin accesses

The plugin reads:

- For each person in the voice channel picked in the dock: user ID, username, display name, server nickname, and muted/deafened state
- Events when someone joins or leaves that channel, changes their mute or deafen state, or starts or stops speaking
- The names and IDs of your servers and their voice channels, when the dock lists them

It does not send messages, join or leave channels, change any settings, or read message content, friends lists or DMs.

## What is kept, and where

- **Voice channel information**, including the server and channel lists, is held in memory while OBS is running and is discarded when OBS closes. The plugin does not write it to disk and does not send it anywhere.
- **Your Discord Voice settings** (the IDs of the server and voice channel picked in the dock) are saved in the plugin's OBS config folder, `%APPDATA%\obs-studio\plugin_config\OBS-VtuberBear`.
- **Logs.** The plugin writes status and error messages to the OBS log file on your computer, including how many people are in the channel being watched and the paths of the plugin's settings files (which contain your Windows user name). They don't include any personally identifying information. When Discord turns a request down, only its numeric error code is logged.

## What leaves your computer

The plugin itself makes no network requests. The StreamKit page it loads comes from `https://streamkit.discord.com` and that page makes its own requests to Discord, for example to load avatars, just as it would in a browser source.
The plugin keeps the page from navigating to any other site. There is no analytics, telemetry or advertising, and no data is shared with third parties.

## Your choices

- **Revoke access:** Discord → User Settings → Authorized Apps → remove StreamKit.
- **Stream on a different channel:** pick it in the Discord Voice dock (View → Docks → Discord Voice).
- **Delete stored data:** close OBS, delete the `%APPDATA%\obs-studio\plugin_config\OBS-VtuberBear` folder (and `plugin_config\CharacterControlOBS`, if an older version left one), then delete the plugin from your OBS plugins folder.
- **Stop it running:** disable or remove the plugin; nothing runs outside OBS.

## Children

The plugin is not directed at children under 13 (or the minimum age in your country).

## Changes

If this policy changes, the new version will be published in this repository with an updated date.

## Contact

lb_burr@outlook.com for any issues or questions
