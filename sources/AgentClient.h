#pragma once

#include <QObject>
#include <QString>
#include <QDebug>
#include <memory>
#include <string>

#include <mqtt/async_client.h>
#include <mcp_mqtt/json_rpc.h>
#include <mcp_mqtt/mcp_server.h>
#include <mcp_mqtt/mqtt_interface.h>

/**
 * MQTT 智能体客户端
 *
 * 封装与智能体的 MQTT 通信协议，实现以下流程：
 * 1. 连接 MQTT Broker
 * 2. 订阅智能体回复主题 $agent-client/{clientId}/#
 * 3. 发送 initializeSession 初始化会话
 * 4. 发送 startVoiceChat 发起语音会话
 * 5. 从应答中提取 RTC 加入房间所需参数
 *
 * 同时作为 MCP 服务器，注册工具供智能体调用（如灯控制工具）。
 *
 * 参考协议文档：specs/client_agent_message_protocol.md
 */
class AgentClient : public QObject {
    Q_OBJECT

public:
    explicit AgentClient(QObject *parent = nullptr);
    ~AgentClient() override;

    /**
     * 启动完整流程：连接 Broker → initializeSession → startVoiceChat
     * 最终通过 voiceChatReady 信号返回 RTC 房间参数
     */
    void start(const QString &brokerUrl,
               const QString &agentId,
               const QString &clientId);

    /**
     * 停止：发送 stopVoiceChat + destroySession，断开 MQTT
     */
    void stop();

    /**
     * 向智能体发送文本消息（textTalk 通知）
     */
    void sendTextTalk(const QString &text);

    bool isConnected() const;

signals:
    void voiceChatReady(const QString &appId, const QString &roomId,
                        const QString &token, const QString &userId,
                        const QString &targetUserId);
    void voiceChatStopped();
    void errorOccurred(const QString &error);
    void lightStateChanged(bool on);
    void textDeltaReceived(const QString &delta);
    void textFinished();

private slots:
    void handleMessage(const QString &topic, const QString &payload);
    void handleConnectionLost(const QString &reason);

private:
    class MqttCallbackBridge;
    class McpMqttAdapter;

    void sendInitializeSession();
    void sendStartVoiceChat();
    void sendStopVoiceChat();
    void sendDestroySession();
    void publishToAgent(const nlohmann::json &message);
    void setupMcpServer();

    std::unique_ptr<mqtt::async_client> m_mqttClient;
    std::unique_ptr<MqttCallbackBridge> m_callbackBridge;
    std::unique_ptr<McpMqttAdapter> m_mcpAdapter;
    mcp_mqtt::McpServer m_mcpServer;

    std::string m_agentId;
    std::string m_clientId;
    int64_t m_nextRequestId = 1;

    std::string m_initSessionId;
    std::string m_startVoiceChatId;
    std::string m_stopVoiceChatId;
    int64_t m_nextTaskId = 1;
};
