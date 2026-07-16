#include "historydatabase.h"

#include "backend.h"
#include "sensorbackend.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QStandardPaths>
#include <QUuid>
#include <QDebug>
#include <QtMath>

#include <algorithm>

namespace {
constexpr double kPhoneTemperatureMin = -20.0;
constexpr double kPhoneTemperatureMax = 40.0;
constexpr double kPhoneHumidityMax = 90.0;
constexpr double kPhoneBatteryLow = 20.0;
constexpr double kPhoneSpeedMax = 50.0;
constexpr double kPhoneAltitudeMin = -100.0;
constexpr double kPhoneAltitudeMax = 500.0;
constexpr double kPhoneAttitudeMax = 50.0;
constexpr double kPhoneAccuracyMax = 50.0;

constexpr double kStm32TemperatureMin = -20.0;
constexpr double kStm32TemperatureMax = 50.0;
constexpr double kStm32HumidityMin = 10.0;
constexpr double kStm32HumidityMax = 90.0;
constexpr double kStm32McuTemperatureMin = -20.0;
constexpr double kStm32McuTemperatureMax = 75.0;
constexpr double kStm32BatteryLow = 20.0;
constexpr double kStm32SpeedMax = 50.0;
constexpr double kStm32AltitudeMin = 0.0;
constexpr double kStm32AltitudeMax = 500.0;
constexpr double kStm32RthDistanceMax = 2000.0;

QString portableProjectDirectory()
{
    // During development the executable is normally inside
    // <project>/build/<kit>. Walk upwards so the database remains under the
    // project itself. For a deployed copy without sources, keep it beside the
    // executable; moving that directory then moves the database with it.
    const QString applicationDirectory = QCoreApplication::applicationDirPath();
    QDir candidate(applicationDirectory);
    for (int depth = 0; depth < 8; ++depth) {
        if (QFileInfo::exists(candidate.filePath(QStringLiteral("CMakeLists.txt")))
            && QFileInfo::exists(candidate.filePath(QStringLiteral("historydatabase.cpp")))) {
            return candidate.absolutePath();
        }
        if (!candidate.cdUp())
            break;
    }
    return applicationDirectory;
}

QVariantMap recordMap(const QSqlQuery &query, const QString &source)
{
    QVariantMap row;
    row.insert(QStringLiteral("sessionId"), query.value(QStringLiteral("session_id")));
    row.insert(QStringLiteral("username"), query.value(QStringLiteral("username")));
    row.insert(QStringLiteral("source"), source);
    row.insert(QStringLiteral("sourceName"),
               source == QStringLiteral("phone") ? QStringLiteral("手机")
                                                   : QStringLiteral("STM32"));
    row.insert(QStringLiteral("startTime"), query.value(QStringLiteral("start_time")));
    row.insert(QStringLiteral("endTime"), query.value(QStringLiteral("end_time")));
    row.insert(QStringLiteral("sampleCount"), query.value(QStringLiteral("sample_count")));
    row.insert(QStringLiteral("startLat"), query.value(QStringLiteral("start_lat")));
    row.insert(QStringLiteral("startLon"), query.value(QStringLiteral("start_lon")));
    row.insert(QStringLiteral("endLat"), query.value(QStringLiteral("end_lat")));
    row.insert(QStringLiteral("endLon"), query.value(QStringLiteral("end_lon")));
    row.insert(QStringLiteral("startBattery"), query.value(QStringLiteral("start_battery")));
    row.insert(QStringLiteral("endBattery"), query.value(QStringLiteral("end_battery")));
    row.insert(QStringLiteral("alarmCount"), query.value(QStringLiteral("alarm_count")));
    return row;
}
}

HistoryDatabase::HistoryDatabase(Backend *backend,
                                 SensorBackend *sensorBackend,
                                 QObject *parent)
    : QObject(parent),
      m_backend(backend),
      m_sensorBackend(sensorBackend),
      m_connectionName(QStringLiteral("flight_history_%1")
                           .arg(reinterpret_cast<quintptr>(this)))
{
    const QString dataDirectory =
        QDir(portableProjectDirectory()).filePath(QStringLiteral("database"));
    if (!QDir().mkpath(dataDirectory))
        qCritical() << "Cannot create history database directory:" << dataDirectory;

    m_databasePath = QDir(dataDirectory).filePath(QStringLiteral("FlightHistory.db"));

    // Preserve installations that previously stored the database in AppData.
    // This one-time migration only runs when the portable database is absent.
    const QString legacyPath = QDir(
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
                                   .filePath(QStringLiteral("FlightHistory.db"));
    if (!QFileInfo::exists(m_databasePath) && QFileInfo::exists(legacyPath)) {
        if (QFile::copy(legacyPath, m_databasePath))
            qInfo() << "Migrated history database from" << legacyPath;
        else
            qWarning() << "Could not migrate history database from" << legacyPath;
    }

    if (openDatabase() && createTables()) {
        connect(m_backend, &Backend::gpsChanged,
                this, &HistoryDatabase::recordPhoneGps);
        connect(m_sensorBackend, &SensorBackend::sensorChanged,
                this, &HistoryDatabase::recordPhoneSensor);
        connect(m_sensorBackend, &SensorBackend::telemetryChanged,
                this, &HistoryDatabase::recordStm32Telemetry);
    }
}

HistoryDatabase::~HistoryDatabase()
{
    if (m_database.isOpen())
        m_database.close();
    m_database = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connectionName);
}

QString HistoryDatabase::databasePath() const { return m_databasePath; }
QString HistoryDatabase::currentSessionId() const { return m_sessionId; }

bool HistoryDatabase::openDatabase()
{
    m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                           m_connectionName);
    m_database.setDatabaseName(m_databasePath);
    if (!m_database.open()) {
        qCritical() << "Cannot open history database:"
                    << m_database.lastError().text();
        return false;
    }

    QSqlQuery pragma(m_database);
    pragma.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    pragma.exec(QStringLiteral("PRAGMA synchronous=NORMAL"));
    pragma.exec(QStringLiteral("PRAGMA foreign_keys=ON"));
    qInfo() << "Flight history database:" << m_databasePath;
    return true;
}

bool HistoryDatabase::createTables()
{
    QSqlQuery query(m_database);
    const QString phoneSql = QStringLiteral(R"SQL(
        CREATE TABLE IF NOT EXISTS phone_data (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            session_id TEXT NOT NULL,
            username TEXT NOT NULL,
            recorded_at TEXT NOT NULL,
            recorded_ms INTEGER NOT NULL,
            event_type TEXT NOT NULL,
            latitude REAL, longitude REAL, altitude REAL, speed REAL,
            battery REAL, pitch REAL, roll REAL, yaw REAL, accuracy REAL,
            gps_time TEXT, temperature REAL, humidity REAL
        )
    )SQL");
    const QString stm32Sql = QStringLiteral(R"SQL(
        CREATE TABLE IF NOT EXISTS stm32_data (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            session_id TEXT NOT NULL,
            username TEXT NOT NULL,
            recorded_at TEXT NOT NULL,
            recorded_ms INTEGER NOT NULL,
            device_id TEXT, proto INTEGER, sequence INTEGER,
            latitude REAL, longitude REAL, heading REAL,
            target_altitude REAL, speed REAL,
            rth_latitude REAL, rth_longitude REAL, rth_distance REAL,
            dht_temperature REAL, dht_humidity REAL,
            sht_temperature REAL, sht_humidity REAL, mcu_temperature REAL,
            flight_mode INTEGER, alarm_code INTEGER, battery INTEGER,
            device_timestamp INTEGER
        )
    )SQL");

    if (!query.exec(phoneSql) || !query.exec(stm32Sql)) {
        qCritical() << "Cannot create history tables:" << query.lastError().text();
        return false;
    }
    query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_phone_session_time ON phone_data(session_id, recorded_ms)"));
    query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_stm32_session_time ON stm32_data(session_id, recorded_ms)"));
    return true;
}

bool HistoryDatabase::startSession(const QString &username)
{
    if (!m_database.isOpen() || username.trimmed().isEmpty())
        return false;

    const QString timestamp =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QString suffix = QUuid::createUuid().toString(QUuid::Id128).left(6);
    m_sessionId = QStringLiteral("%1_%2").arg(timestamp, suffix);
    m_username = username.trimmed();
    emit sessionChanged();
    qInfo() << "History recording session started:" << m_sessionId << m_username;
    return true;
}

void HistoryDatabase::recordPhoneGps()
{
    insertPhoneSnapshot(QStringLiteral("gps"));
}

void HistoryDatabase::recordPhoneSensor()
{
    insertPhoneSnapshot(QStringLiteral("sensor"));
}

bool HistoryDatabase::insertPhoneSnapshot(const QString &eventType)
{
    if (m_sessionId.isEmpty() || !m_database.isOpen())
        return false;

    const QDateTime now = QDateTime::currentDateTime();
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(R"SQL(
        INSERT INTO phone_data (
            session_id, username, recorded_at, recorded_ms, event_type,
            latitude, longitude, altitude, speed, battery,
            pitch, roll, yaw, accuracy, gps_time, temperature, humidity
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )SQL"));
    query.addBindValue(m_sessionId);
    query.addBindValue(m_username);
    query.addBindValue(now.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")));
    query.addBindValue(now.toMSecsSinceEpoch());
    query.addBindValue(eventType);
    query.addBindValue(m_backend->latitude());
    query.addBindValue(m_backend->longitude());
    query.addBindValue(m_backend->height());
    query.addBindValue(m_backend->speed());
    query.addBindValue(m_backend->battery());
    query.addBindValue(m_backend->pitch());
    query.addBindValue(m_backend->roll());
    query.addBindValue(m_backend->yaw());
    query.addBindValue(m_backend->accuracy());
    query.addBindValue(m_backend->gpsTime());
    query.addBindValue(m_sensorBackend->temperature());
    query.addBindValue(m_sensorBackend->humidity());
    if (!query.exec()) {
        qWarning() << "Cannot insert phone history:" << query.lastError().text();
        return false;
    }
    emit historyChanged();
    return true;
}

void HistoryDatabase::recordStm32Telemetry()
{
    if (m_sessionId.isEmpty() || !m_database.isOpen()
        || !m_sensorBackend->telemetryValid()) {
        return;
    }

    const QDateTime now = QDateTime::currentDateTime();
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(R"SQL(
        INSERT INTO stm32_data (
            session_id, username, recorded_at, recorded_ms,
            device_id, proto, sequence, latitude, longitude, heading,
            target_altitude, speed, rth_latitude, rth_longitude, rth_distance,
            dht_temperature, dht_humidity, sht_temperature, sht_humidity,
            mcu_temperature, flight_mode, alarm_code, battery, device_timestamp
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )SQL"));
    query.addBindValue(m_sessionId);
    query.addBindValue(m_username);
    query.addBindValue(now.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")));
    query.addBindValue(now.toMSecsSinceEpoch());
    query.addBindValue(m_sensorBackend->deviceId());
    query.addBindValue(m_sensorBackend->protocolVersion());
    query.addBindValue(QVariant::fromValue(m_sensorBackend->sequence()));
    query.addBindValue(m_sensorBackend->stm32Latitude());
    query.addBindValue(m_sensorBackend->stm32Longitude());
    query.addBindValue(m_sensorBackend->heading());
    query.addBindValue(m_sensorBackend->targetAltitude());
    query.addBindValue(m_sensorBackend->stm32Speed());
    query.addBindValue(m_sensorBackend->rthLatitude());
    query.addBindValue(m_sensorBackend->rthLongitude());
    query.addBindValue(m_sensorBackend->rthDistance());
    query.addBindValue(m_sensorBackend->dhtTemperature());
    query.addBindValue(m_sensorBackend->dhtHumidity());
    query.addBindValue(m_sensorBackend->shtTemperature());
    query.addBindValue(m_sensorBackend->shtHumidity());
    query.addBindValue(m_sensorBackend->mcuTemperature());
    query.addBindValue(m_sensorBackend->flightMode());
    query.addBindValue(m_sensorBackend->alarmCode());
    query.addBindValue(m_sensorBackend->stm32Battery());
    query.addBindValue(QVariant::fromValue(m_sensorBackend->telemetryTimestamp()));
    if (!query.exec()) {
        qWarning() << "Cannot insert STM32 history:" << query.lastError().text();
        return;
    }
    emit historyChanged();
}

QVariantList HistoryDatabase::getHistoryRecords() const
{
    QVariantList result;
    if (!m_database.isOpen())
        return result;

    const struct SourceQuery { QString source; QString sql; } queries[] = {
        { QStringLiteral("phone"), QStringLiteral(R"SQL(
            SELECT p.session_id, p.username,
                   MIN(p.recorded_at) AS start_time,
                   MAX(p.recorded_at) AS end_time,
                   COUNT(*) AS sample_count,
                   (SELECT latitude FROM phone_data x WHERE x.session_id=p.session_id ORDER BY id LIMIT 1) AS start_lat,
                   (SELECT longitude FROM phone_data x WHERE x.session_id=p.session_id ORDER BY id LIMIT 1) AS start_lon,
                   (SELECT latitude FROM phone_data x WHERE x.session_id=p.session_id ORDER BY id DESC LIMIT 1) AS end_lat,
                   (SELECT longitude FROM phone_data x WHERE x.session_id=p.session_id ORDER BY id DESC LIMIT 1) AS end_lon,
                   (SELECT battery FROM phone_data x WHERE x.session_id=p.session_id ORDER BY id LIMIT 1) AS start_battery,
                   (SELECT battery FROM phone_data x WHERE x.session_id=p.session_id ORDER BY id DESC LIMIT 1) AS end_battery,
                   SUM(CASE WHEN
                       (temperature < -20 OR temperature > 40)
                       OR humidity > 90
                       OR (battery > 0 AND battery < 20)
                       OR speed > 50
                       OR altitude < -100 OR altitude > 500
                       OR ABS(pitch) > 50 OR ABS(roll) > 50
                       OR accuracy > 50
                       THEN 1 ELSE 0 END) AS alarm_count
            FROM phone_data p GROUP BY p.session_id, p.username
        )SQL") },
        { QStringLiteral("stm32"), QStringLiteral(R"SQL(
            SELECT p.session_id, p.username,
                   MIN(p.recorded_at) AS start_time,
                   MAX(p.recorded_at) AS end_time,
                   COUNT(*) AS sample_count,
                   (SELECT latitude FROM stm32_data x WHERE x.session_id=p.session_id ORDER BY id LIMIT 1) AS start_lat,
                   (SELECT longitude FROM stm32_data x WHERE x.session_id=p.session_id ORDER BY id LIMIT 1) AS start_lon,
                   (SELECT latitude FROM stm32_data x WHERE x.session_id=p.session_id ORDER BY id DESC LIMIT 1) AS end_lat,
                   (SELECT longitude FROM stm32_data x WHERE x.session_id=p.session_id ORDER BY id DESC LIMIT 1) AS end_lon,
                   (SELECT battery FROM stm32_data x WHERE x.session_id=p.session_id ORDER BY id LIMIT 1) AS start_battery,
                   (SELECT battery FROM stm32_data x WHERE x.session_id=p.session_id ORDER BY id DESC LIMIT 1) AS end_battery,
                   SUM(CASE WHEN
                       alarm_code <> 0
                       OR battery < 20
                       OR speed > 50
                       OR target_altitude < 0 OR target_altitude > 500
                       OR rth_distance > 2000
                       OR dht_temperature < -20 OR dht_temperature > 50
                       OR sht_temperature < -20 OR sht_temperature > 50
                       OR dht_humidity < 10 OR dht_humidity > 90
                       OR sht_humidity < 10 OR sht_humidity > 90
                       OR mcu_temperature < -20 OR mcu_temperature > 75
                       OR (ABS(latitude) < 0.000001 AND ABS(longitude) < 0.000001)
                       THEN 1 ELSE 0 END) AS alarm_count
            FROM stm32_data p GROUP BY p.session_id, p.username
        )SQL") }
    };

    for (const SourceQuery &sourceQuery : queries) {
        QSqlQuery query(m_database);
        if (!query.exec(sourceQuery.sql)) {
            qWarning() << "Cannot query history records:" << query.lastError().text();
            continue;
        }
        while (query.next()) {
            QVariantMap row = recordMap(query, sourceQuery.source);
            const QDateTime start = QDateTime::fromString(
                row.value(QStringLiteral("startTime")).toString(),
                QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
            const QDateTime end = QDateTime::fromString(
                row.value(QStringLiteral("endTime")).toString(),
                QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
            row.insert(QStringLiteral("durationSeconds"), start.secsTo(end));
            row.insert(QStringLiteral("startPos"),
                       formatPosition(row.value(QStringLiteral("startLat")).toDouble(),
                                      row.value(QStringLiteral("startLon")).toDouble()));
            row.insert(QStringLiteral("endPos"),
                       formatPosition(row.value(QStringLiteral("endLat")).toDouble(),
                                      row.value(QStringLiteral("endLon")).toDouble()));
            result.append(row);
        }
    }
    std::sort(result.begin(), result.end(), [](const QVariant &left, const QVariant &right) {
        return left.toMap().value(QStringLiteral("startTime")).toString()
            > right.toMap().value(QStringLiteral("startTime")).toString();
    });
    return result;
}

QVariantList HistoryDatabase::getSessionData(const QString &sessionId,
                                             const QString &source) const
{
    QVariantList result;
    if (!m_database.isOpen() || sessionId.isEmpty())
        return result;

    QSqlQuery query(m_database);
    if (source == QStringLiteral("stm32")) {
        query.prepare(QStringLiteral(R"SQL(
            SELECT recorded_at, recorded_ms, latitude, longitude,
                   target_altitude AS altitude, speed,
                   sht_temperature AS temperature, sht_humidity AS humidity,
                   battery, heading, flight_mode, alarm_code,
                   rth_distance, dht_temperature, dht_humidity,
                   sht_temperature, sht_humidity, mcu_temperature
            FROM stm32_data WHERE session_id=? ORDER BY recorded_ms, id
        )SQL"));
    } else {
        query.prepare(QStringLiteral(R"SQL(
            SELECT recorded_at, recorded_ms, latitude, longitude,
                   altitude, speed, temperature, humidity, battery,
                   yaw AS heading, 0 AS flight_mode, 0 AS alarm_code,
                   pitch, roll, accuracy
            FROM phone_data WHERE session_id=? ORDER BY recorded_ms, id
        )SQL"));
    }
    query.addBindValue(sessionId);
    if (!query.exec()) {
        qWarning() << "Cannot query session data:" << query.lastError().text();
        return result;
    }
    while (query.next()) {
        QVariantMap row;
        const auto record = query.record();
        for (int i = 0; i < record.count(); ++i)
            row.insert(record.fieldName(i), query.value(i));
        result.append(row);
    }
    return result;
}

QVariantList HistoryDatabase::getAbnormalData(const QString &sessionId,
                                              const QString &source) const
{
    QVariantList result;
    const QVariantList samples = getSessionData(sessionId, source);
    for (const QVariant &sample : samples) {
        const QVariantMap data = sample.toMap();
        QStringList events;
        const double battery = data.value(QStringLiteral("battery")).toDouble();
        const double speed = data.value(QStringLiteral("speed")).toDouble();
        const double altitude = data.value(QStringLiteral("altitude")).toDouble();
        const double temperature = data.value(QStringLiteral("temperature")).toDouble();
        const double humidity = data.value(QStringLiteral("humidity")).toDouble();

        if (source == QStringLiteral("stm32")) {
            const int alarmCode = data.value(QStringLiteral("alarm_code")).toInt();
            const double dhtTemperature = data.value(QStringLiteral("dht_temperature")).toDouble();
            const double dhtHumidity = data.value(QStringLiteral("dht_humidity")).toDouble();
            const double mcuTemperature = data.value(QStringLiteral("mcu_temperature")).toDouble();
            const double rthDistance = data.value(QStringLiteral("rth_distance")).toDouble();
            const double latitude = data.value(QStringLiteral("latitude")).toDouble();
            const double longitude = data.value(QStringLiteral("longitude")).toDouble();

            if (alarmCode != 0)
                events.append(QStringLiteral("固件告警代码 %1").arg(alarmCode));
            if (battery < kStm32BatteryLow)
                events.append(QStringLiteral("电量 %1%，低于 20%")
                                  .arg(battery, 0, 'f', 0));
            if (speed > kStm32SpeedMax)
                events.append(QStringLiteral("速度 %1 m/s，超过 50 m/s")
                                  .arg(speed, 0, 'f', 1));
            if (altitude < kStm32AltitudeMin || altitude > kStm32AltitudeMax)
                events.append(QStringLiteral("目标高度 %1 m，允许范围 0~500 m")
                                  .arg(altitude, 0, 'f', 1));
            if (rthDistance > kStm32RthDistanceMax)
                events.append(QStringLiteral("返航距离 %1 m，超过 2000 m")
                                  .arg(rthDistance, 0, 'f', 1));
            if (dhtTemperature < kStm32TemperatureMin
                || dhtTemperature > kStm32TemperatureMax) {
                events.append(QStringLiteral("DHT 温度 %1 ℃，允许范围 -20~50 ℃")
                                  .arg(dhtTemperature, 0, 'f', 1));
            }
            if (temperature < kStm32TemperatureMin
                || temperature > kStm32TemperatureMax) {
                events.append(QStringLiteral("SHT 温度 %1 ℃，允许范围 -20~50 ℃")
                                  .arg(temperature, 0, 'f', 1));
            }
            if (dhtHumidity < kStm32HumidityMin
                || dhtHumidity > kStm32HumidityMax) {
                events.append(QStringLiteral("DHT 湿度 %1%，允许范围 10~90%")
                                  .arg(dhtHumidity, 0, 'f', 1));
            }
            if (humidity < kStm32HumidityMin || humidity > kStm32HumidityMax) {
                events.append(QStringLiteral("SHT 湿度 %1%，允许范围 10~90%")
                                  .arg(humidity, 0, 'f', 1));
            }
            if (mcuTemperature < kStm32McuTemperatureMin
                || mcuTemperature > kStm32McuTemperatureMax) {
                events.append(QStringLiteral("MCU 温度 %1 ℃，允许范围 -20~75 ℃")
                                  .arg(mcuTemperature, 0, 'f', 1));
            }
            if (qAbs(latitude) < 0.000001 && qAbs(longitude) < 0.000001)
                events.append(QStringLiteral("STM32 定位坐标无效 (0, 0)"));
        } else {
            const double pitch = data.value(QStringLiteral("pitch")).toDouble();
            const double roll = data.value(QStringLiteral("roll")).toDouble();
            const double accuracy = data.value(QStringLiteral("accuracy")).toDouble();

            if (temperature < kPhoneTemperatureMin
                || temperature > kPhoneTemperatureMax) {
                events.append(QStringLiteral("环境温度 %1 ℃，允许范围 -20~40 ℃")
                                  .arg(temperature, 0, 'f', 1));
            }
            if (humidity > kPhoneHumidityMax)
                events.append(QStringLiteral("环境湿度 %1%，超过 90%")
                                  .arg(humidity, 0, 'f', 1));
            if (battery > 0 && battery < kPhoneBatteryLow)
                events.append(QStringLiteral("电量 %1%，低于 20%")
                                  .arg(battery, 0, 'f', 0));
            if (speed > kPhoneSpeedMax)
                events.append(QStringLiteral("速度 %1 m/s，超过 50 m/s")
                                  .arg(speed, 0, 'f', 1));
            if (altitude < kPhoneAltitudeMin || altitude > kPhoneAltitudeMax)
                events.append(QStringLiteral("高度 %1 m，允许范围 -100~500 m")
                                  .arg(altitude, 0, 'f', 1));
            if (qAbs(pitch) > kPhoneAttitudeMax)
                events.append(QStringLiteral("俯仰角 %1°，超过 ±50°")
                                  .arg(pitch, 0, 'f', 1));
            if (qAbs(roll) > kPhoneAttitudeMax)
                events.append(QStringLiteral("横滚角 %1°，超过 ±50°")
                                  .arg(roll, 0, 'f', 1));
            if (accuracy > kPhoneAccuracyMax)
                events.append(QStringLiteral("GPS 精度 %1 m，误差超过 50 m")
                                  .arg(accuracy, 0, 'f', 1));
        }
        if (!events.isEmpty()) {
            QVariantMap abnormal;
            abnormal.insert(QStringLiteral("latitude"), data.value(QStringLiteral("latitude")));
            abnormal.insert(QStringLiteral("longitude"), data.value(QStringLiteral("longitude")));
            abnormal.insert(QStringLiteral("time"), data.value(QStringLiteral("recorded_at")));
            abnormal.insert(QStringLiteral("event"), events.join(QStringLiteral("；")));
            result.append(abnormal);
        }
    }
    return result;
}

QString HistoryDatabase::formatPosition(double latitude, double longitude)
{
    if (qAbs(latitude) < 0.000001 && qAbs(longitude) < 0.000001)
        return QStringLiteral("--");
    return QStringLiteral("%1, %2")
        .arg(latitude, 0, 'f', 6)
        .arg(longitude, 0, 'f', 6);
}
