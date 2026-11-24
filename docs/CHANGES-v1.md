# Sailfish OS on the M52 (m52xq): what is in v1 (2026-11-24)

First release. По-русски: [CHANGES-v1-RU.md](CHANGES-v1-RU.md).

## System

- Native Sailfish OS 5.2.0.22 on the mainline 7.1.13 kernel without libhybris: Lipstick on eglfs_kms + Mesa freedreno, scale
  1.75, auto-rotation, auto-brightness, volume keys and volume bar.
- `/home` is on the userdata partition (about 107 GB on this phone).
- Public installer: the proprietary firmware is taken from the phone's partitions; the size of the image is checked before
  anything is written; no personal data in the image; the user and the start wizard are created at the first boot.
- Messages do not switch the display on.

## Phone

- Calls and SMS on two SIM cards at once: ofono 2.12 with the QRTR/QMI drivers (second modem, NAS/VOICE/WMS bound to the
  subscription, secondary UIM session, two slots).
- **Mobile data** on both SIM cards (IPA MAPv5 data path, LTE that is already attached, own mux links of SIM 2). The data SIM
  is chosen in the SIM manager or in the Sailfish settings.
- Call audio: earpiece and loudspeaker, volume from the Sailfish steps, ringtone; the fix for the loudspeaker amplifier's I2C bus.
- Screen at the ear: black screen and touch locked, without a flash of the screen.
- SIM manager and reboot menu (TWRP, power off) as Silica apps.

## Wi-Fi, Bluetooth, sleep

- Wi-Fi and Bluetooth with the addresses from EFS; Bluetooth audio.
- **Sleep:** s2idle. Wi-Fi stays connected and wakes the phone. Bluetooth is switched off while the screen is off and nothing
  is connected, and back on with the screen; a connected headset keeps the phone awake, so music and calls do not stop.

## Camera

- libcamera + QtMultimedia in the Jolla camera: main, front, ultra wide and macro camera, with a switch.
- 64 MP (main) and 32 MP (front) photos, autofocus on a tap (circle) with the lens of the main camera, zoom up to 8x, flash
  (off, on, auto), shutter sound, 16:9 viewfinder. The photo appears in the gallery only when the file is complete.
- Screenshot with both volume keys, also in the Camera.
- The exposure control no longer oscillates; no dark frame when the viewfinder starts.

## Other

- Flashlight toggle in the top menu. Light and proximity sensors.

## Not working yet

GPS position (see the README), VoLTE, fingerprint. Not tested: Venus hardware video codecs.
