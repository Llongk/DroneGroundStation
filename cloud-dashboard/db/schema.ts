import { index, integer, real, sqliteTable, text, uniqueIndex } from "drizzle-orm/sqlite-core";

export const telemetryRecords = sqliteTable(
  "telemetry_records",
  {
    id: integer("id").primaryKey({ autoIncrement: true }),
    messageId: text("message_id").notNull(),
    topic: text("topic").notNull(),
    source: text("source").notNull(),
    deviceId: text("device_id"),
    sessionId: text("session_id"),
    sequence: integer("sequence"),
    receivedAt: integer("received_at").notNull(),
    deviceTimestamp: integer("device_timestamp"),
    latitude: real("latitude"),
    longitude: real("longitude"),
    altitude: real("altitude"),
    speed: real("speed"),
    heading: real("heading"),
    battery: real("battery"),
    temperature: real("temperature"),
    humidity: real("humidity"),
    alarmCode: integer("alarm_code"),
    payload: text("payload").notNull(),
  },
  (table) => [
    uniqueIndex("telemetry_message_id_uq").on(table.messageId),
    index("telemetry_source_time_idx").on(table.source, table.receivedAt),
    index("telemetry_session_idx").on(table.sessionId),
  ],
);
