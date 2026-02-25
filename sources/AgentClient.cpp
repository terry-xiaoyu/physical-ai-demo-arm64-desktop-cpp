#include "AgentClient.h"
#include <QMetaObject>
#include <chrono>
#include <mutex>

// ── IMqttClient 适配器 ─────────────────────────────────────────────
// 将已有的 Paho mqtt::async_client 包装为 MCP SDK 所需的 IMqttClient 接口，
// 使 McpServer 能复用同一条 MQTT 连接来订阅/发布 MCP 协议消息。
class AgentClient::McpMqttAdapter : public mcp_mqtt::IMqttClient {
public:
    explicit McpMqttAdapter(mqtt::async_client* client, const std::string& clientId)
        : m_client(client), m_clientId(clientId) {}

    bool isConnected() const override {
        return m_client && m_client->is_connected();
    }

    bool subscribe(const std::string& topic, int qos, bool noLocal) override {
        try {
            mqtt::subscribe_options subOpts;
            subOpts.set_no_local(noLocal);
            m_client->subscribe(topic, qos, subOpts)->wait();
            return true;
        } catch (const mqtt::exception& e) {
            qWarning() << "MCP subscribe error:" << e.what();
            return false;
        }
    }

    bool unsubscribe(const std::string& topic) override {
        try {
            m_client->unsubscribe(topic)->wait();
            return true;
        } catch (const mqtt::exception& e) {
            qWarning() << "MCP unsubscribe error:" << e.what();
            return false;
        }
    }

    bool publish(const std::string& topic, const std::string& payload,
                 int qos, bool retained,
                 const std::map<std::string, std::string>& userProps) override {
        try {
            auto msg = mqtt::make_message(topic, payload, qos, retained);
            mqtt::properties props;
            for (const auto& [key, value] : userProps) {
                props.add(mqtt::property(mqtt::property::USER_PROPERTY, key, value));
            }
            msg->set_properties(props);
            m_client->publish(msg);
            return true;
        } catch (const mqtt::exception& e) {
            qWarning() << "MCP publish error:" << e.what();
            return false;
        }
    }

    std::string getClientId() const override {
        return m_clientId;
    }

    void setMessageHandler(mcp_mqtt::MqttMessageHandler handler) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_handler = handler;
    }

    void setConnectionLostCallback(std::function<void(const std::string&)>) override {
        // 连接断开已由 MqttCallbackBridge 处理
    }

    // 由 MqttCallbackBridge 调用，将收到的消息转发给 MCP SDK
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

        const auto& props = msg->get_properties();
        // 使用 C 层 API 直接遍历属性，避免 mqtt::get 模板的版本兼容问题
        const auto& cProps = props.c_struct();
        for (int i = 0; i < cProps.count; ++i) {
            if (cProps.array[i].identifier == MQTTPROPERTY_CODE_USER_PROPERTY) {
                std::string key(cProps.array[i].value.data.data,
                                cProps.array[i].value.data.len);
                std::string val(cProps.array[i].value.value.data,
                                cProps.array[i].value.value.len);
                inMsg.userProperties[key] = val;
            }
        }

        handler(inMsg);
    }

private:
    mqtt::async_client* m_client;
    std::string m_clientId;
    std::mutex m_mutex;
    mcp_mqtt::MqttMessageHandler m_handler;
};

// ── 内部 MQTT 回调桥接类 ───────────────────────────────────────────
// Paho 的回调运行在内部线程上，通过 QMetaObject::invokeMethod
// 将消息安全地转发到 Qt 主线程处理；同时也转发给 MCP SDK。
class AgentClient::MqttCallbackBridge : public mqtt::callback {
public:
    explicit MqttCallbackBridge(AgentClient *owner) : m_owner(owner) {}

    void setMcpAdapter(McpMqttAdapter* adapter) { m_mcpAdapter = adapter; }

    void message_arrived(mqtt::const_message_ptr msg) override {
        // 先转发给 MCP SDK（在 MQTT 线程上执行，SDK 内部处理线程安全）
        if (m_mcpAdapter) {
            m_mcpAdapter->forwardMessage(msg);
        }

        // 再转发给智能体协议处理器（通过 Qt 事件队列发到主线程）
        QString topic = QString::fromStdString(msg->get_topic());
        QString payload = QString::fromStdString(msg->to_string());
        QMetaObject::invokeMethod(m_owner, "handleMessage",
            Qt::QueuedConnection,
            Q_ARG(QString, topic),
            Q_ARG(QString, payload));
    }

    void connection_lost(const std::string &cause) override {
        QString reason = QString::fromStdString(cause);
        QMetaObject::invokeMethod(m_owner, "handleConnectionLost",
            Qt::QueuedConnection,
            Q_ARG(QString, reason));
    }

    void connected(const std::string &) override {}
    void delivery_complete(mqtt::delivery_token_ptr) override {}

private:
    AgentClient *m_owner;
    McpMqttAdapter* m_mcpAdapter = nullptr;
};

// ── AgentClient 实现 ──────────────────────────────────────────────

AgentClient::AgentClient(QObject *parent)
    : QObject(parent) {
}

AgentClient::~AgentClient() {
    stop();
}

void AgentClient::start(const QString &brokerUrl,
                         const QString &agentId,
                         const QString &clientId) {
    m_agentId = agentId.toStdString();
    m_clientId = clientId.toStdString();
    m_nextRequestId = 1;

    try {
        mqtt::create_options createOpts(MQTTVERSION_5);
        m_mqttClient = std::make_unique<mqtt::async_client>(
            brokerUrl.toStdString(), m_clientId, createOpts);

        m_callbackBridge = std::make_unique<MqttCallbackBridge>(this);
        m_mqttClient->set_callback(*m_callbackBridge);

        auto connOpts = mqtt::connect_options_builder()
            .mqtt_version(MQTTVERSION_5)
            .clean_start(true)
            .keep_alive_interval(std::chrono::seconds(60))
            .finalize();

        m_mqttClient->connect(connOpts)->wait_for(std::chrono::seconds(10));

        if (!m_mqttClient->is_connected()) {
            emit errorOccurred(QStringLiteral(u"连接 MQTT Broker 超时"));
            return;
        }

        // 订阅智能体回复主题
        std::string subTopic = "$agent-client/" + m_clientId + "/#";
        m_mqttClient->subscribe(subTopic, 1)->wait_for(std::chrono::seconds(5));

        qDebug() << "MQTT connected, subscribed to:" << subTopic.c_str();

        // 启动 MCP 服务器（注册灯控制等工具）
        setupMcpServer();

        // 发送初始化会话
        sendInitializeSession();

    } catch (const mqtt::exception &e) {
        emit errorOccurred(QString("MQTT 错误: %1").arg(e.what()));
    } catch (const std::exception &e) {
        emit errorOccurred(QString("错误: %1").arg(e.what()));
    }
}

void AgentClient::stop() {
    if (m_mqttClient && m_mqttClient->is_connected()) {
        try {
            // 先停止 MCP 服务器（清除 presence，取消 MCP 主题订阅）
            if (m_mcpServer.isRunning()) {
                m_mcpServer.stop();
            }
            sendStopVoiceChat();
            sendDestroySession();
            m_mqttClient->disconnect()->wait_for(std::chrono::seconds(2));
        } catch (...) {}
    }
    if (m_callbackBridge) {
        m_callbackBridge->setMcpAdapter(nullptr);
    }
    m_mcpAdapter.reset();
    m_mqttClient.reset();
    m_callbackBridge.reset();
}

bool AgentClient::isConnected() const {
    return m_mqttClient && m_mqttClient->is_connected();
}

// ── 消息处理 ──────────────────────────────────────────────────────

void AgentClient::handleMessage(const QString &topic, const QString &payload) {
    qDebug() << "MQTT message on" << topic << ":" << payload;

    try {
        auto json = nlohmann::json::parse(payload.toStdString());

        // JSON-RPC 响应（带 id）
        if (json.contains("id") && !json["id"].is_null()) {
            // 提取 id（兼容字符串和数字类型）
            std::string responseId;
            if (json["id"].is_string()) {
                responseId = json["id"].get<std::string>();
            } else if (json["id"].is_number_integer()) {
                responseId = std::to_string(json["id"].get<int64_t>());
            }

            // 处理错误响应
            if (json.contains("error")) {
                auto error = json["error"];
                QString errorMsg = QString::fromStdString(
                    error.value("message", "Unknown error"));
                emit errorOccurred(errorMsg);
                return;
            }

            // 处理成功响应
            if (json.contains("result")) {
                auto result = json["result"];

                if (responseId == m_initSessionId) {
                    qDebug() << "Session initialized, sending startVoiceChat";
                    sendStartVoiceChat();
                }
                else if (responseId == m_startVoiceChatId) {
                    QString appId = QString::fromStdString(result.value("appId", ""));
                    QString roomId = QString::fromStdString(result.value("roomId", ""));
                    QString token = QString::fromStdString(result.value("token", ""));
                    QString userId = QString::fromStdString(result.value("userId", ""));
                    QString targetUserId = QString::fromStdString(result.value("targetUserId", ""));

                    qDebug() << "VoiceChat ready: appId=" << appId
                             << "roomId=" << roomId
                             << "userId=" << userId
                             << "targetUserId=" << targetUserId;

                    emit voiceChatReady(appId, roomId, token, userId, targetUserId);
                }
                else if (responseId == m_stopVoiceChatId) {
                    emit voiceChatStopped();
                }
            }
        }
        // JSON-RPC 通知（无 id，有 method）
        else if (json.contains("method")) {
            std::string method = json["method"].get<std::string>();

            if (method == "voiceChatStopped") {
                qDebug() << "Received voiceChatStopped notification from agent";
                emit voiceChatStopped();
            }
            else if (method == "destroySession") {
                qDebug() << "Received destroySession notification from agent";
                emit voiceChatStopped();
            }
            else if (method == "textTalkDelta") {
                auto params = json.value("params", nlohmann::json::object());
                QString delta = QString::fromStdString(params.value("textDelta", ""));
                if (!delta.isEmpty()) {
                    qDebug() << "Received textTalkDelta:" << delta;
                    emit textDeltaReceived(delta);
                }
            }
            else if (method == "textTalkFinished") {
                qDebug() << "Received textTalkFinished";
                emit textFinished();
            }
        }

    } catch (const std::exception &e) {
        qWarning() << "Failed to parse MQTT message:" << e.what();
    }
}

void AgentClient::handleConnectionLost(const QString &reason) {
    qWarning() << "MQTT connection lost:" << reason;
    emit errorOccurred(QStringLiteral(u"MQTT 连接断开: ") + reason);
}

// ── MCP 服务器设置 ──────────────────────────────────────────────────

void AgentClient::setupMcpServer() {
    // 创建适配器，将已有 MQTT 连接包装为 MCP SDK 接口
    m_mcpAdapter = std::make_unique<McpMqttAdapter>(m_mqttClient.get(), m_clientId);
    m_callbackBridge->setMcpAdapter(m_mcpAdapter.get());

    // 配置 MCP 服务器
    mcp_mqtt::ServerInfo info;
    info.name = "PhysicalAIDemo";
    info.version = "1.0.0";

    mcp_mqtt::ServerCapabilities caps;
    caps.tools = true;

    m_mcpServer.configure(info, caps);
    m_mcpServer.setServiceDescription("Physical AI demo with light control tool");

    // 注册 "light" 工具
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

    m_mcpServer.registerTool(lightTool, [this](const nlohmann::json& args) -> mcp_mqtt::ToolCallResult {
        std::string action = args.value("action", "");
        if (action != "on" && action != "off") {
            return mcp_mqtt::ToolCallResult::error("Invalid action. Use 'on' or 'off'.");
        }

        bool on = (action == "on");

        // 安全地将 UI 更新投递到 Qt 主线程
        QMetaObject::invokeMethod(this, [this, on]() {
            emit lightStateChanged(on);
        }, Qt::QueuedConnection);

        return mcp_mqtt::ToolCallResult::success(
            on ? "Light turned on" : "Light turned off");
    });

    // 启动 MCP 服务器
    mcp_mqtt::McpServerConfig mcpConfig;
    mcpConfig.serverId = m_clientId;
    mcpConfig.serverName = "physical-ai-demo/tools";

    if (!m_mcpServer.start(m_mcpAdapter.get(), mcpConfig)) {
        qWarning() << "Failed to start MCP server";
    } else {
        qDebug() << "MCP server started with light tool";
    }
}

// ── 协议消息发送 ──────────────────────────────────────────────────

void AgentClient::sendInitializeSession() {
    m_initSessionId = std::to_string(m_nextRequestId++);

    mcp_mqtt::JsonRpcRequest req;
    req.id = m_initSessionId;
    req.method = "initializeSession";
    req.params = nlohmann::json::object();

    publishToAgent(req.toJson());
}

void AgentClient::sendStartVoiceChat() {
    m_startVoiceChatId = std::to_string(m_nextRequestId++);

    mcp_mqtt::JsonRpcRequest req;
    req.id = m_startVoiceChatId;
    req.method = "startVoiceChat";
    req.params = nlohmann::json::object();

    publishToAgent(req.toJson());
}

void AgentClient::sendStopVoiceChat() {
    m_stopVoiceChatId = std::to_string(m_nextRequestId++);

    mcp_mqtt::JsonRpcRequest req;
    req.id = m_stopVoiceChatId;
    req.method = "stopVoiceChat";
    req.params = nlohmann::json::object();

    publishToAgent(req.toJson());
}

void AgentClient::sendDestroySession() {
    // destroySession 是通知（无 id）
    mcp_mqtt::JsonRpcNotification notif =
        mcp_mqtt::JsonRpcNotification::create("destroySession", nlohmann::json::object());

    publishToAgent(notif.toJson());
}

void AgentClient::sendTextTalk(const QString &text) {
    std::string taskId = "text-" + std::to_string(m_nextTaskId++);

    nlohmann::json params = {
        {"taskId", taskId},
        {"text", text.toStdString()}
    };

    mcp_mqtt::JsonRpcNotification notif =
        mcp_mqtt::JsonRpcNotification::create("textTalk", params);

    qDebug() << "Sending textTalk, taskId=" << taskId.c_str() << ", text=" << text;
    publishToAgent(notif.toJson());
}

void AgentClient::publishToAgent(const nlohmann::json &message) {
    if (!m_mqttClient || !m_mqttClient->is_connected()) {
        emit errorOccurred(QStringLiteral(u"MQTT 未连接"));
        return;
    }

    std::string topic = "$agent/" + m_agentId + "/" + m_clientId;
    std::string payload = message.dump();

    qDebug() << "Publishing to" << topic.c_str() << ":" << payload.c_str();

    try {
        m_mqttClient->publish(topic, payload, 1, false);
    } catch (const mqtt::exception &e) {
        emit errorOccurred(QString("发送消息失败: %1").arg(e.what()));
    }
}
