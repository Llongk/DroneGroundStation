CREATE TABLE `telemetry_records` (
	`id` integer PRIMARY KEY AUTOINCREMENT NOT NULL,
	`message_id` text NOT NULL,
	`topic` text NOT NULL,
	`source` text NOT NULL,
	`device_id` text,
	`session_id` text,
	`sequence` integer,
	`received_at` integer NOT NULL,
	`device_timestamp` integer,
	`latitude` real,
	`longitude` real,
	`altitude` real,
	`speed` real,
	`heading` real,
	`battery` real,
	`temperature` real,
	`humidity` real,
	`alarm_code` integer,
	`payload` text NOT NULL
);
--> statement-breakpoint
CREATE UNIQUE INDEX `telemetry_message_id_uq` ON `telemetry_records` (`message_id`);--> statement-breakpoint
CREATE INDEX `telemetry_source_time_idx` ON `telemetry_records` (`source`,`received_at`);--> statement-breakpoint
CREATE INDEX `telemetry_session_idx` ON `telemetry_records` (`session_id`);