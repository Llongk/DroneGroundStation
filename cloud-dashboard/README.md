# DroneGroundStation Cloud Dashboard

DroneGroundStation Cloud Dashboard 是无人机地面站的跨设备监控网页。它同时支持 EMQX 实时遥测和 Cloudflare D1 历史遥测，可展示手机与 STM32 的位置、航线、曲线、设备参数和异常告警。

## 功能

- 通过 MQTT over WebSocket Secure 订阅 `dgs/#`。
- 手机与 STM32 数据源独立切换。
- 实时航线地图和航向标记。
- 高度、速度、电量、温度、湿度曲线。
- 低电量、温湿度和设备告警提示。
- D1 历史数据读取、去重和分页接口。
- 响应式布局，可在电脑、平板和手机浏览器展示。

## 架构

```text
Qt CloudBackend
  ├─ MQTT/TLS 8883 ──> EMQX Cloud ── WSS 8084 ──> DashboardClient
  └─ HTTPS POST ─────> /api/ingest ──> Cloudflare D1
                                                │
DashboardClient <──────── GET /api/history ─────┘
```

## 目录

```text
cloud-dashboard/
├─ app/
│  ├─ DashboardClient.tsx       实时连接、状态和交互
│  ├─ page.tsx                  页面入口
│  ├─ globals.css               响应式视觉样式
│  └─ api/
│     ├─ ingest/route.ts        受密钥保护的写入接口
│     └─ history/route.ts       历史查询接口
├─ db/
│  ├─ index.ts                  D1/Drizzle 数据库入口
│  └─ schema.ts                 telemetry_records 模型
├─ drizzle/                     数据库迁移
├─ worker/index.ts              Cloudflare Worker 入口
├─ .openai/hosting.json         Sites 项目与 D1 绑定
├─ tests/                       页面构建测试
└─ package.json                 命令和依赖
```

## 环境要求

- Node.js 22.13.0 或更高版本
- npm
- 可用的 Cloudflare D1/Sites 部署环境
- 可选：EMQX Cloud WebSocket over TLS 账号

## 本地开发

```bash
npm install
npm run dev
```

常用命令：

```bash
npm run lint
npm run build
npm test
npm run db:generate
```

本地环境文件、依赖和构建输出均已通过 `.gitignore` 排除。

## 云端配置

`.openai/hosting.json` 声明：

```json
{
  "project_id": "<Sites project id>",
  "d1": "DB",
  "r2": null
}
```

部署环境必须设置 Secret：

```text
INGEST_API_KEY=<high-entropy-random-secret>
```

不要把真实值写入代码、README、`.env` 示例或 Git 历史。

## 数据写入 API

```http
POST /api/ingest
Content-Type: application/json
X-Ingest-Key: <INGEST_API_KEY>
```

也兼容查询参数 `?key=`，但生产环境优先使用请求头，避免密钥进入代理访问日志。

请求体必须包含 `dgs/` 开头的 `topic`。`payload` 可以使用手机或 STM32 字段，接口会统一映射为 D1 列。相同 `message_id` 重复提交不会产生重复行。

## 历史查询 API

```http
GET /api/history?source=stm32&limit=500
GET /api/history?source=phone&limit=500
GET /api/history?source=stm32&limit=500&before=1784261492078
```

- `source`：`phone` 或 `stm32`。
- `limit`：1～1000，默认 500。
- `before`：可选毫秒时间戳，用于向前分页。

## 数据库

主表为 `telemetry_records`，包括：

- 消息：`message_id`、`topic`、`source`、`sequence`
- 会话：`device_id`、`session_id`
- 时间：`received_at`、`device_timestamp`
- 飞行：经纬度、高度、速度、航向、电量
- 环境：温度、湿度、告警代码
- 审计：完整原始 `payload`

唯一索引 `telemetry_message_id_uq` 用于幂等写入；`telemetry_source_time_idx` 用于快速读取某一来源的历史曲线。

## 页面使用

1. 打开部署后的 HTTPS 地址。
2. 若只看历史，选择数据源并点击“读取云端历史”。
3. 若看实时数据，输入 EMQX 地址、用户名和密码后连接。
4. 网页连接地址格式由程序生成：`wss://<host>:8084/mqtt`。
5. 页面订阅 `dgs/#`，按主题自动区分手机和 STM32。

建议为网页创建只有订阅权限的独立 EMQX 用户。浏览器输入的 MQTT 密码仅用于当前页面连接，不应写死在前端源码中。

## 与 Qt 工程对接

Qt 根目录 `config/cloud.json` 需要：

```json
{
  "historyIngestUrl": "https://your-site.example.com/api/ingest",
  "historyIngestKey": "same-as-INGEST_API_KEY"
}
```

Qt 使用 SQLite `cloud_outbox` 保存待上传消息：

- 新实时数据优先上传。
- 旧数据在空闲时间补传。
- HTTP 2xx 后设置 `http_uploaded=1`。
- 失败后保留记录并定时重试。

## 部署验证

部署后依次验证：

1. `GET /api/history?source=stm32&limit=1` 返回 JSON。
2. 无密钥调用 `POST /api/ingest` 返回 `401`。
3. Qt 启动后日志没有持续出现 `D1 history upload failed`。
4. D1 中 `telemetry_records` 数量增加。
5. 页面读取历史后显示曲线与航线。
6. EMQX 连接后消息计数持续增加。

## 安全与隐私

- 遥测包含真实位置，公开站点前应评估访问控制需求。
- 写接口必须启用 `INGEST_API_KEY`。
- MQTT 查看端使用只读账号。
- 不提交 `.env*`、证书私钥、数据库副本和平台令牌。
- 定期轮换 MQTT 密码和写入密钥。
- 如需限制历史读取，应在 `/api/history` 增加身份认证或站点访问策略。

## 故障排查

### 历史为 0 条

- 检查 Qt 的写入 URL 是否以 `/api/ingest` 结尾。
- 检查 `INGEST_API_KEY` 是否一致。
- 检查 D1 绑定名是否为 `DB`。
- 查看 Qt HTTP 状态码和响应正文。

### 实时数据不更新

- 检查 WSS 端口是否为 `8084`。
- 检查 EMQX 用户是否能订阅 `dgs/#`。
- 在 EMQX 在线调试中确认 Broker 已收到消息。

### 手机连接后仍没有数据

- 手机访问 Qt 页面不代表已授权 GPS。
- 先确认 Qt 本地界面与 `cloud_outbox` 已出现手机记录。
- 新版本会优先上传最新手机记录；旧积压随后补传。

完整部署、迁移和 Git 操作说明见仓库根目录 [`docs/云端部署与跨设备访问.md`](../docs/云端部署与跨设备访问.md)。
