import { describe, expect, it } from "vitest";
import { devicesToShow, realSensorConnected } from "./selection";
import type { Device, SupportSource } from "./types";

const sim: Device = {
  deviceId: "temperature-sim/room-1",
  pluginId: "temperature-sim",
  displayName: "Room Sensor",
  status: "connected",
  capabilities: [],
};

const showcase: Device = {
  deviceId: "can-motor/showcase",
  pluginId: "can-motor",
  displayName: "Bare-metal showcase",
  status: "connected",
  capabilities: [],
};

const sources: SupportSource[] = [
  { id: "temperature-sim", name: "Temperature", version: "0.1.0", state: "active", error: "", simulated: true, settings: [] },
  { id: "can-motor", name: "CAN", version: "0.1.0", state: "inactive", error: "", simulated: false, settings: [] },
];

describe("sensor selection", () => {
  it("shows the simulator when no real sensor is connected", () => {
    expect(realSensorConnected([sim], sources, false)).toBe(false);
    expect(devicesToShow([sim], sources, false).map((device) => device.deviceId)).toEqual(["temperature-sim/room-1"]);
  });

  it("hides the simulator when a real gateway sensor is connected", () => {
    expect(realSensorConnected([sim, showcase], sources, false)).toBe(true);
    expect(devicesToShow([sim, showcase], sources, false).map((device) => device.deviceId)).toEqual(["can-motor/showcase"]);
  });

  it("hides the simulator when the local adapter is open", () => {
    expect(devicesToShow([sim], sources, true)).toEqual([]);
  });

  it("keeps the simulator when a real source is only disconnected", () => {
    const offline = { ...showcase, status: "disconnected" };
    expect(realSensorConnected([sim, offline], sources, false)).toBe(false);
    expect(devicesToShow([sim, offline], sources, false)).toHaveLength(2);
  });
});
