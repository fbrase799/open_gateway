#!/bin/bash
set -euo pipefail
cmake -S /src -B /tmp/build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DOGW_SANITIZERS=ON
cmake --build /tmp/build
# Loaded plugins stay mapped until process exit, so leak detection is not a useful signal here.
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
    ctest --test-dir /tmp/build --output-on-failure
cmake -S /src -B /tmp/build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DOGW_SANITIZERS=ON
cmake --install /tmp/build --prefix /tmp/sdk
cmake -S /src/plugins/motor_sim -B /tmp/motor -G Ninja -DOGW_SDK_DIR=/tmp/sdk
cmake --build /tmp/motor
test -f /tmp/motor/libmotor_sim.so
cmake -S /src/plugins/can_motor -B /tmp/can -G Ninja -DOGW_SDK_DIR=/tmp/sdk
cmake --build /tmp/can
test -f /tmp/can/libcan_motor.so
echo "unit tests, motor plugin, and CAN plugin build succeeded"
