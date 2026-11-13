#!/bin/bash
set -euo pipefail
export DISPLAY=:0
export QT_QPA_PLATFORM=xcb
export LIBGL_ALWAYS_SOFTWARE=1
export QT_QUICK_BACKEND=software
Xvfb :0 -screen 0 800x480x24 -nolisten tcp &
for _ in $(seq 1 50); do
    if xdpyinfo -display :0 >/dev/null 2>&1; then
        break
    fi
    sleep 0.1
done
x11vnc -display :0 -forever -shared -rfbport 5900 -nopw -noxdamage >/tmp/x11vnc.log 2>&1 &
websockify --web /usr/share/novnc 6080 localhost:5900 >/tmp/novnc.log 2>&1 &
exec /usr/local/bin/ogw-display
