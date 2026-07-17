# DroneGroundStation 无人机地面控制站

> 本仓库同时包含桌面地面站与云端监控网页。Qt/C++ 主工程位于仓库根目录，云端工程位于 [`cloud-dashboard/`](cloud-dashboard/)。

## 文档导航

- [`docs/操作使用说明.md`](docs/操作使用说明.md)：登录、手机接入、STM32 接入、飞控与历史数据操作。
- [`docs/技术流程文档.md`](docs/技术流程文档.md)：桌面端类职责、协议、数据库和函数调用链。
- [`docs/云端部署与跨设备访问.md`](docs/云端部署与跨设备访问.md)：EMQX、HTTPS、D1、网页部署、迁移和故障排查。
- [`cloud-dashboard/README.md`](cloud-dashboard/README.md)：云端网页工程开发、接口、数据表和部署说明。
- [`config/cloud.example.json`](config/cloud.example.json)：不含真实密钥的桌面端云配置模板。

## 项目简介

DroneGroundStation 是一个基于 Qt 6、Qt Quick/QML 和 C++17 开发的无人机地面控制站。项目把手机浏览器传感器采集、STM32 AP 热点遥测、人工飞行控制、实时地图、曲线分析、异常告警和 SQLite 历史数据统一到一个桌面程序中。

项目包含两条彼此独立的数据链路：

1. 手机作为采集端，通过网页读取 GPS、姿态、海拔、电量和温湿度，再以 HTTP/JSON 上传到 Qt。
2. STM32 作为 AP 热点和 TCP 服务端，Qt 作为客户端自动发现热点网关，接收 JSON 遥测并下发飞控命令。

为避免两个设备同时修改实时界面和数据库状态，程序实现了单设备互斥：手机处于活动状态时暂停 STM32 探测；手机 8 秒没有有效请求后，自动恢复 STM32 连接。

主要功能：

- 手机 GPS、姿态、海拔、电量、温湿度实时采集。
- GPS 静止防漂移、定位精度过滤和异常跳点过滤。
- STM32 TCP 遥测接收、粘包拆包、字段校验、断线重连和看门狗。
- 自由飞行、起飞、悬停、盘旋、改变航向、返航和降落控制。
- 命令序号、ACK 匹配和 3 秒命令确认超时。
- 手机与 STM32 独立地图、轨迹、曲线、参数和告警页面。
- SQLite 按登录会话分别保存 `phone_data` 与 `stm32_data`。
- 历史轨迹、坐标曲线、高度曲线、温湿度曲线和异常点查看。
- 自动适配当前 Wi-Fi/热点网段，不在业务代码中写死设备 IP。
- 数据库跟随项目或部署目录，迁移后首次运行可自动建库建表。

## 技术栈

| 分类 | 技术 | 用途 |
|---|---|---|
| 语言 | C++17 | 网络、协议、状态管理和数据库 |
| 界面 | Qt Quick / QML | 页面布局、交互、地图和曲线 |
| 控件 | Qt Quick Controls 2 Basic | 可定制跨平台控件样式 |
| 网络 | Qt Network | 手机 HTTP 服务与 STM32 TCP 客户端 |
| 数据 | Qt SQL / SQLite | 历史数据持久化与查询 |
| 地图 | Qt Location / Qt Positioning | OSM 底图、坐标和实时航线 |
| 构建 | CMake 3.16+ | Qt 工程配置与资源打包 |
| 工具链 | MSVC 2022 64 位 | Windows 编译与调试 |
| 手机端 | HTML / CSS / JavaScript | 浏览器传感器采集与 JSON 上传 |

## 总体架构

```text
                         DroneGroundStation
┌─────────────────────────────────────────────────────────────────────┐
│ QML 页面层                                                          │
│ LoginPage ─ MainPage ─ Stm32Page ─ HistoryPage                     │
│                 地图 / 曲线 / 参数 / 告警 / 飞控                    │
├─────────────────────────────────────────────────────────────────────┤
│ C++ 后端层                                                          │
│ Backend              SensorBackend              HistoryDatabase     │
│ 手机 HTTP/GPS        STM32 TCP/命令             SQLite/会话/异常    │
├─────────────────────────────────────────────────────────────────────┤
│ 通信与存储层                                                        │
│ HTTP :8080           TCP :8080                  FlightHistory.db    │
└─────────────────────────────────────────────────────────────────────┘
       ▲                       ▲                         ▲
       │ HTTP/JSON             │ JSON + CRLF             │ SQL
       │                       │                         │
   手机浏览器             STM32 AP/TCP 服务端        database/
```

设计原则：

- C++ 处理不可信输入、连接状态和持久化，QML 只负责展示与交互。
- 手机和 STM32 使用不同属性、页面、轨迹和数据表，不能混用。
- 所有网络操作均由 Qt 事件循环异步驱动，不在界面线程阻塞等待。
- TCP 使用帧边界和缓存上限处理粘包、拆包与异常输入。
- 控制命令必须收到匹配的 ACK 才认为确认成功。
- 实际数据库不提交到 Git，防止泄露用户名和真实位置。

## 源文件依赖与构建关系

```text
CMakeLists.txt
  ├─ appDroneGroundStation
  │   ├─ main.cpp
  │   ├─ backend.cpp/.h
  │   ├─ sensorbackend.cpp/.h
  │   └─ historydatabase.cpp/.h
  ├─ DroneGroundStation QML module
  │   ├─ Main.qml / LoginPage.qml / MainPage.qml
  │   ├─ Stm32Page.qml / HistoryPage.qml
  │   └─ 地图、曲线、仪表和表格组件
  └─ gps_web_page resource
      └─ index.html
```

核心依赖：

1. `Backend` 与 `SensorBackend` 可独立创建。
2. `HistoryDatabase` 持有两个后端的非拥有指针，并订阅其更新信号。
3. `main.cpp` 把三个 C++ 对象注册为 QML 上下文属性。
4. `Main.qml` 使用 `StackView` 管理登录、主页、STM32 和历史页面。
5. `index.html` 通过 Qt Resource System 内嵌到可执行程序。

`CMakeLists.txt` 使用 `NO_CACHEGEN` 禁用 QML AOT 缓存生成。这是为了规避 Qt 6.11 MSVC Debug 下遥测属性高频更新可能触发的 QML 编译产物崩溃；本项目界面规模较小，解释执行不会影响正常使用。

## 程序执行流程（函数调用链）

### 1. 启动阶段

```text
main(argc, argv)
  ├─ QQuickStyle::setStyle("Basic")
  ├─ QGuiApplication app
  ├─ Backend backend
  │   ├─ 创建 QTcpServer
  │   ├─ 监听 IPv4 :8080
  │   ├─ preferredHotspotAddress()
  │   └─ 创建手机 8 秒活动租约计时器
  ├─ SensorBackend sensorBackend
  │   ├─ 创建 QTcpSocket
  │   ├─ 创建重连/连接超时/遥测看门狗计时器
  │   └─ 异步开始 STM32 自动发现
  ├─ HistoryDatabase historyDatabase
  │   ├─ 定位 database/FlightHistory.db
  │   ├─ openDatabase()
  │   ├─ PRAGMA WAL/NORMAL/foreign_keys
  │   └─ createTables()
  ├─ 连接手机活动状态与 STM32 互斥逻辑
  ├─ QQmlEngine::setObjectOwnership(..., CppOwnership)
  ├─ rootContext()->setContextProperty(...)
  ├─ engine.loadFromModule("DroneGroundStation", "Main")
  └─ app.exec()
```

使用 `CppOwnership` 的原因是三个后端对象位于 `main()` 栈上，其生命周期覆盖整个事件循环。QML 页面在 `StackView` 中反复创建和销毁时，不得接管或回收这些对象。

### 2. 登录和页面导航

```text
LoginPage 登录成功
  ├─ historyDatabase.startSession(username)
  └─ Main.qml: stackView.push(mainPage)

MainPage 点击“历史数据”
  └─ stackView.push(historyPage)

MainPage 点击“STM32 控制”
  └─ stackView.push(stm32Page)

子页面点击返回
  └─ stackView.pop()
```

当前默认演示账号是 `admin / 123456`。注册界面只演示前端交互，不会创建永久用户记录，不适合作为正式鉴权系统。

### 3. 手机数据运行阶段

```text
手机 GET /
  → Backend::newConnection()
  → Backend::readData()
  → 从 qrc:/index.html 返回网页
  → markPhoneActivity()

手机 POST /gps
  → readData()
  → processGpsPayload()
  → JSON/坐标/精度/跳点校验
  → 更新 Backend 属性
  → emit gpsChanged()
  ├─ MainPage/MapView/FlightCurve 更新
  └─ HistoryDatabase::recordPhoneGps()

手机 POST /sensor
  → processSensorPayload()
  → emit sensorDataReceived(temp, humidity)
  → SensorBackend::updateSensorData()
  → emit sensorChanged()
  ├─ MainPage 更新温湿度
  └─ HistoryDatabase::recordPhoneSensor()
```

### 4. STM32 遥测运行阶段

```text
SensorBackend::attemptConnection()
  → discoverApServerAddress()
  → QTcpSocket::connectToHost(gateway, 8080)
  → socketConnected()
  → readyRead
  → readTelemetryData()
  → 按 \n 提取完整帧并去除 \r
  → processTelemetryFrame()
  → 校验全部必填字段
  → 更新 SensorBackend 属性
  → emit telemetryChanged()
  ├─ Stm32Page/Stm32MapView/Stm32FlightCurve 更新
  └─ HistoryDatabase::recordStm32Telemetry()
```

### 5. 飞控命令运行阶段

```text
QML 底部按钮
  → sensorBackend.sendFlightCommand(cmd, heading, altitude, speed)
  → 检查连接、待确认状态、命令名和参数范围
  → 生成 command_seq
  → 生成 flight_command JSON + CRLF
  → QTcpSocket::write()
  → commandPending = true
  → 等待 STM32 command_ack

STM32 ACK 到达
  → readTelemetryData()
  → processTelemetryFrame()
  → type 为 command_ack/ack
  → command_seq 与待确认序号匹配
  → 读取 ok/message
  → 清除 commandPending
  → 更新人工/自主模式和界面状态
```

### 6. 退出阶段

关闭窗口后，Qt 退出事件循环并按栈逆序销毁对象。`HistoryDatabase` 析构时关闭并移除专用 SQL 连接；socket、server 和 timer 由 QObject 父子关系自动释放。程序退出后再复制数据库，才能确保 WAL 中的数据已正确落盘。

更细的逐帧流程见 [技术执行流程文档](docs/技术流程文档.md)。

## 手机 HTTP 协议

### 网络角色

- Qt：HTTP 服务端，监听 IPv4 `8080`。
- 手机浏览器：HTTP 客户端。
- 电脑和手机：必须处于同一 Wi-Fi 或热点网段。
- 页面顶部 URL：由活动网卡动态选择，不应手工固定旧 IP。

### 路由表

| 方法 | 路径 | 请求体 | 功能 |
|---|---|---|---|
| GET | `/` | 无 | 返回手机采集页面 |
| GET | `/index.html` | 无 | 返回手机采集页面 |
| GET | `/health` | 无 | 返回服务状态和当前 URL |
| POST | `/gps` | JSON | 上传 GPS、姿态、电量 |
| POST | `/sensor` | JSON | 上传温湿度 |
| OPTIONS | 任意 | 无 | 处理 CORS 预检 |

### GPS 请求示例

```json
{
  "latitude": 40.113789,
  "longitude": 116.384795,
  "height": 120.0,
  "battery": 82,
  "pitch": 1.2,
  "roll": -0.8,
  "yaw": 135.0,
  "accuracy": 8.0
}
```

处理规则：

- 纬度必须在 `[-90, 90]`，经度必须在 `[-180, 180]`。
- `(0, 0)` 不作为有效飞行位置。
- 定位精度过差的实时输入会被拒绝。
- 静止死区会根据精度在约 3～12 m 范围内调整。
- 跳点距离超过 `max(300 m, accuracy × 4)` 且推算速度超过 120 m/s 时拒绝该点。
- 速度由连续有效位置与本地时间差推算，不直接信任浏览器任意值。

### 温湿度请求示例

```json
{
  "temperature": 24.6,
  "humidity": 53.2
}
```

成功响应为 `{"ok":true}`；格式错误、字段缺失或数值非法返回 HTTP 400。响应统一携带内容长度和 CORS 头。

## STM32 TCP/JSON 协议

### 网络角色与自动发现

- STM32/ESP8266：AP 热点和 TCP 服务端。
- Qt：TCP 客户端。
- 默认端口：`8080`。
- 帧编码：UTF-8 JSON。
- 帧结束符：`\r\n`。

自动模式读取活动 WLAN 的 IPv4 和掩码，将所在子网的第一个主机地址作为 AP 网关候选；`192.168.4.x` 网段会优先识别。程序因此能适应不同热点网段，而不是把 `192.168.4.1` 写死为唯一地址。

### 遥测帧

```json
{
  "id":"UAV_01",
  "proto":1,
  "seq":490,
  "lat":40.113789,
  "lng":116.384795,
  "heading":135.0,
  "target_alt":120,
  "speed":8.5,
  "rth_lat":40.113000,
  "rth_lng":116.384000,
  "rth_distance":95.2,
  "dht_temp":25,
  "dht_hum":52,
  "sht_temp":24.8,
  "sht_hum":51.6,
  "mcu_temp":42.1,
  "mode":0,
  "alarm":0,
  "battery":78,
  "ts":0
}
```

所有字段均为必填。数值字段必须是 JSON number，不能写成字符串。`ts` 在无 RTC 时允许为 0。接收缓存最大 64 KiB；程序可以一次处理多帧，也可以等待被拆开的后半帧到达。

连接参数：

| 项目 | 当前值 | 作用 |
|---|---:|---|
| TCP 端口 | 8080 | 遥测与命令共用连接 |
| 失败重试 | 约 2 秒 | 避免高频空连接 |
| 建连超时 | 3.5 秒 | 清理无响应连接 |
| 遥测看门狗 | 3.5 秒 | 已连接但无新遥测时重连 |
| 接收缓存 | 64 KiB | 防止无帧边界数据无限增长 |
| 命令 ACK 超时 | 3 秒 | 解除长期等待状态 |

### 飞控命令

支持的 `cmd`：

| 页面按钮 | `cmd` | 控制模式 | 说明 |
|---|---|---|---|
| 自由飞行 | `free_flight` | autonomous | 执行固件预设航线 |
| 起飞 | `takeoff` | manual | 按目标高度/速度起飞 |
| 悬停 | `hover` | manual | 保持当前位置或固件定义状态 |
| 盘旋 | `loiter` | manual | 执行固件盘旋逻辑 |
| 改变航向 | `set_heading` | manual | 应用页面航向参数 |
| 返航 | `rth` | manual | 返回遥测中的返航点 |
| 降落 | `land` | manual | 执行降落 |

命令示例：

```json
{
  "type":"flight_command",
  "proto":1,
  "id":"UAV_01",
  "command_seq":123456,
  "cmd":"takeoff",
  "control":"manual",
  "use_preset_route":false,
  "heading":135,
  "target_alt":120,
  "speed":8,
  "ts":1780000000000
}
```

参数范围：航向 `0 ≤ heading < 360`，目标高度 `0～500 m`，速度 `0～50 m/s`。`free_flight` 自动设置 `control=autonomous` 和 `use_preset_route=true`，其余命令进入人工控制。

### 命令确认 ACK

```json
{
  "type":"command_ack",
  "command_seq":123456,
  "ok":true,
  "message":"accepted"
}
```

`type` 也允许为 `ack`。只有 `command_seq` 与当前待确认命令完全相同，ACK 才有效。旧 ACK、缺少序号的 ACK 或非法 JSON 均不会改变当前控制状态。

## 单设备互斥机制

手机 HTTP 使用短连接，不能以 socket 是否存在判断在线状态，因此程序采用 8 秒活动租约：

1. 手机访问页面、健康接口或成功上传数据时调用 `markPhoneActivity()`。
2. 第一次进入活动状态时发出 `phoneConnectionChanged()`。
3. `SensorBackend::setPhoneDeviceActive(true)` 停止 STM32 重试、连接超时和遥测看门狗，并中断现有 STM32 socket。
4. 每个有效手机请求重新开始 8 秒计时。
5. 8 秒无有效请求后发出离线状态。
6. `setPhoneDeviceActive(false)` 恢复 STM32 自动发现和重连。

这保证同一时刻最多一个实时设备来源，同时不会删除两个来源已经写入的历史记录。

## SQLite 数据库设计

### 数据库位置

开发运行时，程序从构建目录逐级向上查找同时包含 `CMakeLists.txt` 和 `historydatabase.cpp` 的项目根，最终使用：

```text
<项目根>/database/FlightHistory.db
```

独立部署时使用：

```text
<EXE目录>/database/FlightHistory.db
```

首次运行会创建目录、数据库、数据表和索引。旧版应用数据目录中的数据库在目标不存在时会执行一次迁移。真实 `.db`、`.db-wal` 和 `.db-shm` 默认被 Git 忽略。

### 会话标识

登录成功后调用 `startSession(username)`，生成“时间戳 + UUID 后缀”的唯一 `session_id`。同一次登录期间的手机和 STM32 数据使用相同会话标识，但写入不同表。

### `phone_data` 表

| 字段组 | 字段 |
|---|---|
| 标识 | `id`, `session_id`, `username` |
| 时间 | `recorded_at`, `recorded_ms`, `gps_time` |
| 事件 | `event_type`（GPS 或 sensor） |
| 位置 | `latitude`, `longitude`, `altitude`, `accuracy` |
| 飞行 | `speed`, `battery`, `pitch`, `roll`, `yaw` |
| 环境 | `temperature`, `humidity` |

### `stm32_data` 表

| 字段组 | 字段 |
|---|---|
| 标识 | `id`, `session_id`, `username`, `device_id`, `proto`, `sequence` |
| 时间 | `recorded_at`, `recorded_ms`, `device_timestamp` |
| 位置 | `latitude`, `longitude`, `heading` |
| 飞行 | `target_altitude`, `speed`, `flight_mode`, `battery` |
| 返航 | `rth_latitude`, `rth_longitude`, `rth_distance` |
| 环境 | `dht_temperature`, `dht_humidity`, `sht_temperature`, `sht_humidity`, `mcu_temperature` |
| 告警 | `alarm_code` |

两个表均建立会话与时间索引，历史页面可以先聚合会话摘要，再按 `session_id + source` 读取完整样本。

## 异常阈值

### 手机数据

| 指标 | 异常条件 |
|---|---|
| 温度 | `< -20 ℃` 或 `> 40 ℃` |
| 湿度 | `> 90 %` |
| 电量 | 有效值 `> 0` 且 `< 20 %` |
| 速度 | `> 50 m/s` |
| 高度 | `< -100 m` 或 `> 500 m` |
| 俯仰角 | 绝对值 `> 50°` |
| 横滚角 | 绝对值 `> 50°` |
| GPS 精度 | `> 50 m` |

### STM32 数据

| 指标 | 异常条件 |
|---|---|
| DHT/SHT 温度 | `< -20 ℃` 或 `> 50 ℃` |
| DHT/SHT 湿度 | `< 10 %` 或 `> 90 %` |
| MCU 温度 | `< -20 ℃` 或 `> 75 ℃` |
| 电量 | `< 20 %` |
| 速度 | `> 50 m/s` |
| 目标高度 | `< 0 m` 或 `> 500 m` |
| 返航距离 | `> 2000 m` |
| 固件告警 | `alarm != 0` |

异常查询不会修改或删除原始数据，只生成异常时间、位置和原因供历史告警页面显示。

实时手机或 STM32 数据越过上述阈值时，对应页面边缘会以红色呼吸闪动持续提醒；连接等待、设备离线等普通状态只显示文字而不会误触发闪动。历史会话存在异常点时，历史异常面板也会闪动。

## QML 页面与组件

| 文件 | 职责 |
|---|---|
| `Main.qml` | `ApplicationWindow`、`StackView` 和页面导航 |
| `LoginPage.qml` | 演示登录与注册界面 |
| `MainPage.qml` | 手机实时主页、历史与 STM32 入口 |
| `Dashboard.qml` | 手机核心状态卡片 |
| `TelemetryPanel.qml` | 手机遥测列表 |
| `MapView.qml` | 手机位置、轨迹、拖动、缩放和跟随 |
| `FlightCurve.qml` | 手机实时数据曲线 |
| `AttitudeIndicator.qml` | 俯仰、横滚和偏航显示 |
| `Stm32Page.qml` | STM32 遥测、告警、地图、曲线和飞控 |
| `Stm32MapView.qml` | STM32 实时航线 |
| `Stm32FlightCurve.qml` | STM32 数据曲线 |
| `HistoryPage.qml` | 历史会话选择和详情容器 |
| `HistoryTable.qml` | 会话摘要表 |
| `HistoryFlightPath.qml` | 历史航迹回放展示 |
| `CoordinateChart.qml` | 经纬度变化曲线 |
| `AltitudeChart.qml` | 高度变化曲线 |
| `TemperatureHumidityChart.qml` | 温湿度曲线 |
| `StatusPanel.qml` | 历史状态与告警摘要 |

## C++ 类与函数职责

### `Backend`

| 函数 | 功能 |
|---|---|
| `Backend()` | 初始化 HTTP 服务、访问 URL 和手机租约 |
| `preferredHotspotAddress()` | 从活动网卡选择手机可访问 IPv4 |
| `newConnection()` | 接收新 HTTP TCP 连接并绑定读取事件 |
| `readData()` | 组装 HTTP 请求、解析头和路由 |
| `sendResponse()` | 发送状态码、CORS、长度和响应体 |
| `processGpsPayload()` | 校验 GPS/姿态并执行防漂移过滤 |
| `processSensorPayload()` | 校验温湿度并发出更新信号 |
| `markPhoneActivity()` | 建立或刷新 8 秒活动租约 |

### `SensorBackend`

| 函数 | 功能 |
|---|---|
| `discoverApServerAddress()` | 根据 WLAN IPv4/掩码推导 AP 网关 |
| `attemptConnection()` | 自动选择目标并异步连接 TCP 服务端 |
| `socketConnected()` | 更新对端、启动看门狗、清理连接超时 |
| `socketDisconnected()` | 清理缓存与待确认命令并安排重连 |
| `socketError()` | 记录 socket 错误并恢复连接流程 |
| `scheduleReconnect()` | 在未被手机占用时延时重试 |
| `readTelemetryData()` | 读取 socket 并完成粘包拆包 |
| `processTelemetryFrame()` | 识别遥测/ACK、校验并更新属性 |
| `finiteNumber()` | 检查 JSON 数值字段存在且为有限值 |
| `sendFlightCommand()` | 校验控制参数、生成序号并发送命令 |
| `reconnectTelemetry()` | 立即中断并重新连接 |
| `connectToTelemetry()` | 设置手动 IPv4/端口目标 |
| `useAutomaticTelemetryTarget()` | 恢复网关自动发现 |
| `setPhoneDeviceActive()` | 实现手机与 STM32 互斥 |

### `HistoryDatabase`

| 函数 | 功能 |
|---|---|
| `openDatabase()` | 打开 SQLite 并配置 WAL/NORMAL/foreign_keys |
| `createTables()` | 创建两张数据表和查询索引 |
| `startSession()` | 为登录用户生成新会话 |
| `recordPhoneGps()` | GPS 更新时写入手机快照 |
| `recordPhoneSensor()` | 温湿度更新时写入手机快照 |
| `insertPhoneSnapshot()` | 绑定当前手机全部状态并执行 INSERT |
| `recordStm32Telemetry()` | 写入一帧有效 STM32 遥测 |
| `getHistoryRecords()` | 聚合手机和 STM32 会话摘要 |
| `getSessionData()` | 返回指定来源的有序样本 |
| `getAbnormalData()` | 按阈值提取异常点与原因 |
| `formatPosition()` | 格式化历史页面位置文本 |

## 环境要求

| 项目 | 建议版本 |
|---|---|
| Windows | 10/11 64 位 |
| Qt | 6.11.1，MSVC2022 64 位套件 |
| Visual Studio Build Tools | 2022 |
| CMake | 3.16 或更高 |
| Qt Creator | 与 Qt 6.11.1 配套版本 |

必须安装 Qt 模块：Quick、QuickControls2、Network、Sql、Positioning、Location。运行时还需要 SQLite 驱动和 Qt Location 的地理服务插件。

## 编译与运行

### Qt Creator

1. 克隆仓库。
2. 使用 Qt Creator 打开根目录 `CMakeLists.txt`。
3. 选择 `Desktop Qt 6.11.1 MSVC2022 64bit` Kit。
4. 点击 Configure Project。
5. 执行 Build Project。
6. 执行 Run。
7. 使用 `admin / 123456` 登录。

### 命令行示例

```powershell
cmake -S . -B build-release -G Ninja `
  -DCMAKE_PREFIX_PATH="E:/Qt/6.11.1/msvc2022_64" `
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
```

Qt 路径应按目标设备修改。不要复制旧电脑的 `build` 目录，因为其中包含绝对路径、Kit 和生成器缓存。

## 操作说明

### 手机模式

1. 让电脑和手机连接同一 Wi-Fi/热点。
2. 启动程序并登录。
3. 在手机浏览器打开主页顶部实时显示的 URL。
4. 允许定位、方向/动作传感器权限。
5. 保持采集页面在前台，观察 Qt 数据和轨迹更新。

### STM32 模式

1. 关闭手机采集页面，等待 8 秒租约过期。
2. 让电脑连接 STM32 AP 热点。
3. 确认 STM32 在 AP 网关 `8080` 监听 TCP。
4. 进入“STM32 控制”页面，等待显示“已连接”及有效序号。
5. 自由飞行只执行固件预设路线；点击其他按钮会进入人工控制。
6. 下发命令后检查 ACK 状态和后续遥测变化。

### 历史数据

1. 每次登录会创建新会话。
2. 实时数据到达后自动写入，无需手工保存。
3. 点击主页顶部“历史数据”。
4. 选择手机或 STM32 会话。
5. 查看轨迹、曲线、摘要和异常点。

## 地图说明

- 地图使用 Qt Location OSM 插件。
- 支持鼠标拖动、滚轮缩放、触控手势和跟随模式。
- 手机和 STM32 使用各自轨迹数组，不共享定位状态。
- STM32 AP 通常没有互联网，未缓存区域可能无法下载 OSM 瓦片。
- 没有底图不代表遥测断开；应同时检查序号、连接状态和轨迹坐标。

## 故障排查

### QML 启动后无响应

- 程序已强制使用 `Basic` 样式，避免原生样式不支持 `contentItem` 定制。
- 不要在 QML 事件中使用同步网络等待或死循环。
- 若 QML 根对象加载失败，检查临时目录中的 `DroneGroundStation-qml-startup.log`。

### 手机页面没有 IP 或打不开

- 以界面当前 `accessUrl` 为准，不使用上一次运行的地址。
- 确认手机和电脑同网段。
- Windows 防火墙允许程序监听 TCP 8080。
- 浏览器打开 `http://电脑IP:8080/health`，应得到 JSON 响应。

### 手机 GPS 没反应

- 检查浏览器定位权限和系统定位开关。
- 室内定位精度过差时，数据可能被安全过滤。
- 手机静止时小范围漂移被死区抑制是正常行为。
- 某些浏览器限制非 HTTPS 页面访问传感器，应检查浏览器控制台。

### STM32 已连热点但无数据

- 确认角色：STM32 是 TCP 服务端，Qt 是客户端。
- 确认端口为 8080，不能与旧测试端口混用。
- 每帧必须是完整 JSON 并以 `\r\n` 结束。
- 所有遥测字段必须存在，数字不能加引号。
- 遥测周期应小于 3.5 秒，否则看门狗会主动重连。
- 手机仍活动时 STM32 检测会暂停，这是互斥设计而非连接故障。

### 点击飞控按钮后无变化

- 按钮需要 TCP 已连接，且当前没有待确认命令。
- 参数必须在航向、高度、速度限制内。
- STM32 必须读取同一 TCP 连接上的命令并返回匹配 `command_seq` 的 ACK。
- ACK 只表示接受命令；地图是否变化取决于固件执行后继续发送的新位置遥测。

### 地图没有底图

STM32 AP 无外网时 OSM 瓦片请求失败是常见现象。可改用带互联网的 AP、预缓存瓦片，或后续接入离线地图插件。判断飞行是否正常应以遥测序号和坐标是否变化为主。

### 历史页面为空

- 必须先登录以创建 `session_id`。
- 必须收到有效手机或 STM32 数据后才有记录。
- 查看程序实际输出的数据库路径。
- 不要在程序运行时只复制 `.db` 而遗漏 WAL 文件。

## 发布与迁移

### 源码迁移

新设备需要安装兼容 Qt/MSVC 环境，重新配置 CMake 并构建。数据库会在项目 `database` 目录首次运行时自动创建；若希望保留旧历史数据，应在程序退出后单独复制数据库文件。

### 便携部署

Release 构建完成后使用 `windeployqt` 收集运行依赖：

```powershell
windeployqt --release --qmldir . appDroneGroundStation.exe
```

部署包应包含：

- `appDroneGroundStation.exe`
- Qt Core/Gui/Qml/Quick/Network/Sql/Positioning/Location 运行库
- `platforms`、`sqldrivers/qsqlite`、`geoservices`、`position` 插件
- QML 模块和 MSVC Release 运行库
- EXE 同级 `database` 目录

Debug 构建依赖调试运行库，不适合发送到没有开发环境的电脑。

## 项目目录

```text
DroneGroundStation/
├─ main.cpp                         程序入口和 QML 上下文注册
├─ backend.h/.cpp                   手机 HTTP、GPS 和在线租约
├─ sensorbackend.h/.cpp             手机温湿度、STM32 TCP 和飞控命令
├─ historydatabase.h/.cpp           SQLite 记录、查询与异常识别
├─ index.html                       手机采集页面
├─ Main.qml                         页面栈与导航
├─ LoginPage.qml                    登录页面
├─ MainPage.qml                     手机实时主页
├─ Stm32Page.qml                    STM32 遥测与控制页
├─ HistoryPage.qml                  历史数据页
├─ Dashboard.qml                    手机状态卡片
├─ TelemetryPanel.qml               手机遥测列表
├─ MapView.qml                      手机地图
├─ Stm32MapView.qml                 STM32 地图
├─ *Chart.qml / *Curve.qml          实时与历史曲线
├─ database/
│  └─ .gitkeep                      保留可迁移数据库目录
├─ docs/
│  ├─ 操作使用说明.md               操作、协议和部署手册
│  └─ 技术流程文档.md               函数链和数据流说明
├─ CMakeLists.txt                   Qt/CMake 构建配置
├─ .clang-format                    C++ 格式规范
├─ .gitattributes                   文本属性
└─ .gitignore                       构建产物和真实数据库忽略规则
```

## 进一步文档

- [详细操作与开发说明](docs/操作使用说明.md)
- [技术执行流程文档](docs/技术流程文档.md)

## 数据安全

数据库可能包含用户名、真实 GPS 位置和飞行遥测。仓库只保留空 `database` 目录，不上传实际数据。向他人发送运行目录前应检查并移除 `.db`、`.db-wal` 和 `.db-shm`。

## 当前限制与扩展方向

- 登录和注册仍是演示实现，可增加用户表、密码哈希和权限控制。
- HTTP/TCP 当前未加密，可增加 TLS、设备鉴权、签名和重放保护。
- 异常阈值目前固定在程序中，可改为设置页或数据库配置。
- OSM 依赖网络，可增加离线瓦片和缓存管理。
- 可为协议解析、GPS 过滤和数据库查询增加 Qt Test 自动测试。
- 可增加日志分级、日志文件轮转和协议抓包导出。

## 许可证

当前仓库未声明开源许可证。未经项目所有者许可，不应假定代码可以被任意复制、修改或商业使用。
