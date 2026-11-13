import { bitTiming, packTiming, parseHostFrame, type HostFrame } from "./showcase";

interface GsSetup {
  requestType: "vendor";
  recipient: "device";
  request: number;
  value: number;
  index: number;
}

interface GsEndpoint {
  direction: string;
  endpointNumber: number;
  packetSize: number;
}

export interface GsDevice {
  vendorId?: number;
  productId?: number;
  productName?: string;
  configuration: { interfaces: { interfaceNumber: number; alternates: { endpoints: GsEndpoint[] }[] }[] } | null;
  open(): Promise<void>;
  close(): Promise<void>;
  selectConfiguration(configuration: number): Promise<void>;
  claimInterface(interfaceNumber: number): Promise<void>;
  controlTransferOut(setup: GsSetup, data?: Uint8Array): Promise<{ status: string }>;
  controlTransferIn(setup: GsSetup, length: number): Promise<{ status: string; data?: DataView }>;
  transferIn(endpointNumber: number, length: number): Promise<{ data?: DataView }>;
}

interface GsUsb {
  getDevices?: () => Promise<GsDevice[]>;
  requestDevice(options: { filters: { vendorId: number; productId?: number }[] }): Promise<GsDevice>;
}

export function browserUsb(): GsUsb | null {
  const candidate = (navigator as Navigator & { usb?: GsUsb | null }).usb;
  return candidate ?? null;
}

function vendor(request: number): GsSetup {
  return { requestType: "vendor", recipient: "device", request, value: 0, index: 0 };
}

export async function openShowcase(device: GsDevice, bitrate: number, onFrame: (frame: HostFrame) => void): Promise<() => Promise<void>> {
  await device.open();
  if (device.configuration === null) {
    await device.selectConfiguration(1);
  }
  const iface = device.configuration?.interfaces[0];
  const alternate = iface?.alternates[0];
  const incoming = alternate?.endpoints.find((endpoint) => endpoint.direction === "in");
  if (!iface || !incoming) {
    throw new Error("CAN adapter has no bulk input endpoint");
  }
  await device.claimInterface(iface.interfaceNumber);

  const hostFormat = new Uint8Array(4);
  new DataView(hostFormat.buffer).setUint32(0, 0x0000beef, true);
  await device.controlTransferOut(vendor(0), hostFormat);

  const constants = await device.controlTransferIn(vendor(4), 40);
  if (constants.status !== "ok" || !constants.data || constants.data.byteLength < 8) {
    throw new Error("CAN adapter did not report its clock");
  }
  const clockHz = constants.data.getUint32(4, true);
  const timing = bitTiming(clockHz, bitrate);
  if (!timing) {
    throw new Error(`cannot run ${bitrate} bit/s from a ${clockHz} Hz clock`);
  }
  await device.controlTransferOut(vendor(1), packTiming(timing));
  const mode = new Uint8Array(8);
  new DataView(mode.buffer).setUint32(0, 1, true);
  await device.controlTransferOut(vendor(2), mode);

  let stopped = false;
  const pump = async () => {
    while (!stopped) {
      const result = await device.transferIn(incoming.endpointNumber, Math.max(incoming.packetSize, 76));
      if (stopped || !result.data) {
        continue;
      }
      const packet = new Uint8Array(result.data.buffer, result.data.byteOffset, result.data.byteLength);
      const frame = parseHostFrame(packet);
      if (frame) {
        onFrame(frame);
      }
    }
  };
  void pump().catch(() => undefined);

  return async () => {
    stopped = true;
    try {
      await device.controlTransferOut(vendor(2), new Uint8Array(8));
      await device.close();
    } catch {
      /* The adapter may already be closed. */
    }
  };
}
