#pragma once

#include <QWidget>

class QTextEdit;
class QLineEdit;
class QPushButton;

/**
 * 文本聊天面板
 *
 * 浮动在主窗口右侧，提供与智能体的文本对话功能：
 * - 输入框 + 发送按钮，发送文本消息
 * - 显示区域，展示用户消息和智能体的流式回复
 */
class ChatWidget : public QWidget {
    Q_OBJECT

public:
    explicit ChatWidget(QWidget *parent = nullptr);

    void appendUserMessage(const QString &text);
    void appendAgentDelta(const QString &delta);
    void finishAgentMessage();
    void clearChat();

signals:
    void sigSendText(const QString &text);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onSendClicked();

private:
    QTextEdit *m_chatDisplay;
    QLineEdit *m_inputField;
    QPushButton *m_sendBtn;
    bool m_agentMessageInProgress = false;
};
