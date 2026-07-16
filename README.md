# DroneGroundStation 无人机地面控制站

DroneGroundStation 是基于 Qt 6、Qt Quick/QML 和 C++17 开发的无人机地面控制站。程序同时支持手机传感器实测和 STM32 AP 热点遥测，但为了避免数据源冲突，同一时刻只允许一个设备作为活动数据源。

主要功能：

- 手机网页采集 GPS、姿态、海拔、电量、温湿度并显示实时地图与曲线。
- STM32 通过 TCP 发送 JSON 遥测，显示实时航线、遥测参数、曲线和告警。
- STM32 人工控制：起飞、悬停、盘旋、改变航向、返航、降落。
- STM32 自由飞行：执行固件预设航线。
- SQLite 分表保存手机和 STM32 历史数据，并按登录会话查询轨迹和异常点。
- 自动适配当前 Wi-Fi/热点网段，不在程序中写死固定 IP。
- 项目内 `database` 目录便于整体迁移。

## 快速开始

开发环境建议：

- Windows 10/11 64 位
- Qt 6.11.1（至少包含 Quick、Quick Controls 2、Network、Sql、Positioning、Location）
- Qt Creator
- MSVC 2022 64 位工具链
- CMake 3.16 或更高版本

使用 Qt Creator 打开 `CMakeLists.txt`，选择 Desktop Qt 6.11.1 MSVC2022 64bit Kit，执行 Configure、Build、Run。默认登录账号为 `admin`，密码为 `123456`。

> `build` 目录不是源码的一部分，不应提交。迁移到新开发设备后需要重新配置 Qt Kit 并构建。若需要无需 Qt 环境即可运行的版本，应另外生成 Release 便携部署包并使用 `windeployqt` 收集运行库。

## 使用入口

- 手机模式：电脑与手机处于同一热点/Wi-Fi，启动程序后，在手机浏览器打开主页顶部显示的 URL，允许定位和传感器权限。
- STM32 模式：电脑连接 STM32 AP 热点。程序根据当前 WLAN 地址和掩码推导热点网关，默认连接 TCP `8080` 端口。
- 历史数据：登录后点击主页顶部“历史数据”。
- STM32 控制：点击主页顶部“STM32 控制”；检测到 STM32 时也可从连接提示进入。

## 文档

完整的架构、页面、函数、通信协议、数据库、阈值、调试和部署说明见：

- [详细操作与开发说明](docs/操作使用说明.md)

## 数据与隐私

运行时数据库为 `database/FlightHistory.db`，其中可能包含用户名、真实 GPS 位置和飞行遥测，因此 `.gitignore` 默认不上传数据库文件。程序会在首次运行时自动创建数据库和数据表。

如需在可信设备之间迁移历史数据，可在程序完全退出后，单独复制 `database/FlightHistory.db`；若存在 `FlightHistory.db-wal` 和 `FlightHistory.db-shm`，应将三个文件一并复制。

## 目录概览

```text
DroneGroundStation/
├─ main.cpp                       程序入口与 C++/QML 对象注册
├─ backend.h/.cpp                 手机 HTTP 服务与 GPS 数据处理
├─ sensorbackend.h/.cpp           手机温湿度、STM32 TCP 遥测和飞控命令
├─ historydatabase.h/.cpp         SQLite 记录、查询和异常识别
├─ index.html                     手机采集页面
├─ Main.qml                       页面栈与导航
├─ LoginPage.qml                  登录页
├─ MainPage.qml                   手机实时数据主页
├─ Stm32Page.qml                  STM32 遥测和控制页
├─ HistoryPage.qml                历史数据入口页
├─ MapView.qml                    手机地图
├─ Stm32MapView.qml               STM32 实时航线地图
├─ *Chart.qml / *Curve.qml        曲线图组件
├─ database/                      运行时数据库目录
├─ docs/                          项目文档
└─ CMakeLists.txt                 Qt/CMake 构建配置
```

## 许可证

当前仓库未声明开源许可证。未经项目所有者许可，不应假定代码可被任意复制、修改或商业使用。
