#!/bin/bash
set -euo pipefail
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
        cp -f /opt/ogw/examples/libtemperature_sim.so /var/lib/ogw/plugins/
        if [ -f /opt/ogw/examples/libcan_motor.so ]; then
            cp -f /opt/ogw/examples/libcan_motor.so /var/lib/ogw/plugins/
        fi
        export OGW_CONFIG=/etc/ogw/gateway.local.json
        export OGW_ADMIN_API=true
        ;;
esac
exec /usr/local/bin/ogw
