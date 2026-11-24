#!/bin/sh
# Public release: remove the data of the phone from the snapshot. Run as root: R = image root, BASE = clean base rootfs (see stage1-base.sh)
set -ex
R=${R:-mnt-root}
BASE=${BASE:-rootfs}
# 1. the user: defaultuser is created again at the first start (start-autologin + user-managerd create.d)
awk -F: '$1 != "defaultuser"' $R/etc/passwd > /tmp/m52xq-passwd && cat /tmp/m52xq-passwd > $R/etc/passwd
awk -F: '$1 != "defaultuser"' $R/etc/shadow > /tmp/m52xq-shadow && cat /tmp/m52xq-shadow > $R/etc/shadow
for f in group gshadow; do
	awk -F: 'BEGIN { OFS = ":" } $1 == "defaultuser" { next }
		{ n = split($NF, m, ","); s = ""; for (i = 1; i <= n; i++) if (m[i] != "defaultuser") s = s (s == "" ? "" : ",") m[i]; $NF = s; print }' \
		$R/etc/$f > /tmp/m52xq-$f && cat /tmp/m52xq-$f > $R/etc/$f
done
rm -f /tmp/m52xq-passwd /tmp/m52xq-shadow /tmp/m52xq-group /tmp/m52xq-gshadow
rm -rf $R/etc/oneshot.d/100000 $R/home/.system/var/lib/environment/100000
rm -f $R/home/.system/var/lib/sailjail/settings/user-100000.settings $R/home/.system/var/lib/connman/global_proxy/settings
# 2. state directories: as in the clean base rootfs
for d in var/lib/timed var/lib/dsme var/lib/location etc/location var/lib/mce var/lib/ofono var/lib/bluetooth var/lib/PackageKit \
	var/lib/jolla-startupwizard var/lib/sailfish-share root home/appsupport-root var/cache; do
	rm -rf $R/$d
	cp -a $BASE/$d $R/$d
done
rm -rf $R/var/lib/tqftpserv $R/var/lib/m52xq
rm -f $R/var/lib/systemd/random-seed; rm -rf $R/var/lib/systemd/coredump/*
# mce: only "proximity off" (m52xq-incall-proximity blanks at the ear), no settings of this phone
printf '/system/osso/dsm/proximity/ps_enabled=false\n' > $R/var/lib/mce/builtin-gconf.values
# 3. logs (keep the files that wtmp/btmp/lastlog need, empty)
rm -f $R/var/log/systemboot.log $R/var/log/YaST2/config_diff_*.log $R/var/log/zypp/history
for f in btmp wtmp lastlog tallylog; do [ -f $R/var/log/$f ] && : > $R/var/log/$f; done
# 4. identity of this phone
rm -f $R/etc/m52xq/bt_addr $R/etc/m52xq/wifi_mac $R/etc/ssh/ssh_host_*
: > $R/etc/machine-id
rm -f $R/var/lib/dbus/machine-id
# 5. proprietary firmware (the installer takes it from the phone being flashed)
F=$R/lib/firmware
rm -f $F/adsp.* $F/adspr.jsn $F/adsps.jsn $F/adspua.jsn $F/modem.* $F/modemr.jsn $F/a660_zap.* $F/yupik_ipa_fws.*
rm -rf $F/modem_pr $F/qcom/vpu-2.0/* $F/qca/*
SN=$R/usr/share/qcom/sm7325/Samsung/m52xq/sensors
rm -rf $SN/config/* $SN/registry/*; rm -f $SN/sns_reg.conf
# 6. screen lock data of the phone (encrypted lock code and its state): as in the clean base rootfs
rm -rf $R/usr/share/lipstick/devicelock
cp -a $BASE/usr/share/lipstick/devicelock $R/usr/share/lipstick/devicelock
