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

// 定位项目根目录；独立部署时回退到可执行文件目录。
// 开发期间向上查找 CMakeLists.txt 和 historydatabase.cpp 确认根目录。
QString portableProjectDirectory()
{
    // 开发期间可执行文件通常在 <project>/build/<kit> 下。
    // 向上一级查找以便数据库保留在项目根目录下。
    // 对于独立部署版本（无源码），保持在可执行文件旁边以便一起移动。
    const QString applicationDirectory = QCoreApplication::applicationDirPath(); // 可执行文件目录
    QDir candidate(applicationDirectory);
    for (int depth = 0; depth < 8; ++depth) { // 最多向上查找 8 层
        if (QFileInfo::exists(candidate.filePath(QStringLiteral("CMakeLists.txt"))) // 检查项目文件
            && QFileInfo::exists(candidate.filePath(QStringLiteral("historydatabase.cpp")))) {
            return candidate.absolutePath(); // 找到项目根目录
        }
        if (!candidate.cdUp()) // 无法继续向上
            break;
    }
    return applicationDirectory; // 回退到可执行文件目录
}

// 将会话聚合查询的一行转换为历史页面需要的字段映射。
QVariantMap recordMap(const QSqlQuery &query, const QString &source)
{
    QVariantMap row;
    row.insert(QStringLiteral("sessionId"), query.value(QStringLiteral("session_id"))); // 会话 ID
    row.insert(QStringLiteral("username"), query.value(QStringLiteral("username"))); // 用户名
    row.insert(QStringLiteral("source"), source); // 数据来源
    row.insert(QStringLiteral("sourceName"),
               source == QStringLiteral("phone") ? QStringLiteral("手机") // 来源显示名
                                                   : QStringLiteral("STM32"));
    row.insert(QStringLiteral("startTime"), query.value(QStringLiteral("start_time"))); // 开始时间
    row.insert(QStringLiteral("endTime"), query.value(QStringLiteral("end_time"))); // 结束时间
    row.insert(QStringLiteral("sampleCount"), query.value(QStringLiteral("sample_count"))); // 采样数
    row.insert(QStringLiteral("startLat"), query.value(QStringLiteral("start_lat"))); // 起始纬度
    row.insert(QStringLiteral("startLon"), query.value(QStringLiteral("start_lon"))); // 起始经度
    row.insert(QStringLiteral("endLat"), query.value(QStringLiteral("end_lat"))); // 结束纬度
    row.insert(QStringLiteral("endLon"), query.value(QStringLiteral("end_lon"))); // 结束经度
    row.insert(QStringLiteral("startBattery"), query.value(QStringLiteral("start_battery"))); // 起始电量
    row.insert(QStringLiteral("endBattery"), query.value(QStringLiteral("end_battery"))); // 结束电量
    row.insert(QStringLiteral("alarmCount"), query.value(QStringLiteral("alarm_count"))); // 告警数
    return row;
}
}

// 初始化历史数据库，创建目录、迁移旧数据、打开库并连接数据变更信号。
HistoryDatabase::HistoryDatabase(Backend *backend,
                                 SensorBackend *sensorBackend,
                                 QObject *parent)
    : QObject(parent),
      m_backend(backend),
      m_sensorBackend(sensorBackend),
      m_connectionName(QStringLiteral("flight_history_%1")
                           .arg(reinterpret_cast<quintptr>(this))) // 基于指针的唯一连接名
{
    const QString dataDirectory =
        QDir(portableProjectDirectory()).filePath(QStringLiteral("database")); // 数据库目录
    if (!QDir().mkpath(dataDirectory)) // 创建目录失败
        qCritical() << "Cannot create history database directory:" << dataDirectory;

    m_databasePath = QDir(dataDirectory).filePath(QStringLiteral("FlightHistory.db")); // 数据库文件路径

    // Preserve installations that previously stored the database in AppData.
    // This one-time migration only runs when the portable database is absent.
    const QString legacyPath = QDir(
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)) // 旧路径
                                   .filePath(QStringLiteral("FlightHistory.db"));
    if (!QFileInfo::exists(m_databasePath) && QFileInfo::exists(legacyPath)) { // 迁移旧数据库
        if (QFile::copy(legacyPath, m_databasePath)) // 复制成功
            qInfo() << "Migrated history database from" << legacyPath;
        else // 复制失败
            qWarning() << "Could not migrate history database from" << legacyPath;
    }

    if (openDatabase() && createTables()) { // 数据库打开成功
        connect(m_backend, &Backend::gpsChanged,
                this, &HistoryDatabase::recordPhoneGps); // GPS 变更 → 记录手机 GPS
        connect(m_sensorBackend, &SensorBackend::sensorChanged,
                this, &HistoryDatabase::recordPhoneSensor); // 传感器变更 → 记录手机传感器
        connect(m_sensorBackend, &SensorBackend::telemetryChanged,
                this, &HistoryDatabase::recordStm32Telemetry); // 遥测变更 → 记录 STM32 遥测
    }
}

// 关闭并清理数据库连接。
HistoryDatabase::~HistoryDatabase()
{
    if (m_database.isOpen()) // 数据库已打开
        m_database.close(); // 关闭连接
    m_database = QSqlDatabase(); // 清空数据库对象
    QSqlDatabase::removeDatabase(m_connectionName); // 移除连接名
}

QString HistoryDatabase::databasePath() const { return m_databasePath; } // 返回数据库文件路径
QString HistoryDatabase::currentSessionId() const { return m_sessionId; } // 返回当前飞行会话 ID

// 打开 SQLite 数据库，设置 WAL 模式和 foreign_keys。
bool HistoryDatabase::openDatabase()
{
    m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), // 创建数据库连接
                                           m_connectionName);
    m_database.setDatabaseName(m_databasePath); // 设置数据库文件路径
    if (!m_database.open()) { // 打开数据库失败
        qCritical() << "Cannot open history database:"
                    << m_database.lastError().text();
        return false;
    }

    QSqlQuery pragma(m_database);
    pragma.exec(QStringLiteral("PRAGMA journal_mode=WAL")); // 使用 WAL 模式提高并发
    pragma.exec(QStringLiteral("PRAGMA synchronous=NORMAL")); // 写入同步策略
    pragma.exec(QStringLiteral("PRAGMA foreign_keys=ON")); // 启用外键约束
    qInfo() << "Flight history database:" << m_databasePath;
    return true;
}

// 创建 phone_data 和 stm32_data 表及索引。
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
    )SQL"); // 手机数据表
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
    )SQL"); // STM32 数据表

    if (!query.exec(phoneSql) || !query.exec(stm32Sql)) { // 创建表失败
        qCritical() << "Cannot create history tables:" << query.lastError().text();
        return false;
    }
    query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_phone_session_time ON phone_data(session_id, recorded_ms)")); // 手机数据索引
    query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_stm32_session_time ON stm32_data(session_id, recorded_ms)")); // STM32 数据索引
    return true;
}

// 开始一次新的飞行会话，生成唯一会话 ID。
bool HistoryDatabase::startSession(const QString &username)
{
    if (!m_database.isOpen() || username.trimmed().isEmpty()) // 数据库未打开或用户名为空
        return false;

    const QString timestamp =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")); // 时间戳部分
    const QString suffix = QUuid::createUuid().toString(QUuid::Id128).left(6); // UUID 后 6 位
    m_sessionId = QStringLiteral("%1_%2").arg(timestamp, suffix); // 组合会话 ID
    m_username = username.trimmed(); // 保存用户名
    emit sessionChanged(); // 通知会话变更
    qInfo() << "History recording session started:" << m_sessionId << m_username;
    return true;
}

// 将手机 GPS 数据插入历史数据库。
void HistoryDatabase::recordPhoneGps()
{
    insertPhoneSnapshot(QStringLiteral("gps")); // 委托通用插入方法
}

// 将手机传感器数据插入历史数据库。
void HistoryDatabase::recordPhoneSensor()
{
    insertPhoneSnapshot(QStringLiteral("sensor")); // 委托通用插入方法
}

// 将当前手机 GPS/传感器快照写入 phone_data 表。
bool HistoryDatabase::insertPhoneSnapshot(const QString &eventType)
{
    if (m_sessionId.isEmpty() || !m_database.isOpen()) // 会话未开始或数据库未打开
        return false;

    const QDateTime now = QDateTime::currentDateTime(); // 当前时间
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(R"SQL(
        INSERT INTO phone_data (
            session_id, username, recorded_at, recorded_ms, event_type,
            latitude, longitude, altitude, speed, battery,
            pitch, roll, yaw, accuracy, gps_time, temperature, humidity
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )SQL"));
    query.addBindValue(m_sessionId); // 会话 ID
    query.addBindValue(m_username); // 用户名
    query.addBindValue(now.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))); // 格式化的记录时间
    query.addBindValue(now.toMSecsSinceEpoch()); // 毫秒时间戳
    query.addBindValue(eventType); // 事件类型
    query.addBindValue(m_backend->latitude()); // 纬度
    query.addBindValue(m_backend->longitude()); // 经度
    query.addBindValue(m_backend->height()); // 高度
    query.addBindValue(m_backend->speed()); // 速度
    query.addBindValue(m_backend->battery()); // 电量
    query.addBindValue(m_backend->pitch()); // 俯仰角
    query.addBindValue(m_backend->roll()); // 横滚角
    query.addBindValue(m_backend->yaw()); // 偏航角
    query.addBindValue(m_backend->accuracy()); // 定位精度
    query.addBindValue(m_backend->gpsTime()); // GPS 时间
    query.addBindValue(m_sensorBackend->temperature()); // 温度
    query.addBindValue(m_sensorBackend->humidity()); // 湿度
    if (!query.exec()) {
        qWarning() << "Cannot insert phone history:" << query.lastError().text();
        return false;
    }
    emit historyChanged(); // 通知历史数据变更
    return true;
}

// 将当前 STM32 遥测数据写入 stm32_data 表。
void HistoryDatabase::recordStm32Telemetry()
{
    if (m_sessionId.isEmpty() || !m_database.isOpen() // 会话未开始或数据库未打开
        || !m_sensorBackend->telemetryValid()) { // 遥测数据无效
        return;
    }

    const QDateTime now = QDateTime::currentDateTime(); // 当前时间
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
    query.addBindValue(m_sessionId); // 会话 ID
    query.addBindValue(m_username); // 用户名
    query.addBindValue(now.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))); // 格式化的记录时间
    query.addBindValue(now.toMSecsSinceEpoch()); // 毫秒时间戳
    query.addBindValue(m_sensorBackend->deviceId()); // 设备 ID
    query.addBindValue(m_sensorBackend->protocolVersion()); // 协议版本
    query.addBindValue(QVariant::fromValue(m_sensorBackend->sequence())); // 帧序号
    query.addBindValue(m_sensorBackend->stm32Latitude()); // 纬度
    query.addBindValue(m_sensorBackend->stm32Longitude()); // 经度
    query.addBindValue(m_sensorBackend->heading()); // 航向角
    query.addBindValue(m_sensorBackend->targetAltitude()); // 目标高度
    query.addBindValue(m_sensorBackend->stm32Speed()); // 飞行速度
    query.addBindValue(m_sensorBackend->rthLatitude()); // 返航点纬度
    query.addBindValue(m_sensorBackend->rthLongitude()); // 返航点经度
    query.addBindValue(m_sensorBackend->rthDistance()); // 返航点距离
    query.addBindValue(m_sensorBackend->dhtTemperature()); // DHT 温度
    query.addBindValue(m_sensorBackend->dhtHumidity()); // DHT 湿度
    query.addBindValue(m_sensorBackend->shtTemperature()); // SHT 温度
    query.addBindValue(m_sensorBackend->shtHumidity()); // SHT 湿度
    query.addBindValue(m_sensorBackend->mcuTemperature()); // MCU 温度
    query.addBindValue(m_sensorBackend->flightMode()); // 飞行模式
    query.addBindValue(m_sensorBackend->alarmCode()); // 告警代码
    query.addBindValue(m_sensorBackend->stm32Battery()); // 电量
    query.addBindValue(QVariant::fromValue(m_sensorBackend->telemetryTimestamp())); // 设备时间戳
    if (!query.exec()) {
        qWarning() << "Cannot insert STM32 history:" << query.lastError().text();
        return;
    }
    emit historyChanged(); // 通知历史数据变更
}

// 返回所有飞行会话的聚合摘要列表（手机 + STM32 数据源），按时间倒序。
QVariantList HistoryDatabase::getHistoryRecords() const
{
    QVariantList result;
    if (!m_database.isOpen()) // 数据库未打开
        return result;

    const struct SourceQuery { QString source; QString sql; } queries[] = { // 定义查询源
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
        )SQL") }, // 手机数据聚合查询
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
        )SQL") } // STM32 数据聚合查询
    };

    for (const SourceQuery &sourceQuery : queries) { // 遍历手机和 STM32 两个数据源
        QSqlQuery query(m_database);
        if (!query.exec(sourceQuery.sql)) { // 执行聚合查询
            qWarning() << "Cannot query history records:" << query.lastError().text();
            continue;
        }
        while (query.next()) { // 遍历查询结果
            QVariantMap row = recordMap(query, sourceQuery.source); // 映射为字段
            const QDateTime start = QDateTime::fromString(
                row.value(QStringLiteral("startTime")).toString(),
                QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")); // 解析开始时间
            const QDateTime end = QDateTime::fromString(
                row.value(QStringLiteral("endTime")).toString(),
                QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")); // 解析结束时间
            row.insert(QStringLiteral("durationSeconds"), start.secsTo(end)); // 计算飞行时长
            row.insert(QStringLiteral("startPos"),
                       formatPosition(row.value(QStringLiteral("startLat")).toDouble(),
                                      row.value(QStringLiteral("startLon")).toDouble())); // 格式化起始位置
            row.insert(QStringLiteral("endPos"),
                       formatPosition(row.value(QStringLiteral("endLat")).toDouble(),
                                      row.value(QStringLiteral("endLon")).toDouble())); // 格式化结束位置
            result.append(row); // 添加到结果列表
        }
    }
    // 将手机和 STM32 会话统一按开始时间倒序排列。
    std::sort(result.begin(), result.end(), [](const QVariant &left, const QVariant &right) {
        return left.toMap().value(QStringLiteral("startTime")).toString()
            > right.toMap().value(QStringLiteral("startTime")).toString();
    });
    return result;
}

// 返回指定会话的详细数据列表，按记录时间排序。
QVariantList HistoryDatabase::getSessionData(const QString &sessionId,
                                             const QString &source) const
{
    QVariantList result;
    if (!m_database.isOpen() || sessionId.isEmpty()) // 数据库未打开或会话 ID 为空
        return result;

    QSqlQuery query(m_database);
    if (source == QStringLiteral("stm32")) { // STM32 数据查询
        query.prepare(QStringLiteral(R"SQL(
            SELECT recorded_at, recorded_ms, latitude, longitude,
                   target_altitude AS altitude, speed,
                   sht_temperature AS temperature, sht_humidity AS humidity,
                   battery, heading, flight_mode, alarm_code,
                   rth_distance, dht_temperature, dht_humidity,
                   sht_temperature, sht_humidity, mcu_temperature
            FROM stm32_data WHERE session_id=? ORDER BY recorded_ms, id
        )SQL"));
    } else { // 手机数据查询
        query.prepare(QStringLiteral(R"SQL(
            SELECT recorded_at, recorded_ms, latitude, longitude,
                   altitude, speed, temperature, humidity, battery,
                   yaw AS heading, 0 AS flight_mode, 0 AS alarm_code,
                   pitch, roll, accuracy
            FROM phone_data WHERE session_id=? ORDER BY recorded_ms, id
        )SQL"));
    }
    query.addBindValue(sessionId); // 绑定会话 ID
    if (!query.exec()) {
        qWarning() << "Cannot query session data:" << query.lastError().text();
        return result;
    }
    while (query.next()) { // 遍历查询结果
        QVariantMap row;
        const auto record = query.record(); // 获取记录元数据
        for (int i = 0; i < record.count(); ++i) // 遍历所有字段
            row.insert(record.fieldName(i), query.value(i)); // 字段名 -> 值
        result.append(row); // 添加到结果列表
    }
    return result;
}

// 返回指定会话中所有异常采样点，每个点包含位置、时间和异常描述。
QVariantList HistoryDatabase::getAbnormalData(const QString &sessionId,
                                              const QString &source) const
{
    QVariantList result;
    const QVariantList samples = getSessionData(sessionId, source); // 获取所有采样点
    for (const QVariant &sample : samples) { // 遍历每个采样点
        const QVariantMap data = sample.toMap();
        QStringList events; // 异常事件列表
        const double battery = data.value(QStringLiteral("battery")).toDouble(); // 电量
        const double speed = data.value(QStringLiteral("speed")).toDouble(); // 速度
        const double altitude = data.value(QStringLiteral("altitude")).toDouble(); // 高度
        const double temperature = data.value(QStringLiteral("temperature")).toDouble(); // 温度
        const double humidity = data.value(QStringLiteral("humidity")).toDouble(); // 湿度

        if (source == QStringLiteral("stm32")) { // STM32 异常检测规则
            const int alarmCode = data.value(QStringLiteral("alarm_code")).toInt(); // 告警代码
            const double dhtTemperature = data.value(QStringLiteral("dht_temperature")).toDouble(); // DHT 温度
            const double dhtHumidity = data.value(QStringLiteral("dht_humidity")).toDouble(); // DHT 湿度
            const double mcuTemperature = data.value(QStringLiteral("mcu_temperature")).toDouble(); // MCU 温度
            const double rthDistance = data.value(QStringLiteral("rth_distance")).toDouble(); // 返航距离
            const double latitude = data.value(QStringLiteral("latitude")).toDouble(); // 纬度
            const double longitude = data.value(QStringLiteral("longitude")).toDouble(); // 经度

            if (alarmCode != 0) // 固件告警
                events.append(QStringLiteral("固件告警代码 %1").arg(alarmCode));
            if (battery < kStm32BatteryLow) // 电量过低
                events.append(QStringLiteral("电量 %1%，低于 20%")
                                  .arg(battery, 0, 'f', 0));
            if (speed > kStm32SpeedMax) // 速度超限
                events.append(QStringLiteral("速度 %1 m/s，超过 50 m/s")
                                  .arg(speed, 0, 'f', 1));
            if (altitude < kStm32AltitudeMin || altitude > kStm32AltitudeMax) // 高度越界
                events.append(QStringLiteral("目标高度 %1 m，允许范围 0~500 m")
                                  .arg(altitude, 0, 'f', 1));
            if (rthDistance > kStm32RthDistanceMax) // 返航距离过远
                events.append(QStringLiteral("返航距离 %1 m，超过 2000 m")
                                  .arg(rthDistance, 0, 'f', 1));
            if (dhtTemperature < kStm32TemperatureMin // DHT 温度异常
                || dhtTemperature > kStm32TemperatureMax) {
                events.append(QStringLiteral("DHT 温度 %1 ℃，允许范围 -20~50 ℃")
                                  .arg(dhtTemperature, 0, 'f', 1));
            }
            if (temperature < kStm32TemperatureMin // SHT 温度异常
                || temperature > kStm32TemperatureMax) {
                events.append(QStringLiteral("SHT 温度 %1 ℃，允许范围 -20~50 ℃")
                                  .arg(temperature, 0, 'f', 1));
            }
            if (dhtHumidity < kStm32HumidityMin // DHT 湿度异常
                || dhtHumidity > kStm32HumidityMax) {
                events.append(QStringLiteral("DHT 湿度 %1%，允许范围 10~90%")
                                  .arg(dhtHumidity, 0, 'f', 1));
            }
            if (humidity < kStm32HumidityMin || humidity > kStm32HumidityMax) { // SHT 湿度异常
                events.append(QStringLiteral("SHT 湿度 %1%，允许范围 10~90%")
                                  .arg(humidity, 0, 'f', 1));
            }
            if (mcuTemperature < kStm32McuTemperatureMin // MCU 温度异常
                || mcuTemperature > kStm32McuTemperatureMax) {
                events.append(QStringLiteral("MCU 温度 %1 ℃，允许范围 -20~75 ℃")
                                  .arg(mcuTemperature, 0, 'f', 1));
            }
            if (qAbs(latitude) < 0.000001 && qAbs(longitude) < 0.000001) // 无效坐标
                events.append(QStringLiteral("STM32 定位坐标无效 (0, 0)"));
        } else { // 手机异常检测规则
            const double pitch = data.value(QStringLiteral("pitch")).toDouble(); // 俯仰角
            const double roll = data.value(QStringLiteral("roll")).toDouble(); // 横滚角
            const double accuracy = data.value(QStringLiteral("accuracy")).toDouble(); // 定位精度

            if (temperature < kPhoneTemperatureMin // 温度异常
                || temperature > kPhoneTemperatureMax) {
                events.append(QStringLiteral("环境温度 %1 ℃，允许范围 -20~40 ℃")
                                  .arg(temperature, 0, 'f', 1));
            }
            if (humidity > kPhoneHumidityMax) // 湿度过高
                events.append(QStringLiteral("环境湿度 %1%，超过 90%")
                                  .arg(humidity, 0, 'f', 1));
            if (battery > 0 && battery < kPhoneBatteryLow) // 电量过低
                events.append(QStringLiteral("电量 %1%，低于 20%")
                                  .arg(battery, 0, 'f', 0));
            if (speed > kPhoneSpeedMax) // 速度超限
                events.append(QStringLiteral("速度 %1 m/s，超过 50 m/s")
                                  .arg(speed, 0, 'f', 1));
            if (altitude < kPhoneAltitudeMin || altitude > kPhoneAltitudeMax) // 高度越界
                events.append(QStringLiteral("高度 %1 m，允许范围 -100~500 m")
                                  .arg(altitude, 0, 'f', 1));
            if (qAbs(pitch) > kPhoneAttitudeMax) // 俯仰角过大
                events.append(QStringLiteral("俯仰角 %1°，超过 ±50°")
                                  .arg(pitch, 0, 'f', 1));
            if (qAbs(roll) > kPhoneAttitudeMax) // 横滚角过大
                events.append(QStringLiteral("横滚角 %1°，超过 ±50°")
                                  .arg(roll, 0, 'f', 1));
            if (accuracy > kPhoneAccuracyMax) // 精度不足
                events.append(QStringLiteral("GPS 精度 %1 m，误差超过 50 m")
                                  .arg(accuracy, 0, 'f', 1));
        }
        if (!events.isEmpty()) { // 有异常事件
            QVariantMap abnormal;
            abnormal.insert(QStringLiteral("latitude"), data.value(QStringLiteral("latitude"))); // 纬度
            abnormal.insert(QStringLiteral("longitude"), data.value(QStringLiteral("longitude"))); // 经度
            abnormal.insert(QStringLiteral("time"), data.value(QStringLiteral("recorded_at"))); // 时间
            abnormal.insert(QStringLiteral("event"), events.join(QStringLiteral("；"))); // 异常描述
            result.append(abnormal); // 添加到结果列表
        }
    }
    return result;
}

// 格式化经纬度为 6 位小数文本，无效坐标返回 "--"。
QString HistoryDatabase::formatPosition(double latitude, double longitude)
{
    if (qAbs(latitude) < 0.000001 && qAbs(longitude) < 0.000001) // 无效坐标
        return QStringLiteral("--");
    return QStringLiteral("%1, %2")
        .arg(latitude, 0, 'f', 6) // 纬度 6 位小数
        .arg(longitude, 0, 'f', 6); // 经度 6 位小数
}