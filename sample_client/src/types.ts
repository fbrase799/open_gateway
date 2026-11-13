export interface HistoryPoint {
  value: number | boolean | string;
  quality: string;
  sourceTimestamp: string;
}

export interface Reading {
  value: number | boolean | string;
  quality: string;
  sourceTimestamp: string;
  receivedAt: string;
  detail: string | null;
}

export interface EnumState {
  id: string;
  label: string;
  tone: string;
}

export interface Capability {
  id: string;
  kind: string;
  valueType: string;
  displayName: string;
  unit?: string;
  presentation?: string;
  range?: { min: number; max: number };
  enumStates?: EnumState[];
  staleAfterMs: number;
  current: Reading | null;
  history: HistoryPoint[];
}

export interface Device {
  deviceId: string;
  pluginId: string;
  displayName: string;
  status: string;
  capabilities: Capability[];
}

export interface PluginInfo {
  id: string;
  displayName: string;
  version: string;
  file: string;
  state: string;
  error: string;
  deviceCount?: number;
}

export interface SettingField {
  name: string;
  valueType: string;
  value?: string | number | boolean;
  min?: number;
  max?: number;
}

export interface SupportExport {
  id: string;
  enabled: boolean;
  mutable: boolean;
  settings: SettingField[];
}

export interface SupportSource {
  id: string;
  name: string;
  version: string;
  state: string;
  error: string;
  simulated?: boolean;
  settings: SettingField[];
}

export interface SupportDescription {
  exports: SupportExport[];
  sources: SupportSource[];
}

export interface AppState {
  devices: Device[];
  plugins: PluginInfo[];
  support: SupportDescription;
}

export const emptySupport: SupportDescription = { exports: [], sources: [] };

export const emptyState: AppState = { devices: [], plugins: [], support: emptySupport };
