import type { Capability, Device, Reading } from "./types";

export const SHOWCASE_FRAME_ID = 0x101;
export const SHOWCASE_BITRATE = 500_000;

export interface ShowcaseSample {
  counts: number;
  duty: number;
  direction: "CW" | "CCW";
}

export interface HostFrame {
  id: number;
  data: Uint8Array;
}

export interface BitTiming {
  propSeg: number;
  phaseSeg1: number;
  phaseSeg2: number;
  sjw: number;
  brp: number;
}

function measurement(id: string, displayName: string, valueType: string, unit: string, presentation: string, range?: { min: number; max: number }): Capability {
  return {
    id,
    kind: "measurement",
    valueType,
    displayName,
    unit,
    presentation,
    range,
    staleAfterMs: 1000,
    current: null,
    history: [],
  };
}

export function showcaseDevice(): Device {
  return {
    deviceId: "sh-c31a/showcase",
    pluginId: "sh-c31a",
    displayName: "Bare-metal showcase",
    status: "connected",
    capabilities: [
      measurement("encoder_counts", "Encoder", "number", "counts", "sparkline"),
      measurement("duty", "Duty", "number", "%", "gauge", { min: 0, max: 100 }),
      measurement("direction", "Direction", "string", "", "text"),
    ],
  };
}

export function decodeShowcase(data: Uint8Array): ShowcaseSample | null {
  if (data.length < 6) {
    return null;
  }
  const view = new DataView(data.buffer, data.byteOffset, data.byteLength);
  return {
    counts: view.getInt32(0, true),
    duty: data[4],
    direction: data[5] === 0 ? "CW" : "CCW",
  };
}

export function parseHostFrame(packet: Uint8Array): HostFrame | null {
  if (packet.length < 12) {
    return null;
  }
  const view = new DataView(packet.buffer, packet.byteOffset, packet.byteLength);
  const rawId = view.getUint32(4, true);
  if ((rawId & 0x2000_0000) !== 0) {
    return null;
  }
  const extended = (rawId & 0x8000_0000) !== 0;
  const id = extended ? rawId & 0x1fff_ffff : rawId & 0x7ff;
  const length = Math.min(packet[8], packet.length - 12);
  return { id, data: packet.slice(12, 12 + length) };
}

export function bitTiming(clockHz: number, bitrate: number): BitTiming | null {
  if (clockHz <= 0 || bitrate <= 0 || clockHz % bitrate !== 0) {
    return null;
  }
  const quanta = clockHz / bitrate;
  let best: (BitTiming & { error: number }) | null = null;
  for (let brp = 1; brp <= 1024; brp += 1) {
    if (quanta % brp !== 0) {
      continue;
    }
    const ntq = quanta / brp;
    if (ntq < 8 || ntq > 25) {
      continue;
    }
    let phaseSeg2 = Math.round(ntq * 0.125);
    if (phaseSeg2 < 1) phaseSeg2 = 1;
    if (phaseSeg2 > 8) phaseSeg2 = 8;
    const tseg1 = ntq - 1 - phaseSeg2;
    if (tseg1 < 2 || tseg1 > 16) {
      continue;
    }
    const sample = (1 + tseg1) / ntq;
    const error = Math.abs(sample - 0.875);
    if (best === null || error < best.error) {
      best = { propSeg: 1, phaseSeg1: tseg1 - 1, phaseSeg2, sjw: 1, brp, error };
    }
  }
  if (best === null) {
    return null;
  }
  return { propSeg: best.propSeg, phaseSeg1: best.phaseSeg1, phaseSeg2: best.phaseSeg2, sjw: best.sjw, brp: best.brp };
}

export function packTiming(timing: BitTiming): Uint8Array {
  const bytes = new Uint8Array(20);
  const view = new DataView(bytes.buffer);
  view.setUint32(0, timing.propSeg, true);
  view.setUint32(4, timing.phaseSeg1, true);
  view.setUint32(8, timing.phaseSeg2, true);
  view.setUint32(12, timing.sjw, true);
  view.setUint32(16, timing.brp, true);
  return bytes;
}

export function applyShowcase(device: Device, sample: ShowcaseSample, iso: string): Device {
  const values: Record<string, number | string> = {
    encoder_counts: sample.counts,
    duty: sample.duty,
    direction: sample.direction,
  };
  return {
    ...device,
    status: "connected",
    capabilities: device.capabilities.map((capability) => {
      const value = values[capability.id];
      if (value === undefined) {
        return capability;
      }
      const reading: Reading = { value, quality: "good", sourceTimestamp: iso, receivedAt: iso, detail: null };
      const history =
        typeof value === "number"
          ? [...capability.history, { value, quality: "good", sourceTimestamp: iso }].slice(-60)
          : capability.history;
      return { ...capability, current: reading, history };
    }),
  };
}
