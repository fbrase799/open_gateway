#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
compose=(docker compose -f docker/compose.yaml)
cid="$("${compose[@]}" ps -q gateway)"
if [[ -z "${cid}" ]]; then
    echo "gateway is not running. Start it with: docker compose -f docker/compose.yaml up --build" >&2
    exit 1
fi
docker compose -f docker/compose.yaml exec -T gateway \
    cp /opt/ogw/examples/libmotor_sim.so /var/lib/ogw/plugins/libmotor_sim.so
curl -fsS -X POST "http://127.0.0.1:8081/api/v1/admin/plugins/scan"
echo
echo "motor-sim is installed and inactive. Enable it from the sample client support panel."
