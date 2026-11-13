import type { Capability } from "./types";

export type ViewName = "gauge" | "sparkline" | "status" | "text" | "plain" | "command";

export function selectView(capability: Pick<Capability, "kind" | "valueType" | "presentation">): ViewName {
  if (capability.kind === "command") {
    return "command";
  }
  if (capability.valueType === "number" && capability.presentation === "gauge") {
    return "gauge";
  }
  if (capability.valueType === "number") {
    return "sparkline";
  }
  if (capability.valueType === "bool" || capability.valueType === "enum") {
    return "status";
  }
  if (capability.valueType === "string") {
    return "text";
  }
  return "plain";
}

export function formatNumber(value: number): string {
  if (!Number.isFinite(value)) {
    return "—";
  }
  return Math.abs(value) >= 100 ? value.toFixed(0) : value.toFixed(1);
}

export function formatValue(value: number | boolean | string): string {
  if (typeof value === "number") {
    return formatNumber(value);
  }
  if (typeof value === "boolean") {
    return value ? "true" : "false";
  }
  if (typeof value === "string") {
    return value;
  }
  return "—";
}

export function clockLabel(iso: string): string {
  if (typeof iso !== "string") {
    return "";
  }
  const match = iso.match(/T(\d{2}:\d{2}:\d{2})/);
  return match ? match[1] : iso;
}

export function enumLabel(capability: Capability, token: string): { label: string; tone: string } {
  const state = capability.enumStates?.find((item) => item.id === token);
  return { label: state?.label ?? token, tone: state?.tone ?? "neutral" };
}
