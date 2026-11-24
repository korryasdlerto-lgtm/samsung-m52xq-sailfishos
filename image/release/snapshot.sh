#!/bin/sh
# Snapshot of the running phone (busybox tar on the phone, GNU tar --numeric-owner here). The address: PHONE=172.16.42.1 (USB) by default.
# Then run cleanup.sh: the snapshot contains personal data.
set -e
PHONE=${PHONE:-172.16.42.1}
SSH="ssh -o LogLevel=ERROR root@$PHONE"
$SSH 'tar -C /boot -cf - .' | sudo tar --numeric-owner -xpf - -C mnt-boot
echo "boot done: $(sudo du -sh mnt-boot | cut -f1)"
$SSH 'sync; cd / && tar -cf - --exclude=./proc --exclude=./sys --exclude=./dev --exclude=./run --exclude=./tmp --exclude=./boot \
  --exclude=./var/log/journal --exclude=./home/defaultuser --exclude=./home/.zypp-cache --exclude=./home/.zypp-tmp --exclude=./var/tmp \
  --exclude=./var/cache/zypp --exclude=./lost+found .' | sudo tar --numeric-owner -xpf - -C mnt-root
for d in proc sys dev run tmp boot var/tmp var/log/journal; do sudo mkdir -p mnt-root/$d; done
sudo chmod 1777 mnt-root/tmp mnt-root/var/tmp
echo "root done: $(sudo du -sh mnt-root | cut -f1)"
