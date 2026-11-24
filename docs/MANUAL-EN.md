# Sailfish OS 5.2 on the Samsung Galaxy M52 5G (m52xq): manual (v1)

Unofficial port: native Sailfish OS 5.2.0.22 (no libhybris, no Android) on a mainline Linux 7.1.13 kernel, graphics through
Mesa freedreno. Tested on one SM-M526BR. По-русски: [MANUAL-RU.md](MANUAL-RU.md).

## What works and what does not

Works: display and GPU, browser (YouTube up to 4K), touch, volume keys, auto-rotation and auto-brightness; Wi-Fi; Bluetooth; calls, SMS and mobile data
on two SIM cards (both active); ringtone, earpiece and loudspeaker; screen off at the ear; sound; flashlight; light, proximity
and acceleration sensors; camera (four lenses, 64 MP photos, flash); sleep with Wi-Fi wake-up; battery and charging; SIM
manager; reboot menu; screenshot with both volume keys.

Not yet: GPS position (see README), VoLTE, fingerprint. Not tested: hardware video codecs.

## Requirements

- Unlocked bootloader and TWRP for m52xq.
- A microSD card with `sailfishos-5.2-m52xq-mainline-v1.zip` (620 MB).
- Backups of EFS, persist, modem, boot, vendor_boot and vbmeta. If you are not comfortable restoring a Samsung phone with
  Odin, do not install this.
- A `vendor` partition from a Samsung-based ROM (stock, UN1CA or LineageOS): the installer takes the Bluetooth firmware and
  the sensor configs from it.

## Install (TWRP)

1. Wipe → Format Data → `yes`, then Advanced Wipe → Data → Repair or Change File System → Change File System → **EXT4**.
   Userdata becomes `/home`: it must be ext4.
2. Install → Select Storage → Micro SD card → the zip → Swipe to confirm Flash.
3. Wait for `Updater process ended with RC=0`, then Reboot System.

The installer writes boot, vendor_boot and dtbo, grows `system` (lptools, up to 8 GB if there is room), checks that the image
fits before it writes, and writes the system image. The proprietary Samsung/Qualcomm firmware is not in the zip: the installer
copies it, read-only, from the partitions of the phone being flashed.

If the phone has a file `/data/rootfs.img`, it boots that file instead of the new system (the installer warns about it): rename
or remove it.

## First boot

- The first boot takes longer: the user is created, then the Sailfish start wizard runs (language, licence, Wi-Fi).
- SSH over USB (`172.16.42.1`) or Wi-Fi: user `root`, password `m52xq`. Change the password.

## Tips

- **Two SIM cards:** the "SIM cards manager" app: both SIMs / only SIM 1 / only SIM 2, the SIM for mobile data, SIM cards and
  radio off/on. The SIM for calls: Settings → SIM cards. With "always ask" the SIM selector disappears from the top menu; choose
  a SIM there and it comes back.
- **Camera:** the circles 0.5 / 1.0 / M choose the lens, the two arrows the front camera. Tap the picture to focus (main camera
  only). The flash is in the settings of the Camera; the LED lights up about a second before the photo.
- **Bluetooth:** with a headset connected the phone does not sleep (the link would break). Without a connected device Bluetooth
  is switched off while the screen is off.
- **GPS:** turn on online assistance (Settings → Location).
- **Screenshot:** press Volume Up + Volume Down together.
- **Flashlight:** top menu. If it is missing: Settings → Top menu → Flashlight.
- **Reboot into TWRP:** the "Reboot menu" app.

## Known issues

See the README. The short list: no GPS position at present, magenta tint in blown highlights of photos, soft photos.

Author: Asdlerto Korry
