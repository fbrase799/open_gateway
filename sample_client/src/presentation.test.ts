import { describe, expect, it } from "vitest";
import { formatNumber, selectView } from "./presentation";

describe("selectView", () => {
  it("maps known measurement hints and falls back for unknown ones", () => {
    expect(selectView({ kind: "measurement", valueType: "number", presentation: "gauge" })).toBe("gauge");
    expect(selectView({ kind: "measurement", valueType: "number", presentation: "sparkline" })).toBe("sparkline");
    expect(selectView({ kind: "measurement", valueType: "number" })).toBe("sparkline");
    expect(selectView({ kind: "measurement", valueType: "number", presentation: "mystery" })).toBe("sparkline");
    expect(selectView({ kind: "measurement", valueType: "enum", presentation: "status" })).toBe("status");
    expect(selectView({ kind: "measurement", valueType: "bool" })).toBe("status");
    expect(selectView({ kind: "measurement", valueType: "string" })).toBe("text");
    expect(selectView({ kind: "measurement", valueType: "bytes" })).toBe("plain");
  });

  it("does not turn a numeric measurement into a command", () => {
    expect(selectView({ kind: "measurement", valueType: "number", presentation: "gauge" })).not.toBe("command");
    expect(selectView({ kind: "command", valueType: "number" })).toBe("command");
  });
});

describe("formatNumber", () => {
  it("keeps one decimal for small values", () => {
    expect(formatNumber(21.46)).toBe("21.5");
    expect(formatNumber(1500)).toBe("1500");
  });
});
