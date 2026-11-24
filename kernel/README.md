Kernel of this release: mainline Linux 7.1.13 (SM5714 fuel gauge as sent upstream, fix for the shared I2C bus of the loudspeaker
amplifier, the M52 device drivers).

Source: https://github.com/korryasdlerto-lgtm/sc7280-mainline-linux-m52xq plus
[`patches/`](patches): `0001` is everything this release has on top of that tree (gpi and i2c-geni: the shared bus of the
loudspeaker amplifier, the panel (black screen at the ear), Venus, ath11k (Wi-Fi off), SM5714 charger and fuel gauge, TFA9878,
the q6voice call audio). Apply with `patch -p1`.

Config: `config-7.1.13`. Besides the drivers the sleep of Sailfish needs `CONFIG_PM_AUTOSLEEP` and `CONFIG_PM_WAKELOCKS`
(MCE puts the phone to sleep through `/sys/power/autosleep` and `/sys/power/wake_lock`).

boot.img is in the flashable zip (`firmware/`) without the IPA firmware: the installer appends the phone's own `yupik_ipa_fws` from
`apnhlos` to the ramdisk. The vendor_boot and dtbo images are in the zip too.

The modules in the image (Sailfish layout `/lib/modules/7.1.13`, not usrmerged) are listed in `modules-7.1.13.txt`.
MODVERSIONS is off: use the modules only with this kernel.

The device tree (vendor_boot) is that of the Ubuntu + Lomiri port:
https://github.com/korryasdlerto-lgtm/samsung-m52xq-ubuntu-lomiri (`device-tree/`).
