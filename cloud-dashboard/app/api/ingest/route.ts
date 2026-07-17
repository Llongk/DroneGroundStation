import { env } from "cloudflare:workers";
import { getDb } from "../../../db";
import { telemetryRecords } from "../../../db/schema";

type JsonObject = Record<string, unknown>;

function numeric(payload: JsonObject, ...keys: string[]) {
  for (const key of keys) {
    const value = Number(payload[key]);
    if (Number.isFinite(value)) return value;
  }
  return null;
}

function stringValue(payload: JsonObject, key: string) {
  const value = payload[key];
  return value === undefined || value === null ? null : String(value);
}

export async function POST(request: Request) {
  try {
    const configuredKey = String(
      (env as unknown as Record<string, unknown>).INGEST_API_KEY ?? "",
    );
    const requestUrl = new URL(request.url);
    const suppliedKey = request.headers.get("x-ingest-key")
      ?? requestUrl.searchParams.get("key")
      ?? "";
    if (!configuredKey || suppliedKey !== configuredKey) {
      return Response.json({ error: "unauthorized" }, { status: 401 });
    }

    const contentLength = Number(request.headers.get("content-length") ?? 0);
    if (contentLength > 128 * 1024) {
      return Response.json({ error: "payload too large" }, { status: 413 });
    }
    const envelope = (await request.json()) as JsonObject;
    const topic = String(envelope.topic ?? "");
    if (!topic.startsWith("dgs/")) {
      return Response.json({ error: "topic is not allowed" }, { status: 400 });
    }
    const payload = (envelope.payload && typeof envelope.payload === "object"
      ? envelope.payload : envelope) as JsonObject;
    const source = topic.includes("/phone/") || payload.source === "phone" ? "phone" : "stm32";
    const receivedAt = Number(envelope.timestamp) || Date.now();
    const sequence = numeric(payload, "sequence");
    const messageId = String(payload.message_id
      ?? `${topic}-${String(envelope.clientid ?? "unknown")}-${receivedAt}-${sequence ?? "x"}`);
    const temperature = numeric(payload, source === "stm32" ? "dht_temperature" : "temperature");
    const humidity = numeric(payload, source === "stm32" ? "dht_humidity" : "humidity");

    await getDb().insert(telemetryRecords).values({
      messageId,
      topic,
      source,
      deviceId: stringValue(payload, "device_id"),
      sessionId: stringValue(payload, "session_id"),
      sequence,
      receivedAt,
      deviceTimestamp: numeric(payload, "timestamp"),
      latitude: numeric(payload, "latitude", "lat"),
      longitude: numeric(payload, "longitude", "lng", "lon"),
      altitude: numeric(payload, "target_altitude", "altitude", "height"),
      speed: numeric(payload, "speed"),
      heading: numeric(payload, "heading", "yaw"),
      battery: numeric(payload, "battery"),
      temperature,
      humidity,
      alarmCode: numeric(payload, "alarm_code"),
      payload: JSON.stringify(payload),
    }).onConflictDoNothing({ target: telemetryRecords.messageId });

    return Response.json({ ok: true, messageId }, { status: 200 });
  } catch (error) {
    const message = error instanceof Error ? error.message : "ingest failed";
    return Response.json({ error: message }, { status: 500 });
  }
}
