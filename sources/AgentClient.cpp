#include "AgentClient.h"
#include <QMetaObject>
#include <chrono>

// ── 内部 MQTT 回调桥接类 ───────────────────────────────────────────
// Paho 的回调运行在内部线程上，通过 QMetaObject::invokeMethod
// 将消息安全地转发到 Qt 主线程处理。
class AgentClient::MqttCallbackBridge : public mqtt::callback {
public:
    explicit MqttCallbackBridge(AgentClient *owner) : m_owner(owner) {}

    void message_arrived(mqtt::const_message_ptr msg) override {
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
            sendStopVoiceChat();
            sendDestroySession();
            m_mqttClient->disconnect()->wait_for(std::chrono::seconds(2));
        } catch (...) {}
    }
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
        }

    } catch (const std::exception &e) {
        qWarning() << "Failed to parse MQTT message:" << e.what();
    }
}

void AgentClient::handleConnectionLost(const QString &reason) {
    qWarning() << "MQTT connection lost:" << reason;
    emit errorOccurred(QStringLiteral(u"MQTT 连接断开: ") + reason);
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
