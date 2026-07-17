"use client";

import mqtt, { MqttClient } from "mqtt";
import { FormEvent, useCallback, useEffect, useMemo, useRef, useState } from "react";

type Source = "stm32" | "phone";
type Telemetry = Record<string, unknown>;
type Sample = { time: number; value: number };
type Point = { lat: number; lon: number };
type HistoryRecord = {
  receivedAt: number;
  latitude: number | null;
  longitude: number | null;
  altitude: number | null;
  speed: number | null;
  battery: number | null;
  temperature: number | null;
  humidity: number | null;
};

const MAX_SAMPLES = 120;
const DEFAULT_HOST = "h00fc100.ala.cn-shenzhen.emqxsl.cn";

function numberOf(data: Telemetry | null, ...keys: string[]) {
  for (const key of keys) {
    const value = Number(data?.[key]);
    if (Number.isFinite(value)) return value;
  }
  return 0;
}

function textOf(data: Telemetry | null, key: string, fallback = "--") {
  const value = data?.[key];
  return value === undefined || value === null || value === "" ? fallback : String(value);
}

function displayTime(data: Telemetry | null) {
  const raw = numberOf(data, "timestamp");
  return raw ? new Date(raw).toLocaleTimeString("zh-CN", { hour12: false }) : "--:--:--";
}

function isAlarm(data: Telemetry | null, source: Source) {
  if (!data) return false;
  const battery = numberOf(data, "battery");
  const temperature = numberOf(data, source === "stm32" ? "dht_temperature" : "temperature");
  const humidity = numberOf(data, source === "stm32" ? "dht_humidity" : "humidity");
  return (battery > 0 && battery < 20) || temperature > 50 || temperature < -20
    || humidity > 90 || numberOf(data, "alarm_code") > 0;
}

function MetricCard({ label, value, unit, tone = "cyan" }: { label: string; value: string; unit?: string; tone?: string }) {
  return (
    <article className={`metric-card tone-${tone}`}>
      <span>{label}</span>
      <strong>{value}</strong>
      {unit && <small>{unit}</small>}
    </article>
  );
}

function LineChart({ title, unit, color, values }: { title: string; unit: string; color: string; values: Sample[] }) {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const draw = () => {
      const rect = canvas.getBoundingClientRect();
      const dpr = window.devicePixelRatio || 1;
      canvas.width = Math.max(1, Math.floor(rect.width * dpr));
      canvas.height = Math.max(1, Math.floor(rect.height * dpr));
      const ctx = canvas.getContext("2d");
      if (!ctx) return;
      ctx.scale(dpr, dpr);
      const width = rect.width;
      const height = rect.height;
      ctx.clearRect(0, 0, width, height);
      ctx.strokeStyle = "rgba(130, 171, 191, .14)";
      ctx.lineWidth = 1;
      for (let i = 1; i < 4; i += 1) {
        const y = (height / 4) * i;
        ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(width, y); ctx.stroke();
      }
      if (values.length < 2) return;
      const raw = values.map((item) => item.value);
      let min = Math.min(...raw);
      let max = Math.max(...raw);
      const padding = Math.max((max - min) * 0.18, 1);
      min -= padding; max += padding;
      ctx.beginPath();
      values.forEach((item, index) => {
        const x = (index / (values.length - 1)) * width;
        const y = height - ((item.value - min) / (max - min)) * height;
        if (index === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
      });
      ctx.strokeStyle = color;
      ctx.lineWidth = 2.5;
      ctx.shadowColor = color;
      ctx.shadowBlur = 8;
      ctx.stroke();
    };
    draw();
    const observer = new ResizeObserver(draw);
    observer.observe(canvas);
    return () => observer.disconnect();
  }, [values, color]);

  const latest = values.at(-1)?.value;
  return (
    <article className="chart-card">
      <header><span>{title}</span><strong>{latest === undefined ? "--" : latest.toFixed(1)} {unit}</strong></header>
      <canvas ref={canvasRef} aria-label={`${title}实时曲线`} />
    </article>
  );
}

function FlightMap({ points, heading }: { points: Point[]; heading: number }) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const redrawRef = useRef(0);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const token = ++redrawRef.current;
    const center = points.at(-1) ?? { lat: 40.105255, lon: 116.365137 };
    const zoom = 16;
    const tileSize = 256;
    const world = (point: Point) => {
      const sin = Math.sin((point.lat * Math.PI) / 180);
      return {
        x: ((point.lon + 180) / 360) * tileSize * 2 ** zoom,
        y: (0.5 - Math.log((1 + sin) / (1 - sin)) / (4 * Math.PI)) * tileSize * 2 ** zoom,
      };
    };
    const draw = async () => {
      const rect = canvas.getBoundingClientRect();
      const dpr = window.devicePixelRatio || 1;
      canvas.width = Math.max(1, Math.floor(rect.width * dpr));
      canvas.height = Math.max(1, Math.floor(rect.height * dpr));
      const ctx = canvas.getContext("2d");
      if (!ctx) return;
      ctx.scale(dpr, dpr);
      ctx.fillStyle = "#10212b";
      ctx.fillRect(0, 0, rect.width, rect.height);
      const centerWorld = world(center);
      const startX = centerWorld.x - rect.width / 2;
      const startY = centerWorld.y - rect.height / 2;
      const firstTileX = Math.floor(startX / tileSize);
      const firstTileY = Math.floor(startY / tileSize);
      const lastTileX = Math.floor((startX + rect.width) / tileSize);
      const lastTileY = Math.floor((startY + rect.height) / tileSize);
      const tasks: Promise<void>[] = [];
      for (let x = firstTileX; x <= lastTileX; x += 1) {
        for (let y = firstTileY; y <= lastTileY; y += 1) {
          tasks.push(new Promise((resolve) => {
            const image = new Image();
            image.onload = () => {
              if (token === redrawRef.current) ctx.drawImage(image, x * tileSize - startX, y * tileSize - startY, tileSize, tileSize);
              resolve();
            };
            image.onerror = () => resolve();
            image.src = `https://tile.openstreetmap.org/${zoom}/${x}/${y}.png`;
          }));
        }
      }
      await Promise.all(tasks);
      if (token !== redrawRef.current) return;
      if (points.length > 1) {
        ctx.beginPath();
        points.forEach((point, index) => {
          const pos = world(point);
          const x = pos.x - startX;
          const y = pos.y - startY;
          if (index === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
        });
        ctx.strokeStyle = "#14d9c5";
        ctx.lineWidth = 4;
        ctx.shadowColor = "#14d9c5";
        ctx.shadowBlur = 10;
        ctx.stroke();
      }
      const marker = world(center);
      const mx = marker.x - startX;
      const my = marker.y - startY;
      ctx.save();
      ctx.translate(mx, my);
      ctx.rotate((heading * Math.PI) / 180);
      ctx.fillStyle = "#ffbf47";
      ctx.beginPath(); ctx.moveTo(0, -15); ctx.lineTo(11, 11); ctx.lineTo(0, 7); ctx.lineTo(-11, 11); ctx.closePath(); ctx.fill();
      ctx.restore();
    };
    draw();
    const observer = new ResizeObserver(draw);
    observer.observe(canvas);
    return () => observer.disconnect();
  }, [points, heading]);

  const latest = points.at(-1);
  return (
    <section className="map-card">
      <div className="section-heading map-heading">
        <div><span className="eyebrow">LIVE FLIGHT PATH</span><h2>实时航线</h2></div>
        <span className="coordinate">{latest ? `${latest.lat.toFixed(6)}, ${latest.lon.toFixed(6)}` : "等待定位数据"}</span>
      </div>
      <canvas ref={canvasRef} aria-label="无人机实时航线地图" />
      <span className="map-credit">© OpenStreetMap contributors</span>
    </section>
  );
}

export function DashboardClient() {
  const clientRef = useRef<MqttClient | null>(null);
  const [host, setHost] = useState(DEFAULT_HOST);
  const [username, setUsername] = useState("DroneGroundStation");
  const [password, setPassword] = useState("");
  const [connected, setConnected] = useState(false);
  const [connecting, setConnecting] = useState(false);
  const [status, setStatus] = useState("等待连接 EMQX Cloud");
  const [activeSource, setActiveSource] = useState<Source>("stm32");
  const [data, setData] = useState<Record<Source, Telemetry | null>>({ stm32: null, phone: null });
  const [lastSeen, setLastSeen] = useState<Record<Source, number>>({ stm32: 0, phone: 0 });
  const [messageCount, setMessageCount] = useState(0);
  const [historyStatus, setHistoryStatus] = useState("读取云端历史");
  const [paths, setPaths] = useState<Record<Source, Point[]>>({ stm32: [], phone: [] });
  const [series, setSeries] = useState<Record<Source, Record<string, Sample[]>>>({
    stm32: { altitude: [], speed: [], battery: [], temperature: [], humidity: [] },
    phone: { altitude: [], speed: [], battery: [], temperature: [], humidity: [] },
  });

  const disconnect = useCallback(() => {
    clientRef.current?.end(true);
    clientRef.current = null;
    setConnected(false);
    setConnecting(false);
    setStatus("已断开云端连接");
  }, []);

  useEffect(() => () => clientRef.current?.end(true), []);

  const handleMessage = useCallback((topic: string, payload: Uint8Array) => {
    try {
      const parsed = JSON.parse(new TextDecoder().decode(payload)) as Telemetry;
      const source: Source = topic.includes("/phone/") || parsed.source === "phone" ? "phone" : "stm32";
      const now = Date.now();
      setData((previous) => ({ ...previous, [source]: parsed }));
      setLastSeen((previous) => ({ ...previous, [source]: now }));
      setMessageCount((count) => count + 1);
      const lat = numberOf(parsed, "latitude", "lat");
      const lon = numberOf(parsed, "longitude", "lng", "lon");
      if (Math.abs(lat) > 0.000001 && Math.abs(lon) > 0.000001) {
        setPaths((previous) => ({ ...previous, [source]: [...previous[source], { lat, lon }].slice(-500) }));
      }
      const nextValues = {
        altitude: numberOf(parsed, "target_altitude", "altitude", "height"),
        speed: numberOf(parsed, "speed"),
        battery: numberOf(parsed, "battery"),
        temperature: numberOf(parsed, source === "stm32" ? "dht_temperature" : "temperature"),
        humidity: numberOf(parsed, source === "stm32" ? "dht_humidity" : "humidity"),
      };
      setSeries((previous) => {
        const sourceSeries = { ...previous[source] };
        Object.entries(nextValues).forEach(([key, value]) => {
          sourceSeries[key] = [...sourceSeries[key], { time: now, value }].slice(-MAX_SAMPLES);
        });
        return { ...previous, [source]: sourceSeries };
      });
    } catch {
      setStatus(`收到无法解析的消息：${topic}`);
    }
  }, []);

  const connect = (event: FormEvent) => {
    event.preventDefault();
    if (!host.trim() || !username.trim() || !password) {
      setStatus("请完整填写连接地址、用户名和密码");
      return;
    }
    clientRef.current?.end(true);
    setConnecting(true);
    setStatus("正在建立安全连接…");
    const client = mqtt.connect(`wss://${host.trim()}:8084/mqtt`, {
      username: username.trim(), password, clean: true, protocolVersion: 4,
      clientId: `DGS-Web-${Math.random().toString(16).slice(2, 10)}`,
      reconnectPeriod: 3000, connectTimeout: 10000, keepalive: 30,
    });
    clientRef.current = client;
    client.on("connect", () => {
      setConnected(true); setConnecting(false); setStatus("云端已连接，正在接收 dgs/#");
      client.subscribe("dgs/#", { qos: 1 }, (error) => error && setStatus(`订阅失败：${error.message}`));
    });
    client.on("reconnect", () => { setConnected(false); setConnecting(true); setStatus("连接中断，正在自动重连…"); });
    client.on("offline", () => { setConnected(false); setStatus("网络暂时离线，等待恢复…"); });
    client.on("error", (error) => { setConnected(false); setConnecting(false); setStatus(`连接错误：${error.message}`); });
    client.on("message", handleMessage);
  };

  const loadCloudHistory = async () => {
    setHistoryStatus("正在读取…");
    try {
      const response = await fetch(`/api/history?source=${activeSource}&limit=500`, { cache: "no-store" });
      const result = await response.json() as { records?: HistoryRecord[]; error?: string };
      if (!response.ok) throw new Error(result.error || "读取失败");
      const records = [...(result.records ?? [])].reverse();
      const historyPoints = records
        .filter((row) => Number.isFinite(row.latitude) && Number.isFinite(row.longitude)
          && Math.abs(row.latitude ?? 0) > 0.000001 && Math.abs(row.longitude ?? 0) > 0.000001)
        .map((row) => ({ lat: Number(row.latitude), lon: Number(row.longitude) }));
      const keys = ["altitude", "speed", "battery", "temperature", "humidity"] as const;
      const historySeries: Record<string, Sample[]> = {};
      keys.forEach((key) => {
        historySeries[key] = records
          .filter((row) => Number.isFinite(row[key]))
          .map((row) => ({ time: row.receivedAt, value: Number(row[key]) }))
          .slice(-MAX_SAMPLES);
      });
      setPaths((previous) => ({ ...previous, [activeSource]: historyPoints.slice(-500) }));
      setSeries((previous) => ({ ...previous, [activeSource]: historySeries }));
      setHistoryStatus(`已读取 ${records.length} 条`);
    } catch (error) {
      setHistoryStatus(error instanceof Error ? error.message : "读取失败");
    }
  };

  const current = data[activeSource];
  const currentSeries = series[activeSource];
  const activeAlarm = isAlarm(current, activeSource);
  const altitude = numberOf(current, "target_altitude", "altitude", "height");
  const temperature = numberOf(current, activeSource === "stm32" ? "dht_temperature" : "temperature");
  const humidity = numberOf(current, activeSource === "stm32" ? "dht_humidity" : "humidity");
  const lat = numberOf(current, "latitude", "lat");
  const lon = numberOf(current, "longitude", "lng", "lon");
  const sourceOnline = (source: Source) => Date.now() - lastSeen[source] < 15000;
  const rawJson = useMemo(() => current ? JSON.stringify(current, null, 2) : "等待遥测消息…", [current]);

  return (
    <main>
      <header className="topbar">
        <div className="brand-mark">▲</div>
        <div className="brand"><span>DRONEGROUNDSTATION</span><strong>云端实时监控中心</strong></div>
        <div className="topbar-spacer" />
        <div className={`cloud-pill ${connected ? "online" : ""}`}><i />{connected ? "EMQX 在线" : "云端未连接"}</div>
        <div className="clock">消息 {messageCount}</div>
      </header>

      <section className="connection-card">
        <div>
          <span className="eyebrow">SECURE MQTT OVER WSS</span>
          <h1>连接云端遥测</h1>
          <p>{status}</p>
        </div>
        <form onSubmit={connect}>
          <label>Broker<input value={host} onChange={(e) => setHost(e.target.value)} aria-label="EMQX Broker 地址" /></label>
          <label>用户名<input value={username} onChange={(e) => setUsername(e.target.value)} autoComplete="username" /></label>
          <label>密码<input value={password} onChange={(e) => setPassword(e.target.value)} type="password" autoComplete="current-password" placeholder="仅保留在当前页面" /></label>
          {!connected ? <button type="submit" disabled={connecting}>{connecting ? "连接中…" : "连接并订阅"}</button>
            : <button className="secondary" type="button" onClick={disconnect}>断开</button>}
        </form>
      </section>

      <nav className="source-tabs" aria-label="数据源切换">
        {(["stm32", "phone"] as Source[]).map((source) => (
          <button key={source} className={activeSource === source ? "active" : ""} onClick={() => setActiveSource(source)}>
            <i className={sourceOnline(source) ? "online" : ""} />{source === "stm32" ? "STM32 无人机" : "手机传感器"}
            <small>{sourceOnline(source) ? "实时" : data[source] ? "已离线" : "等待数据"}</small>
          </button>
        ))}
        <div className={`alarm-banner ${activeAlarm ? "active" : ""}`}>{activeAlarm ? "检测到遥测异常" : "当前数据正常"}</div>
      </nav>

      <section className="metrics-grid">
        <MetricCard label="目标高度" value={current ? altitude.toFixed(1) : "--"} unit="m" />
        <MetricCard label="实时速度" value={current ? numberOf(current, "speed").toFixed(1) : "--"} unit="m/s" tone="blue" />
        <MetricCard label="飞行器电量" value={current ? numberOf(current, "battery").toFixed(0) : "--"} unit="%" tone={numberOf(current, "battery") < 20 ? "red" : "amber"} />
        <MetricCard label="航向角" value={current ? numberOf(current, "heading", "yaw").toFixed(1) : "--"} unit="°" tone="violet" />
        <MetricCard label="环境温度" value={current ? temperature.toFixed(1) : "--"} unit="℃" tone="orange" />
        <MetricCard label="环境湿度" value={current ? humidity.toFixed(1) : "--"} unit="%" tone="green" />
      </section>

      <section className="primary-grid">
        <FlightMap points={paths[activeSource]} heading={numberOf(current, "heading", "yaw")} />
        <aside className="telemetry-panel">
          <div className="section-heading"><div><span className="eyebrow">TELEMETRY</span><h2>遥测详情</h2></div><time>{displayTime(current)}</time></div>
          <dl>
            <div><dt>设备 ID</dt><dd>{textOf(current, "device_id", activeSource === "phone" ? "PHONE" : "--")}</dd></div>
            <div><dt>数据序列</dt><dd>{textOf(current, "sequence")}</dd></div>
            <div><dt>当前纬度</dt><dd>{current ? lat.toFixed(7) : "--"}</dd></div>
            <div><dt>当前经度</dt><dd>{current ? lon.toFixed(7) : "--"}</dd></div>
            <div><dt>飞行模式</dt><dd>{textOf(current, "flight_mode", activeSource === "phone" ? "手机定位" : "--")}</dd></div>
            <div><dt>MCU 温度</dt><dd>{current && activeSource === "stm32" ? `${numberOf(current, "mcu_temperature").toFixed(1)} ℃` : "--"}</dd></div>
            <div><dt>返航距离</dt><dd>{current && activeSource === "stm32" ? `${numberOf(current, "rth_distance").toFixed(1)} m` : "--"}</dd></div>
            <div><dt>告警代码</dt><dd className={numberOf(current, "alarm_code") ? "danger" : "good"}>{textOf(current, "alarm_code", "0")}</dd></div>
          </dl>
        </aside>
      </section>

      <section className="charts-section">
        <div className="section-heading">
          <div><span className="eyebrow">LIVE + CLOUD HISTORY</span><h2>实时与云端历史曲线</h2></div>
          <button className="history-button" onClick={loadCloudHistory}>{historyStatus}</button>
        </div>
        <div className="charts-grid">
          <LineChart title="高度" unit="m" color="#45d6f2" values={currentSeries.altitude} />
          <LineChart title="速度" unit="m/s" color="#8b7cf6" values={currentSeries.speed} />
          <LineChart title="电量" unit="%" color="#ffbf47" values={currentSeries.battery} />
          <LineChart title="温度" unit="℃" color="#ff7a66" values={currentSeries.temperature} />
          <LineChart title="湿度" unit="%" color="#52d99a" values={currentSeries.humidity} />
        </div>
      </section>

      <details className="raw-panel"><summary>查看最新原始 JSON</summary><pre>{rawJson}</pre></details>
      <footer>DroneGroundStation Cloud · TLS/WSS · 订阅 dgs/# · 密码不会写入网页源代码</footer>
    </main>
  );
}
