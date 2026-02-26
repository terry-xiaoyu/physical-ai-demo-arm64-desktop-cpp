# VolcEngineRTC ARM64 Desktop Demo (C++)

基于火山引擎实时音视频 (VolcEngineRTC) Linux ARM64 SDK 的 Qt 桌面端演示工程。通过 MQTT 协议与智能体交互，发起语音会话并自动加入 RTC 房间。

## 使用说明

启动应用后，在登录界面填写以下信息：

1. **MQTT Broker 地址** — 支持以下连接方式：
   - `tcp://host:1883` — 普通 TCP 连接
   - `ssl://host:8883` — TCP + TLS 加密连接
   - `ws://host:8083/mqtt` — WebSocket 连接
   - `wss://host:8084/mqtt` — WebSocket + TLS 加密连接

   使用 `ssl://` 或 `wss://` 时，应用会自动启用 TLS 并验证服务器证书（使用系统 CA 证书库）。
2. **Agent ID** — 智能体 ID
3. **Client ID** — 客户端 ID（也用作 MQTT Client ID）

点击 "开始语音通话" 后，应用会自动：
1. 连接到 MQTT Broker
2. 向智能体发送 `initializeSession` 初始化会话
3. 发送 `startVoiceChat` 发起语音通话
4. 从智能体的应答中获取 `appId`、`roomId`、`token`、`userId`、`targetUserId`
5. 使用 `targetUserId` 作为本端用户 ID 加入 RTC 房间

挂断时，应用会发送 `stopVoiceChat` 和 `destroySession` 给智能体，并断开 MQTT 连接。

## 平台与架构

- **目标平台**: Linux aarch64 (ARM64)
- **SDK 版本**: 3.60.103.4700
- **UI 框架**: Qt5

## 环境要求

- Linux aarch64 (ARM64) 系统，在 [Ubuntu 24.04 Desktop ARM64](https://cdimage.ubuntu.com/releases/24.04/release/ubuntu-24.04.4-desktop-arm64.iso) 上测试通过
- GCC/G++ (支持 C++17)
- CMake 3.14+
- Qt5 开发库 (Widgets, Core, Gui, Network)
- OpenSSL 开发库
- PulseAudio 开发库 (音频采集/播放)
- nlohmann/json (>= 3.9)
- Eclipse Paho MQTT C 库
- Eclipse Paho MQTT C++ 库
- mcp-over-mqtt-cpp-sdk（已包含在同级目录 `../mcp-over-mqtt-cpp-sdk`）

## 安装依赖 (Ubuntu/Debian)

### 1. 系统基础依赖

```sh
sudo apt update
sudo apt install build-essential cmake git qtbase5-dev qt5-qmake qtchooser \
    libssl-dev libpulse-dev nlohmann-json3-dev
```

### 2. 安装 Paho MQTT C 库

```sh
git clone https://github.com/eclipse/paho.mqtt.c.git
cd paho.mqtt.c
mkdir build && cd build
cmake .. -DPAHO_WITH_SSL=ON -DPAHO_BUILD_SHARED=ON
make -j$(nproc)
sudo make install
sudo ldconfig
cd ../..
```

### 3. 安装 Paho MQTT C++ 库

```sh
git clone https://github.com/eclipse/paho.mqtt.cpp.git
cd paho.mqtt.cpp
mkdir build && cd build
cmake .. -DPAHO_WITH_SSL=ON -DPAHO_BUILD_SHARED=ON
make -j$(nproc)
sudo make install
sudo ldconfig
cd ../..
```

### 4. 获取 mcp-over-mqtt-cpp-sdk

将 mcp-over-mqtt-cpp-sdk 放在与本项目同级的目录下：

```sh
# 确保目录结构如下：
# parent_dir/
# ├── physical-ai-demo-arm64-desktop-cpp/   (本项目)
# └── mcp-over-mqtt-cpp-sdk/                (SDK)

git clone https://github.com/terry-xiaoyu/mcp-over-mqtt-cpp-sdk.git
```

> 注意: 不同发行版的包名可能有差异，请根据实际情况调整。如果系统包仓库中没有 `nlohmann-json3-dev`，也可以从源码安装：
> ```sh
> git clone https://github.com/nlohmann/json.git
> cd json && mkdir build && cd build
> cmake .. && sudo make install
> ```

### 5. 获取 VolcEngineRTC SDK

从 https://www.volcengine.com/docs/6348/75707?lang=zh 或者火山技术团队获取 SDK 压缩包: VolcEngineRTC_Linux_3.60.103.4700_aarch64_Release.zip，然后解压到本项目根目录下：

```sh
unzip -q VolcEngineRTC_Linux_3.60.103.4700_aarch64_Release.zip
mv VolcEngineRTC_Linux_3.60.103.4700_aarch64_Release VolcEngineRTC_arm64
```

## 编译

```sh
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## 运行

编译完成后，需要确保运行时能找到 SDK 动态库：

```sh
# 方式一：设置 LD_LIBRARY_PATH
export LD_LIBRARY_PATH=$(pwd)/../VolcEngineRTC_arm64/lib:$LD_LIBRARY_PATH
./QuickStart

# 方式二：将 .so 文件复制到可执行文件同级目录
cp ../VolcEngineRTC_arm64/lib/*.so .
./QuickStart
```

### MCP 工具

应用在连接 MQTT 后会同时启动一个 MCP 服务器，通过 `mcp-over-mqtt-cpp-sdk` 向智能体暴露可调用的工具。目前已注册以下工具：

| 工具名 | 描述 | 参数 |
|--------|------|------|
| `light` | 控制灯的开关 | `action`: `"on"` 或 `"off"` |

当智能体调用 `light` 工具时，界面左上角的圆形灯指示器会相应变化：
- **关闭** — 深灰色 (`#555555`)
- **打开** — 金黄色 (`#FFD700`)

## MCP 工具开发指南

本项目基于 [mcp-over-mqtt-cpp-sdk](https://github.com/terry-xiaoyu/mcp-over-mqtt-cpp-sdk) 实现 MCP 工具。SDK 的核心设计原则是 **"用户控制 MQTT 客户端，SDK 处理 MCP 逻辑"** — SDK 不管理 MQTT 连接，只处理 `$mcp-*` 主题的消息。

### 架构概览

```
                    ┌──────────────────────┐
                    │     MQTT Broker       │
                    └──────┬───────────────┘
                           │
              ┌────────────┴────────────┐
              │  Paho mqtt::async_client │   ← 应用自己创建和管理的 MQTT 客户端
              └────────────┬────────────┘
                           │
            ┌──────────────┼──────────────┐
            │              │              │
            ▼              ▼              ▼
    ┌──────────────┐ ┌──────────┐ ┌────────────────┐
    │ AgentClient  │ │ MCP SDK  │ │  其他自定义主题  │
    │ (智能体协议)  │ │ ($mcp-*) │ │                │
    └──────────────┘ └──────────┘ └────────────────┘
```

本项目的 `AgentClient` 同时承担两个角色：
1. **智能体客户端** — 通过 `$agent-*` 主题与智能体交互（initializeSession、startVoiceChat 等）
2. **MCP 服务器** — 通过 `$mcp-*` 主题暴露工具供智能体调用

两者共享同一条 MQTT 连接，通过 `McpMqttAdapter`（`IMqttClient` 接口的适配器）将已有的 Paho 客户端桥接给 MCP SDK。

### 实现步骤详解

#### 第 1 步：实现 IMqttClient 适配器

MCP SDK 通过 `IMqttClient` 接口操作 MQTT 连接。你需要将自己的 MQTT 客户端包装为该接口的实现。本项目在 `AgentClient.cpp` 中以内部类 `McpMqttAdapter` 实现：

```cpp
#include <mcp_mqtt/mcp_server.h>
#include <mcp_mqtt/mqtt_interface.h>

class McpMqttAdapter : public mcp_mqtt::IMqttClient {
public:
    explicit McpMqttAdapter(mqtt::async_client* client, const std::string& clientId)
        : m_client(client), m_clientId(clientId) {}

    bool isConnected() const override {
        return m_client && m_client->is_connected();
    }

    bool subscribe(const std::string& topic, int qos, bool noLocal) override {
        mqtt::subscribe_options subOpts;
        subOpts.set_no_local(noLocal);
        m_client->subscribe(topic, qos, subOpts);
        return true;
    }

    bool unsubscribe(const std::string& topic) override {
        m_client->unsubscribe(topic);
        return true;
    }

    bool publish(const std::string& topic, const std::string& payload,
                 int qos, bool retained,
                 const std::map<std::string, std::string>& userProps) override {
        auto msg = mqtt::make_message(topic, payload, qos, retained);
        mqtt::properties props;
        for (const auto& [key, value] : userProps) {
            props.add(mqtt::property(mqtt::property::USER_PROPERTY, key, value));
        }
        msg->set_properties(props);
        m_client->publish(msg);
        return true;
    }

    std::string getClientId() const override { return m_clientId; }

    void setMessageHandler(mcp_mqtt::MqttMessageHandler handler) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_handler = handler;
    }

    void setConnectionLostCallback(std::function<void(const std::string&)>) override {}

    // 由 MQTT 回调调用，将消息转发给 MCP SDK
    void forwardMessage(mqtt::const_message_ptr msg) {
        mcp_mqtt::MqttMessageHandler handler;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            handler = m_handler;
        }
        if (!handler) return;

        mcp_mqtt::MqttIncomingMessage inMsg;
        inMsg.topic = msg->get_topic();
        inMsg.payload = msg->to_string();
        inMsg.qos = msg->get_qos();
        inMsg.retained = msg->is_retained();
        // ... 提取 MQTT 5.0 user properties ...
        handler(inMsg);
    }

private:
    mqtt::async_client* m_client;
    std::string m_clientId;
    std::mutex m_mutex;
    mcp_mqtt::MqttMessageHandler m_handler;
};
```

#### 第 2 步：在 MQTT 回调中转发消息

Paho MQTT 的 `message_arrived` 回调中，需要同时将消息转发给 MCP SDK 和原有的业务逻辑处理器：

```cpp
void message_arrived(mqtt::const_message_ptr msg) override {
    // 1. 转发给 MCP SDK（在 MQTT 线程上执行，SDK 内部线程安全）
    if (m_mcpAdapter) {
        m_mcpAdapter->forwardMessage(msg);
    }

    // 2. 转发给业务逻辑（通过 Qt 事件队列转到主线程）
    QMetaObject::invokeMethod(owner, "handleMessage",
        Qt::QueuedConnection, ...);
}
```

#### 第 3 步：配置并启动 MCP 服务器

MQTT 连接建立后，创建适配器并启动 `McpServer`：

```cpp
#include <mcp_mqtt/mcp_server.h>

// 创建适配器
auto adapter = std::make_unique<McpMqttAdapter>(mqttClient.get(), clientId);

// 配置 MCP 服务器
mcp_mqtt::McpServer mcpServer;

mcp_mqtt::ServerInfo info;
info.name = "MyServer";
info.version = "1.0.0";

mcp_mqtt::ServerCapabilities caps;
caps.tools = true;

mcpServer.configure(info, caps);
mcpServer.setServiceDescription("My MCP server description");

// 启动
mcp_mqtt::McpServerConfig config;
config.serverId = clientId;          // 唯一标识，通常用 MQTT Client ID
config.serverName = "my-app/tools";  // 层级式服务名

mcpServer.start(adapter.get(), config);
```

#### 第 4 步：注册工具

使用 `McpServer::registerTool()` 注册工具。每个工具包含：
- **name** — 工具名称（英文标识符）
- **description** — 工具描述
- **inputSchema** — JSON Schema 格式的参数定义
- **handler** — 工具被调用时的回调函数

以 `light` 工具为例：

```cpp
mcp_mqtt::Tool lightTool;
lightTool.name = "light";
lightTool.description = "Control the light - turn it on or off";
lightTool.inputSchema.properties = {
    {"action", {
        {"type", "string"},
        {"description", "Action to perform: 'on' to turn on, 'off' to turn off"},
        {"enum", nlohmann::json::array({"on", "off"})}
    }}
};
lightTool.inputSchema.required = {"action"};

mcpServer.registerTool(lightTool,
    [this](const nlohmann::json& args) -> mcp_mqtt::ToolCallResult {
        std::string action = args.value("action", "");
        if (action != "on" && action != "off") {
            return mcp_mqtt::ToolCallResult::error("Invalid action. Use 'on' or 'off'.");
        }

        bool on = (action == "on");

        // 注意：此回调运行在 MQTT 线程上，需要安全地通知 UI 线程
        QMetaObject::invokeMethod(this, [this, on]() {
            emit lightStateChanged(on);
        }, Qt::QueuedConnection);

        return mcp_mqtt::ToolCallResult::success(
            on ? "Light turned on" : "Light turned off");
    });
```

#### 第 5 步：在 UI 中响应工具调用

在 Qt 主窗口中连接信号并更新 UI：

```cpp
// 创建灯指示器（50×50 圆形）
m_lightIndicator = new QWidget(parentWidget);
m_lightIndicator->setFixedSize(50, 50);
m_lightIndicator->setAttribute(Qt::WA_StyledBackground, true);
m_lightIndicator->setStyleSheet(
    "background-color: #555555; border-radius: 25px; border: 3px solid #777777;");

// 连接信号
connect(agentClient, &AgentClient::lightStateChanged, this, [this](bool on) {
    m_lightIndicator->setStyleSheet(on
        ? "background-color: #FFD700; border-radius: 25px; border: 3px solid #FFA500;"
        : "background-color: #555555; border-radius: 25px; border: 3px solid #777777;");
});
```

#### 第 6 步：停止 MCP 服务器

在断开 MQTT 连接之前停止 MCP 服务器：

```cpp
if (mcpServer.isRunning()) {
    mcpServer.stop();   // 清除 presence，取消 MCP 主题订阅
}
// ... 然后断开 MQTT 连接 ...
```

### 添加新工具

要添加一个新的 MCP 工具，只需在 `AgentClient::setupMcpServer()` 中追加注册代码即可。以下是一个温度传感器工具的示例：

```cpp
mcp_mqtt::Tool tempTool;
tempTool.name = "get_temperature";
tempTool.description = "Read the current temperature from the sensor";
tempTool.inputSchema.properties = {
    {"unit", {
        {"type", "string"},
        {"description", "Temperature unit"},
        {"enum", nlohmann::json::array({"celsius", "fahrenheit"})}
    }}
};
tempTool.inputSchema.required = {"unit"};

m_mcpServer.registerTool(tempTool,
    [](const nlohmann::json& args) -> mcp_mqtt::ToolCallResult {
        std::string unit = args.value("unit", "celsius");
        double temp = 25.0;  // 实际项目中从传感器读取
        if (unit == "fahrenheit") {
            temp = temp * 9.0 / 5.0 + 32.0;
        }
        return mcp_mqtt::ToolCallResult::success(
            "Current temperature: " + std::to_string(temp) + (unit == "celsius" ? "°C" : "°F"));
    });
```

### 线程安全注意事项

MCP 工具的回调函数运行在 **MQTT 内部线程**上（非 Qt 主线程），因此：

- 不要在回调中直接操作 Qt UI 控件
- 使用 `QMetaObject::invokeMethod(obj, lambda, Qt::QueuedConnection)` 将 UI 更新投递到主线程
- 使用 Qt 信号（默认 `Qt::AutoConnection`）在跨线程时会自动使用队列连接

### 关键类型参考

| 类型 | 说明 |
|------|------|
| `mcp_mqtt::McpServer` | MCP 服务器主类，管理工具注册和客户端会话 |
| `mcp_mqtt::IMqttClient` | MQTT 客户端接口，用户需实现此接口 |
| `mcp_mqtt::Tool` | 工具定义（name, description, inputSchema） |
| `mcp_mqtt::ToolInputSchema` | 工具参数的 JSON Schema 定义 |
| `mcp_mqtt::ToolCallResult` | 工具调用返回值，提供 `success()` 和 `error()` 静态工厂方法 |
| `mcp_mqtt::ToolHandler` | 工具回调类型：`std::function<ToolCallResult(const nlohmann::json&)>` |
| `mcp_mqtt::McpServerConfig` | 服务器配置（serverId, serverName） |

更多 SDK 细节请参阅 `../mcp-over-mqtt-cpp-sdk/README.md`。

## 打包 (可选)

工程提供了 `archive` 目标，可将可执行文件与依赖库打包为 AppImage：

```sh
make archive
```

> 需要预先安装 `patchelf` 和 `linuxdeployqt`。

## 项目结构

```
physical-ai-demo-arm64-desktop-cpp/
├── CMakeLists.txt                  # 构建配置
├── sources/                        # 应用源码
│   ├── main.cpp                    # 程序入口
│   ├── AgentClient.h/cpp           # MQTT 智能体客户端 + MCP 服务器
│   ├── RoomMainWidget.h/cpp        # 主窗口，管理 RTC 引擎与房间
│   ├── LoginWidget.h/cpp           # 登录界面（MQTT 配置输入）
│   ├── OperateWidget.h/cpp         # 操作面板（挂断、静音等）
│   └── VideoWidget.h/cpp           # 视频渲染组件
├── ui/                             # Qt Designer UI 文件
├── specs/                          # 协议文档
│   └── client_agent_message_protocol.md
├── VolcEngineRTC_arm64/            # ARM64 SDK (不包含在代码库中，自行下载并放置)
│   ├── include/                    # 头文件
│   └── lib/                        # 动态库 (.so)
└── Resources/                      # 资源文件
```

## 常见问题

### 找不到动态库

```
error while loading shared libraries: libVolcEngineRTC.so: cannot open shared object file
```

请通过 `LD_LIBRARY_PATH` 或 rpath 指定 SDK lib 目录，参见上方"运行"一节。

### 音频设备问题

确保 PulseAudio 服务正在运行：

```sh
pulseaudio --start
```

### 找不到 PahoMqttCpp

如果 cmake 报 `Could not find a package configuration file provided by "PahoMqttCpp"`，请确保已按上述步骤安装 Paho MQTT C 和 C++ 库，并执行了 `sudo ldconfig`。如果安装到了非默认路径，可通过 `CMAKE_PREFIX_PATH` 指定：

```sh
cmake .. -DCMAKE_PREFIX_PATH=/usr/local
```

### 找不到 nlohmann/json

如果 cmake 报 `Could not find a package configuration file provided by "nlohmann_json"`，请确保已安装 `nlohmann-json3-dev` 包或从源码安装。
