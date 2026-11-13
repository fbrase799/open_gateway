import { describe, expect, it } from "vitest";
import { reduce } from "./reducer";
import { emptyState, type Device } from "./types";

const room: Device = {
  deviceId: "temperature-sim/room-1",
  pluginId: "temperature-sim",
  displayName: "Room Sensor",
  status: "connected",
  capabilities: [
    {
      id: "temperature",
      kind: "measurement",
      valueType: "number",
      displayName: "Temperature",
      unit: "°C",
      presentation: "gauge",
      staleAfterMs: 5000,
      current: null,
      history: [],
    },
  ],
};

describe("reduce", () => {
  it("replaces state from a snapshot and patches a later reading", () => {
    const snapped = reduce(emptyState, {
      type: "snapshot",
      devices: [room],
      plugins: [{ id: "temperature-sim", state: "active" }],
      support: { exports: [{ id: "rest", enabled: true, mutable: true, settings: [] }], sources: [] },
    });
    expect(snapped.support.exports).toHaveLength(1);
    const next = reduce(snapped, {
      type: "reading",
      deviceId: "temperature-sim/room-1",
      capabilityId: "temperature",
      value: 22.5,
      quality: "good",
      sourceTimestamp: "2026-11-07T12:00:00.000Z",
      receivedAt: "2026-11-07T12:00:00.100Z",
      detail: null,
    });
    expect(next.devices[0].capabilities[0].current?.value).toBe(22.5);
    expect(next.devices[0].capabilities[0].history).toHaveLength(1);
  });

  it("adds a device from an upsert and removes it again", () => {
    const added = reduce(emptyState, { type: "support", support: { exports: [], sources: [{ id: "motor-sim", name: "Motor", version: "0.1.0", state: "inactive", error: "", settings: [] }] } });
    expect(added.support.sources[0].id).toBe("motor-sim");
    const fromDevice = reduce(emptyState, { type: "device_upsert", device: room });
    expect(fromDevice.devices).toHaveLength(1);
    const removed = reduce(fromDevice, { type: "device_removed", deviceId: room.deviceId });
    expect(removed.devices).toHaveLength(0);
  });

  it("keeps the previous catalog when a snapshot omits support", () => {
    const start = reduce(emptyState, {
      type: "snapshot",
      devices: [room],
      plugins: [],
      support: { exports: [{ id: "rest", enabled: true, mutable: true, settings: [] }], sources: [] },
    });
    const next = reduce(start, { type: "snapshot", devices: [{ deviceId: "temperature-sim/room-1", displayName: "Room Sensor", status: "disconnected" }], plugins: [], support: {} });
    expect(next.support.exports).toHaveLength(1);
    expect(next.devices[0].capabilities).toEqual([]);
  });

  it("records plugin failure without dropping devices", () => {
    const start = reduce(emptyState, { type: "snapshot", devices: [room], plugins: [] });
    const next = reduce(start, {
      type: "plugin_state",
      plugin: { id: "bad-plugin", displayName: "Bad", version: "0", file: "libbad_plugin.so", state: "failed", error: "init failed" },
    });
    expect(next.plugins[0].state).toBe("failed");
    expect(next.devices).toHaveLength(1);
  });
});
