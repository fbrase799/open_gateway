import type { Device, SupportSource } from "./types";

export function sourceIsSimulated(source: SupportSource): boolean {
  if (typeof source.simulated === "boolean") {
    return source.simulated;
  }
  return source.id.endsWith("-sim");
}

export function simulatedSourceIds(sources: SupportSource[]): Set<string> {
  return new Set(sources.filter(sourceIsSimulated).map((source) => source.id));
}

export function deviceIsSimulated(device: Device, simulated: Set<string>): boolean {
  return simulated.has(device.pluginId) || device.pluginId.endsWith("-sim");
}

export function realSensorConnected(devices: Device[], sources: SupportSource[], localReal: boolean): boolean {
  if (localReal) {
    return true;
  }
  const simulated = simulatedSourceIds(sources);
  return devices.some((device) => device.status === "connected" && !deviceIsSimulated(device, simulated));
}

export function devicesToShow(devices: Device[], sources: SupportSource[], localReal: boolean): Device[] {
  const list = Array.isArray(devices) ? devices : [];
  if (!realSensorConnected(list, sources, localReal)) {
    return list;
  }
  const simulated = simulatedSourceIds(sources);
  return list.filter((device) => !deviceIsSimulated(device, simulated));
}
