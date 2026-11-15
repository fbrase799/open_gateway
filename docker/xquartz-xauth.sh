#!/bin/bash
# Prepare the MIT-MAGIC-COOKIE that the display container presents to XQuartz.
# Run this on the Mac after XQuartz is running, then start Compose.
set -euo pipefail

XAUTH_BIN="${XAUTH_BIN:-/opt/X11/bin/xauth}"
if [[ ! -x "$XAUTH_BIN" ]]; then
    echo "XQuartz is not installed (missing $XAUTH_BIN)." >&2
    exit 1
fi

# macOS sets DISPLAY to a launchd socket path. XQuartz stores the cookie for :0.
cookie_display=""
for candidate in "${DISPLAY:-}" :0; do
    if [[ -n "$candidate" ]] && "$XAUTH_BIN" nlist "$candidate" 2>/dev/null | grep -q .; then
        cookie_display="$candidate"
        break
    fi
done
if [[ -z "$cookie_display" ]]; then
    echo "XQuartz has no cookie in ~/.Xauthority. Start XQuartz, then run this again." >&2
    exit 1
fi

# Keep authentication on, and accept TCP so the container can present the cookie.
# no_auth stays false: a client without this cookie is rejected.
defaults write org.xquartz.X11 nolisten_tcp -bool false
defaults write org.xquartz.X11 no_auth -bool false

target="/tmp/.docker.xauth"
if [[ -d "$target" ]]; then
    rm -rf "$target"
fi
tmp=$(mktemp)
"$XAUTH_BIN" nlist "$cookie_display" | sed -e 's/^..../ffff/' | "$XAUTH_BIN" -f "$tmp" nmerge -
install -m 600 "$tmp" "$target"
rm -f "$tmp"
echo "Wrote $target from $cookie_display"
echo "Restart XQuartz if you have not restarted it since enabling TCP, then run Compose."
