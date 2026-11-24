#!/bin/sh
# Runs INSIDE sdk/target; target rootfs at /sfos. Based on moWerk/sailfishos-pro1x-mainline scripts/stage2-device.sh (1, 1b).
set -e
R=/sfos
# 1. stage1 installed busybox-symlinks-bash 3rd but busybox itself 114th: the scriptlets of the first 113 packages
#    ran without /bin/sh -> reinstall them (list from the install order, scripts/stage2-reinstall.list)
names=$(cat /root/stage2-reinstall.list)
zypper --non-interactive --root $R in -f --no-recommends $names
chroot $R /sbin/ldconfig
# 1b. what the kickstart %post does (as in pro1x, from the nabu/pinephone kickstarts)
echo -n "aarch64-meego-linux" > $R/etc/rpm/platform
grep -q '^arch = aarch64' $R/etc/zypp/zypp.conf || echo "arch = aarch64" >> $R/etc/zypp/zypp.conf
rm -f $R/var/lib/rpm/__db*
chroot $R rpm --rebuilddb
rm -f $R/.bootstrap
chroot $R /bin/sh -c '
  export LANG=en_US.UTF-8 LC_COLLATE=en_US.UTF-8
  UID_MIN=$(grep "^UID_MIN" /etc/login.defs | tr -s " " | cut -d " " -f2)
  DEVICEUSER=$(getent passwd $UID_MIN | sed "s/:.*//")
  echo "stage2: device user: $DEVICEUSER (UID_MIN $UID_MIN)"
  if [ -x /usr/bin/oneshot ]; then
    /usr/bin/oneshot --mic
    [ -n "$DEVICEUSER" ] && su -c "/usr/bin/oneshot --mic" $DEVICEUSER
  fi
  ssu release 5.2.0.22
  ssu mode 4
'
echo STAGE2A-DONE
