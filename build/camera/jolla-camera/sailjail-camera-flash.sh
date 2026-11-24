#!/bin/sh
# m52xq: the sandboxed camera may call the flashlight service (camera flash). Usage: sailjail-camera-flash.sh <rootfs>
for f in jolla-camera.profile jolla-camera-lockscreen.profile; do
	F="$1/etc/sailjail/permissions/$f"
	grep -q "org.sailfish.flashlight.provider" "$F" || printf '\n# m52xq: the camera flash is switched through the flashlight service\ndbus-user.talk org.sailfish.flashlight.provider\n' >> "$F"
done
