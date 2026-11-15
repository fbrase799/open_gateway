#!/bin/bash
set -euo pipefail
export LIBGL_ALWAYS_SOFTWARE=1
export QT_QUICK_BACKEND=software
export QT_QPA_PLATFORM=xcb
mode="${OGW_UI:-novnc}"
if [ "$mode" = "desktop" ]; then
    # MIT-SHM does not cross the container boundary into WSLg or XQuartz.
    export QT_X11_NO_MITSHM=1
    host_display=""
    authority="${XAUTHORITY:-}"
    # A regular cookie file is the Mac/XQuartz path. Docker creates a directory
    # at that path when the file is absent, so a directory must not count.
    if [ -f "$authority" ] && [ -s "$authority" ]; then
        host_display="${OGW_X11_TCP_DISPLAY:-${DISPLAY:-host.docker.internal:0}}"
    else
        unset XAUTHORITY || true
        display_num="0"
        case "${DISPLAY:-}" in
            :*)
                display_num="${DISPLAY#:}"
                display_num="${display_num%%.*}"
                ;;
        esac
        if [ -S "/tmp/.X11-unix/X${display_num}" ]; then
            host_display=":${display_num}"
        elif [ -S /tmp/.X11-unix/X0 ]; then
            host_display=":0"
        else
            echo "No host X display. On WSL, WSLg must provide /tmp/.X11-unix/X0. On a Mac, start XQuartz and run docker/xquartz-xauth.sh" >&2
            exit 1
        fi
    fi
    export DISPLAY="$host_display"
    cookie=$(od -An -N16 -tx1 /dev/urandom | tr -d ' \n')
    xauth -f /tmp/xephyr.auth add :1 MIT-MAGIC-COOKIE-1 "$cookie"
    Xephyr :1 -auth /tmp/xephyr.auth -screen 1280x720 -extension MIT-SHM -br -noreset >/tmp/xephyr.log 2>&1 &
    ready=0
    for _ in $(seq 1 50); do
        if DISPLAY=:1 XAUTHORITY=/tmp/xephyr.auth xdpyinfo >/dev/null 2>&1; then
            ready=1
            break
        fi
        sleep 0.1
    done
    if [ "$ready" != 1 ]; then
        echo "desktop display did not start" >&2
        cat /tmp/xephyr.log >&2 || true
        exit 1
    fi
    export DISPLAY=:1
    export XAUTHORITY=/tmp/xephyr.auth
    exec /usr/local/bin/ogw-display
fi
export DISPLAY=:0
Xvfb :0 -screen 0 1280x720x24 -nolisten tcp &
for _ in $(seq 1 50); do
    if xdpyinfo -display :0 >/dev/null 2>&1; then
        break
    fi
    sleep 0.1
done
x11vnc -display :0 -forever -shared -rfbport 5900 -nopw -noxdamage >/tmp/x11vnc.log 2>&1 &
websockify --web /usr/share/novnc 6080 localhost:5900 >/tmp/novnc.log 2>&1 &
exec /usr/local/bin/ogw-display
