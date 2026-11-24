#!/bin/sh
# m52xq: messages (SMS, MMS, chats, voicemail) do not switch the display on; calls are not touched. Usage: ... [ROOT]
R=${1:-}
cd "$R/usr/share/lipstick/notificationcategories" || exit 1
for f in x-nemo.messaging.*.conf; do
	[ -e "$f" ] && sed -i 's/^x-nemo-display-on=true$/x-nemo-display-on=false/' "$f"
done
grep -l '^x-nemo-display-on=true' x-nemo.messaging.*.conf 2>/dev/null && exit 1
echo "messages: display-on off in $(grep -l '^x-nemo-display-on=false' x-nemo.messaging.*.conf | wc -l) files"
