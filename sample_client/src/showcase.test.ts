import { describe, expect, it } from "vitest";
import { applyShowcase, bitTiming, decodeShowcase, parseHostFrame, showcaseDevice } from "./showcase";

describe("showcase frame", () => {
  it("decodes the STM32 status payload", () => {
    const sample = decodeShowcase(new Uint8Array([0xfe, 0xff, 0xff, 0xff, 40, 1, 0, 0]));
    expect(sample).toEqual({ counts: -2, duty: 40, direction: "CCW" });
  });

  it("reads that payload out of a gs_usb host frame", () => {
    const packet = new Uint8Array(20);
    const view = new DataView(packet.buffer);
    view.setUint32(4, 0x101, true);
    packet[8] = 8;
    packet.set([0xfe, 0xff, 0xff, 0xff, 40, 0], 12);
    const frame = parseHostFrame(packet);
    expect(frame?.id).toBe(0x101);
    expect(decodeShowcase(frame!.data)).toEqual({ counts: -2, duty: 40, direction: "CW" });
  });

  it("times 500 kbit/s for the candleLight and SH-C31A clocks", () => {
    expect(bitTiming(48_000_000, 500_000)).toMatchObject({ brp: 6, phaseSeg1: 12, phaseSeg2: 2 });
    expect(bitTiming(170_000_000, 500_000)?.brp).toBe(20);
  });

  it("keeps a sample on the showcase card", () => {
    const next = applyShowcase(showcaseDevice(), { counts: 12, duty: 30, direction: "CW" }, "2026-11-13T10:12:00.000Z");
    expect(next.capabilities[0].current?.value).toBe(12);
    expect(next.capabilities[1].current?.value).toBe(30);
    expect(next.capabilities[2].current?.value).toBe("CW");
  });
});