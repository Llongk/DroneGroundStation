import { and, desc, eq, lt } from "drizzle-orm";
import { getDb } from "../../../db";
import { telemetryRecords } from "../../../db/schema";

export async function GET(request: Request) {
  try {
    const url = new URL(request.url);
    const source = url.searchParams.get("source") === "phone" ? "phone" : "stm32";
    const limit = Math.min(Math.max(Number(url.searchParams.get("limit")) || 500, 1), 1000);
    const before = Number(url.searchParams.get("before"));
    const condition = Number.isFinite(before) && before > 0
      ? and(eq(telemetryRecords.source, source), lt(telemetryRecords.receivedAt, before))
      : eq(telemetryRecords.source, source);
    const rows = await getDb().select().from(telemetryRecords)
      .where(condition).orderBy(desc(telemetryRecords.receivedAt)).limit(limit);
    return Response.json({ source, records: rows });
  } catch (error) {
    const message = error instanceof Error ? error.message : "history query failed";
    return Response.json({ error: message }, { status: 500 });
  }
}
