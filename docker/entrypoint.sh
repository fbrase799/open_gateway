#!/bin/bash
set -euo pipefail

# SocketCAN interfaces stay in the host network namespace. When Compose mounts
# that namespace, move can0 here so the plugin can bind it.
adopt_can_interface() {
    local ns="${OGW_HOST_NETNS:-}"
    local iface="${OGW_CAN_IFACE:-can0}"
    local bitrate="${OGW_CAN_BITRATE:-500000}"
    if [ -z "$ns" ] || [ ! -e "$ns" ]; then
        return 0
    fi
    if nsenter --net="$ns" ip link show "$iface" >/dev/null 2>&1; then
        nsenter --net="$ns" ip link set "$iface" netns 1 || true
    fi
    if ip link show "$iface" >/dev/null 2>&1 && ! ip link show "$iface" | grep -q 'state UP'; then
        ip link set "$iface" down >/dev/null 2>&1 || true
        ip link set "$iface" up type can bitrate "$bitrate" || true
    fi
}

mkdir -p /var/lib/ogw/plugins
case "${OGW_PROFILE:-local}" in
    cloud)
        cp -f /opt/ogw/examples/libtemperature_sim.so /opt/ogw/examples/libmotor_sim.so /var/lib/ogw/plugins/
        export OGW_CONFIG=/etc/ogw/gateway.cloud.json
        export OGW_ADMIN_API=false
        ;;
    edge)
        cp -f /opt/ogw/examples/libtemperature_sim.so /opt/ogw/examples/libmotor_sim.so /var/lib/ogw/plugins/
        if [ -f /opt/ogw/examples/libcan_motor.so ]; then
            cp -f /opt/ogw/examples/libcan_motor.so /var/lib/ogw/plugins/
        fi
        export OGW_CONFIG=/etc/ogw/gateway.edge.json
        export OGW_ADMIN_API=false
        ;;
    test)
        cp -f /opt/ogw/examples/libtemperature_sim.so /var/lib/ogw/plugins/
        export OGW_CONFIG=/etc/ogw/gateway.test.json
        export OGW_ADMIN_API=true
        ;;
    *)
        cp -f /opt/ogw/examples/libtemperature_sim.so /opt/ogw/examples/libmotor_sim.so /var/lib/ogw/plugins/
        if [ -f /opt/ogw/examples/libcan_motor.so ]; then
            cp -f /opt/ogw/examples/libcan_motor.so /var/lib/ogw/plugins/
        fi
        export OGW_CONFIG=/etc/ogw/gateway.local.json
        export OGW_ADMIN_API=true
        adopt_can_interface
        ;;
esac
exec /usr/local/bin/ogw
