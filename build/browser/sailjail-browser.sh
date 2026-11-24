#!/bin/sh
# m52xq: settings of the Sailfish Browser sandbox. Usage: sailjail-browser.sh <rootfs>
F="$1/etc/sailjail/permissions/sailfish-browser.profile"
grep -q 'vblank_mode' "$F" && exit 0
cat >> "$F" <<'EOF'

# m52xq: keep the Mesa shader cache on disk (otherwise it lives on the
# sandbox tmpfs and every shader is compiled again at each start)
mkdir     ${HOME}/.cache/mesa_shader_cache_db
whitelist ${HOME}/.cache/mesa_shader_cache_db

# m52xq: the browser calls swapBuffers in its main thread; with Mesa that
# waits for the compositor frame callback and blocks touch and ping
env vblank_mode=0
EOF
