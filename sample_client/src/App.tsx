import { useEffect, useRef, useState } from "react";
import { browserUsb, openShowcase, type GsDevice } from "./gsusb";
import { devicesToShow, realSensorConnected, sourceIsSimulated } from "./selection";
import { SHOWCASE_BITRATE, SHOWCASE_FRAME_ID, applyShowcase, decodeShowcase, showcaseDevice } from "./showcase";
import { clockLabel, enumLabel, formatValue, selectView } from "./presentation";
import { reduce } from "./reducer";
import { emptyState, type AppState, type Capability, type Device, type SettingField, type SupportSource } from "./types";

function badgeClass(token: string): string {
  if (token === "good" || token === "connected" || token === "ok" || token === "active" || token === "live" || token === "polling") return "badge ok";
  if (token === "bad" || token === "fault" || token === "faulted" || token === "failed") return "badge fault";
  if (token === "stale" || token === "disconnected" || token === "warn" || token === "uncertain") return "badge warn";
  return "badge";
}

function Sparkline({ capability }: { capability: Capability }) {
  const history = Array.isArray(capability.history) ? capability.history : [];
  const points = history.filter((point) => typeof point.value === "number") as { value: number }[];
  if (points.length < 2) {
    return <div className="spark empty">waiting for samples</div>;
  }
  const values = points.map((point) => point.value);
  const min = Math.min(...values);
  const max = Math.max(...values);
  const span = max - min || 1;
  const path = values
    .map((value, index) => {
      const x = (index / (values.length - 1)) * 100;
      const y = 28 - ((value - min) / span) * 24;
      return `${index === 0 ? "M" : "L"}${x.toFixed(1)} ${y.toFixed(1)}`;
    })
    .join(" ");
  return (
    <svg className="spark" viewBox="0 0 100 32" preserveAspectRatio="none" aria-hidden="true">
      <path d={path} />
    </svg>
  );
}

function Gauge({ capability }: { capability: Capability }) {
  const value = typeof capability.current?.value === "number" ? capability.current.value : 0;
  const min = capability.range?.min ?? 0;
  const max = capability.range?.max ?? 100;
  const fraction = Math.max(0, Math.min(1, (value - min) / (max - min || 1)));
  const length = Math.PI * 54;
  return (
    <svg className="gauge" viewBox="0 0 140 86" aria-hidden="true">
      <path d="M16 74 A54 54 0 0 1 124 74" className="track" />
      <path
        d="M16 74 A54 54 0 0 1 124 74"
        className="fill"
        strokeDasharray={`${length * fraction} ${length}`}
      />
    </svg>
  );
}

function CapabilityCard({ capability }: { capability: Capability }) {
  const view = selectView(capability);
  const current = capability.current;
  const quality = current?.quality ?? "uncertain";
  const valueText = current ? formatValue(current.value) : "—";
  const enumView =
    view === "status" && capability.valueType === "enum" && typeof current?.value === "string"
      ? enumLabel(capability, current.value)
      : null;

  return (
    <article className="card">
      <header>
        <h3>{capability.displayName}</h3>
        <span className={badgeClass(quality)}>{quality}</span>
      </header>
      {view === "command" ? (
        <p className="command">Command channel is not enabled.</p>
      ) : (
        <>
          {view === "gauge" ? <Gauge capability={capability} /> : null}
          <p className="value">
            {enumView ? enumView.label : valueText}
            {capability.unit && view !== "status" ? <small>{capability.unit}</small> : null}
          </p>
          {enumView ? <span className={badgeClass(enumView.tone)}>{enumView.tone}</span> : null}
          {view === "gauge" || view === "sparkline" ? <Sparkline capability={capability} /> : null}
          {typeof current?.detail === "string" && current.detail ? <p className="detail">{current.detail}</p> : null}
          <p className="time">{current ? clockLabel(current.sourceTimestamp) : "no sample"}</p>
        </>
      )}
    </article>
  );
}

function DeviceSection({ device }: { device: Device }) {
  return (
    <section className="device">
      <header>
        <div>
          <h2>{device.displayName}</h2>
          <p>{device.deviceId}</p>
        </div>
        <span className={badgeClass(device.status)}>{device.status}</span>
      </header>
      <div className="cards">
        {(Array.isArray(device.capabilities) ? device.capabilities : [])
          .filter((capability) => capability.kind === "measurement" || capability.kind === "command")
          .map((capability) => (
            <CapabilityCard key={capability.id} capability={capability} />
          ))}
      </div>
    </section>
  );
}

function exportEnabled(state: AppState, id: string, fallback: boolean): boolean {
  const exports = state.support?.exports;
  if (!Array.isArray(exports)) {
    return fallback;
  }
  const entry = exports.find((item) => item.id === id);
  return entry ? entry.enabled : fallback;
}

const showcaseFilter = { vendorId: 0x1d50, productId: 0x606f };

function isShowcaseAdapter(device: GsDevice): boolean {
  return device.vendorId === showcaseFilter.vendorId && device.productId === showcaseFilter.productId;
}

function ShowcaseLink({ onDevice }: { onDevice: (device: Device | null) => void }) {
  const [label, setLabel] = useState("Connect showcase");
  const [note, setNote] = useState("");
  const stop = useRef<(() => Promise<void>) | null>(null);

  const attach = async (device: GsDevice) => {
    let card = showcaseDevice();
    const close = await openShowcase(device, SHOWCASE_BITRATE, (frame) => {
      if (frame.id !== SHOWCASE_FRAME_ID) {
        return;
      }
      const sample = decodeShowcase(frame.data);
      if (!sample) {
        return;
      }
      card = applyShowcase(card, sample, new Date().toISOString());
      onDevice(card);
    });
    stop.current = close;
    onDevice(card);
    setLabel("Disconnect showcase");
    setNote(device.productName || "SH-C31A");
  };

  useEffect(() => {
    const usb = browserUsb();
    if (!usb?.getDevices) {
      return;
    }
    let cancelled = false;
    void usb.getDevices().then((devices) => {
      if (cancelled || stop.current) {
        return;
      }
      const match = devices.find(isShowcaseAdapter);
      if (!match) {
        return;
      }
      return attach(match);
    }).catch((error: unknown) => {
      if (!cancelled) {
        setNote(error instanceof Error ? error.message : "CAN adapter did not open");
      }
    });
    return () => {
      cancelled = true;
    };
  }, []);

  useEffect(() => () => {
    void stop.current?.();
  }, []);

  const connect = async () => {
    if (stop.current) {
      await stop.current();
      stop.current = null;
      onDevice(null);
      setLabel("Connect showcase");
      setNote("");
      return;
    }
    const usb = browserUsb();
    if (!usb) {
      setNote("This browser cannot open USB devices. Open the page in Chrome.");
      return;
    }
    try {
      await attach(await usb.requestDevice({ filters: [showcaseFilter] }));
    } catch (error) {
      setNote(error instanceof Error ? error.message : "CAN adapter did not open");
    }
  };

  return (
    <>
      <button type="button" onClick={() => void connect()}>{label}</button>
      {note ? <em>{note}</em> : null}
    </>
  );
}

export function App() {
  const [state, setState] = useState<AppState>(emptyState);
  const [link, setLink] = useState("connecting");
  const [showcase, setShowcase] = useState<Device | null>(null);
  const suspended = useRef<Set<string>>(new Set());
  const socketOn = exportEnabled(state, "websocket", true);
  const restOn = exportEnabled(state, "rest", true);
  const sources = state.support?.sources ?? [];
  const visible = devicesToShow(state.devices ?? [], sources, showcase !== null);
  const real = realSensorConnected(state.devices ?? [], sources, showcase !== null);

  useEffect(() => {
    if (real) {
      for (const source of sources) {
        if (sourceIsSimulated(source) && source.state === "active") {
          suspended.current.add(source.id);
          putSupport(`/api/v1/support/sources/${source.id}`, { state: "inactive", settings: {} });
        }
      }
      return;
    }
    for (const id of [...suspended.current]) {
      const source = sources.find((item) => item.id === id);
      if (source && source.state !== "active" && source.state !== "failed") {
        putSupport(`/api/v1/support/sources/${id}`, { state: "active", settings: {} });
      }
      if (!source || source.state === "active") {
        suspended.current.delete(id);
      }
    }
  }, [real, sources]);

  useEffect(() => {
    let socket: WebSocket | null = null;
    let stopped = false;
    let attempt = 0;
    let timer = 0;
    let poll = 0;

    const pullSupport = () => {
      void fetch("/api/v1/support")
        .then((response) => response.json())
        .then((body: AppState["support"]) => {
          if (!stopped && Array.isArray(body?.exports) && Array.isArray(body?.sources)) {
            setState((current) => ({ ...current, support: body }));
          }
        })
        .catch(() => undefined);
    };
    const pullDevices = () => {
      void fetch("/api/v1/devices")
        .then((response) => (response.ok ? response.json() : null))
        .then((body: { devices: AppState["devices"] } | null) => {
          if (!stopped && body && Array.isArray(body.devices)) setState((current) => ({ ...current, devices: body.devices }));
        })
        .catch(() => {
          if (!stopped) setLink("offline");
        });
      void fetch("/api/v1/plugins")
        .then((response) => response.json())
        .then((body: { plugins: AppState["plugins"] }) => {
          if (!stopped) setState((current) => ({ ...current, plugins: body.plugins }));
        })
        .catch(() => undefined);
    };

    pullSupport();
    if (restOn) pullDevices();

    if (socketOn) {
      const connect = () => {
        const protocol = window.location.protocol === "https:" ? "wss" : "ws";
        socket = new WebSocket(`${protocol}://${window.location.host}/api/v1/stream`);
        socket.onopen = () => {
          attempt = 0;
          setLink("live");
        };
        socket.onmessage = (message) => {
          const event = JSON.parse(String(message.data)) as { type?: string };
          if (event.type === "snapshot" || stateReady(event)) {
            setState((current) => reduce(current, event));
          }
        };
        socket.onclose = () => {
          if (stopped) return;
          setLink("reconnecting");
          const delay = Math.min(5000, 400 * 2 ** attempt);
          attempt += 1;
          timer = window.setTimeout(connect, delay);
        };
      };
      connect();
    } else {
      setLink(restOn ? "polling" : "paused");
      poll = window.setInterval(() => {
        pullSupport();
        if (restOn) pullDevices();
      }, 1000);
    }

    return () => {
      stopped = true;
      window.clearTimeout(timer);
      window.clearInterval(poll);
      socket?.close();
    };
  }, [socketOn, restOn]);

  return (
    <main>
      <header className="top">
        <div>
          <p className="eyebrow">Open Gateway</p>
          <h1>Live devices</h1>
        </div>
        <span className={link === "live" ? "badge ok" : "badge warn"}>{link}</span>
      </header>
      <div className="layout">
        <div>
          {visible.length === 0 && showcase === null ? <p className="empty-state">Waiting for devices.</p> : null}
          {visible.map((device) => (
            <DeviceSection key={device.deviceId} device={device} />
          ))}
          {showcase ? <DeviceSection device={showcase} /> : null}
        </div>
        <aside>
          <h2>Gateway</h2>
          {(Array.isArray(state.support?.exports) ? state.support.exports : []).map((item) => (
            <label key={item.id} className="switch-row">
              <span>{item.id}</span>
              <input
                type="checkbox"
                checked={item.enabled}
                disabled={!item.mutable}
                onChange={(event) => putSupport(`/api/v1/support/exports/${item.id}`, { enabled: event.target.checked, settings: {} })}
              />
            </label>
          ))}
          <h2>Showcase</h2>
          <ShowcaseLink onDevice={setShowcase} />
          <h2>Sources</h2>
          {(state.support?.sources ?? []).length === 0 ? <p>No sources advertised.</p> : null}
          <ul>
            {(state.support?.sources ?? []).map((source) => (
              <SourceRow key={source.id} source={source} />
            ))}
          </ul>
        </aside>
      </div>
    </main>
  );
}

function stateReady(event: { type?: string }): boolean {
  return event.type === "reading" || event.type === "device_upsert" || event.type === "device_removed" || event.type === "plugin_state" || event.type === "support";
}

function putSupport(path: string, body: unknown) {
  void fetch(path, {
    method: "PUT",
    headers: { "content-type": "application/json" },
    body: JSON.stringify(body),
  });
}

function settingValue(field: SettingField, draft: string | undefined): string | number | boolean {
  const raw = draft ?? (field.value === undefined ? "" : String(field.value));
  if (field.valueType === "number") return raw === "" ? 0 : Number(raw);
  if (field.valueType === "bool") return raw === "true";
  return raw;
}

function SourceRow({ source }: { source: SupportSource }) {
  const [draft, setDraft] = useState<Record<string, string>>({});
  const nextState = source.state === "active" ? "inactive" : "active";
  const settings = Array.isArray(source.settings) ? source.settings : [];
  return (
    <li>
      <strong>{source.name || source.id}</strong>
      <span className={badgeClass(source.state)}>{source.state}</span>
      <small>{source.version}</small>
      {source.error ? <em>{source.error}</em> : null}
      <button
        type="button"
        onClick={() => putSupport(`/api/v1/support/sources/${source.id}`, { state: nextState, settings: {} })}
      >
        {source.state === "active" ? "Disable" : "Enable"}
      </button>
      {settings.map((field) => (
        <label key={field.name} className="setting">
          <span>{field.name}</span>
          <input
            type={field.valueType === "number" ? "number" : "text"}
            min={field.min}
            max={field.max}
            value={draft[field.name] ?? (field.value === undefined ? "" : String(field.value))}
            onChange={(event) => setDraft((current) => ({ ...current, [field.name]: event.target.value }))}
          />
        </label>
      ))}
      {settings.length > 0 ? (
        <button
          type="button"
          onClick={() =>
            putSupport(`/api/v1/support/sources/${source.id}`, {
              state: source.state === "active" ? "active" : "inactive",
              settings: Object.fromEntries(settings.map((field) => [field.name, settingValue(field, draft[field.name])])),
            })
          }
        >
          Apply
        </button>
      ) : null}
    </li>
  );
}
