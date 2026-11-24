#!/bin/sh
# Runs INSIDE sdk/target; target rootfs at /sfos; sparse tree at /root/sparse. Bring-up access (as pro1x step 4).
set -e
R=/sfos
cp -r --no-preserve=ownership /root/sparse/. $R/
grep -q '^Include /etc/ssh/sshd_config.d' $R/etc/ssh/sshd_config || sed -i '1i Include /etc/ssh/sshd_config.d/*.conf' $R/etc/ssh/sshd_config
echo 'root:1111' | chroot $R chpasswd
chroot $R systemctl enable sshd.service m52xq-usbnet.service
# usb-moded would reconfigure the gadget made by the initramfs; off until it has an m52xq config
chroot $R systemctl mask usb-moded.service
echo STAGE2C-DONE
