import type { AppState, Capability, Device, HistoryPoint, PluginInfo, Reading, SupportDescription } from "./types";
import { emptySupport } from "./types";

const HISTORY_LIMIT = 60;

function appendHistory(capability: Capability, reading: Reading): HistoryPoint[] {
  const next = [
    ...(Array.isArray(capability.history) ? capability.history : []),
    { value: reading.value, quality: reading.quality, sourceTimestamp: reading.sourceTimestamp },
  ];
  return next.slice(-HISTORY_LIMIT);
}

function asDevice(value: unknown): Device | null {
  if (!value || typeof value !== "object") {
    return null;
  }
  const device = value as Device;
  if (!device.deviceId) {
    return null;
  }
    const capabilities = Array.isArray(device.capabilities) ? device.capabilities : [];
  return {
    ...device,
    capabilities: capabilities.flatMap((capability) =>
      capability && typeof capability === "object"
        ? [{ ...capability, history: Array.isArray(capability.history) ? capability.history : [] }]
        : [],
    ),
  };
}

function asSupport(value: unknown, fallback: SupportDescription): SupportDescription {
  if (!value || typeof value !== "object") {
    return fallback;
  }
  const support = value as Partial<SupportDescription>;
  if (!Array.isArray(support.exports) || !Array.isArray(support.sources)) {
    return fallback;
  }
  return {
    exports: support.exports,
    sources: support.sources.map((source) => ({ ...source, settings: Array.isArray(source.settings) ? source.settings : [] })),
  };
}

function patchDevice(device: Device, capabilityId: string, reading: Reading): Device {
  return {
    ...device,
    capabilities: device.capabilities.map((capability) =>
      capability.id === capabilityId
        ? { ...capability, current: reading, history: appendHistory(capability, reading) }
        : capability,
    ),
  };
}

export function reduce(state: AppState, event: { type?: string; [key: string]: unknown }): AppState {
  switch (event.type) {
    case "snapshot":
      return {
        devices: Array.isArray(event.devices) ? event.devices.map(asDevice).filter((device): device is Device => device !== null) : [],
        plugins: Array.isArray(event.plugins) ? (event.plugins as PluginInfo[]) : [],
        support: asSupport(event.support, Array.isArray(state.support?.exports) ? state.support : emptySupport),
      };
    case "reading": {
      const deviceId = String(event.deviceId ?? "");
      const capabilityId = String(event.capabilityId ?? "");
      const reading: Reading = {
        value: event.value as Reading["value"],
        quality: String(event.quality ?? "uncertain"),
        sourceTimestamp: String(event.sourceTimestamp ?? ""),
        receivedAt: String(event.receivedAt ?? ""),
        detail: (event.detail as string | null) ?? null,
      };
      return {
        ...state,
        devices: state.devices.map((device) => (device.deviceId === deviceId ? patchDevice(device, capabilityId, reading) : device)),
      };
    }
    case "device_upsert": {
      const device = asDevice(event.device);
      if (!device) {
        return state;
      }
      const exists = state.devices.some((item) => item.deviceId === device.deviceId);
      return {
        ...state,
        devices: exists
          ? state.devices.map((item) => (item.deviceId === device.deviceId ? device : item))
          : [...state.devices, device],
      };
    }
    case "device_removed":
      return {
        ...state,
        devices: state.devices.filter((device) => device.deviceId !== event.deviceId),
      };
    case "support":
      return {
        ...state,
        support: asSupport(event.support, state.support),
      };
    case "plugin_state": {
      const plugin = event.plugin as PluginInfo | undefined;
      if (!plugin) {
        return state;
      }
      const exists = state.plugins.some((item) => item.id === plugin.id);
      return {
        ...state,
        plugins: exists ? state.plugins.map((item) => (item.id === plugin.id ? { ...item, ...plugin } : item)) : [...state.plugins, plugin],
      };
    }
    default:
      return state;
  }
}
