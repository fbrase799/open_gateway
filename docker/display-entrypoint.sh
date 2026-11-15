#!/bin/bash
set -euo pipefail

config_path="${OGW_DISPLAY_CONFIG:-/etc/ogw/display.toml}"
export OGW_DISPLAY_CONFIG="$config_path"
if [ ! -f "$config_path" ]; then
    mkdir -p "$(dirname "$config_path")"
    cat > "$config_path" <<'EOF'
# Title shown in the display header and the window title.
title = "Open Gateway"

# Simulated devices in the sensor list. enabled = false hides one.
# name is the label. The list adds "Simulator" on a second row.
[simulators.temperature-sim]
enabled = true
name = "Temperature"

[simulators.motor-sim]
enabled = true
name = "Motor Control"

# Real devices in the sensor list. enabled = false hides one.
[devices.can-motor]
enabled = true
name = "Motor Control"
EOF
fi

export LIBGL_ALWAYS_SOFTWARE=1
export QT_QUICK_BACKEND=software
export QT_QPA_PLATFORM=xcb
mode="${OGW_UI:-novnc}"
if [ "$mode" = "desktop" ]; then
    # MIT-SHM does not cross the container boundary into WSLg or XQuartz.
    export QT_X11_NO_MITSHM=1
    host_display=""
    authority="${XAUTHORITY:-}"
    # Docker creates a directory at the cookie path when the file is absent.
    cookie_ready=0
    if [ -f "$authority" ] && [ -s "$authority" ]; then
        cookie_ready=1
    fi
    display_num="0"
    case "${DISPLAY:-}" in
        :*)
            display_num="${DISPLAY#:}"
            display_num="${display_num%%.*}"
            ;;
    esac

    # A mounted socket node is not proof of a display. Docker Desktop on a Mac
    # exposes /tmp/.X11-unix/X0, and that socket does not connect across the VM.
    # WSLg's socket answers, so use it only after xdpyinfo succeeds.
    unix_display=""
    for candidate in "$display_num" 0; do
        if [ -n "$unix_display" ]; then
            break
        fi
        if [ ! -S "/tmp/.X11-unix/X${candidate}" ]; then
            continue
        fi
        if env -u XAUTHORITY DISPLAY=":${candidate}" timeout 2 xdpyinfo >/dev/null 2>&1; then
            unix_display=":${candidate}"
        fi
    done

    if [ -n "$unix_display" ]; then
        host_display="$unix_display"
        unset XAUTHORITY || true
    elif [ "$cookie_ready" = 1 ]; then
        host_display="${OGW_X11_TCP_DISPLAY:-host.docker.internal:0}"
        if ! DISPLAY="$host_display" XAUTHORITY="$authority" timeout 3 xdpyinfo >/dev/null 2>&1; then
            echo "XQuartz did not accept the display cookie. Start XQuartz, run docker/xquartz-xauth.sh, then start Compose again." >&2
            exit 1
        fi
    else
        echo "No host X display. On WSL, WSLg must provide /tmp/.X11-unix/X0. On a Mac, start XQuartz and run docker/xquartz-xauth.sh" >&2
        exit 1
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
