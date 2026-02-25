#include "ChatWidget.h"
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QEvent>
#include <QTextCursor>

ChatWidget::ChatWidget(QWidget *parent)
    : QWidget(parent) {
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | windowFlags());
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedWidth(260);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(6);

    // 聊天显示区域
    m_chatDisplay = new QTextEdit(this);
    m_chatDisplay->setReadOnly(true);
    m_chatDisplay->setStyleSheet(
        "QTextEdit {"
        "  background-color: #2B2F35;"
        "  color: #DDDDDD;"
        "  border: none;"
        "  border-radius: 8px;"
        "  padding: 8px;"
        "  font-size: 13px;"
        "  font-family: Arial, sans-serif;"
        "}");
    mainLayout->addWidget(m_chatDisplay, 1);

    // 输入区域
    auto *inputLayout = new QHBoxLayout();
    inputLayout->setSpacing(6);

    m_inputField = new QLineEdit(this);
    m_inputField->setPlaceholderText(QStringLiteral(u"输入消息..."));
    m_inputField->setStyleSheet(
        "QLineEdit {"
        "  background-color: #2B2F35;"
        "  color: #FFFFFF;"
        "  border: 1px solid #555555;"
        "  border-radius: 6px;"
        "  padding: 6px 8px;"
        "  font-size: 13px;"
        "}"
        "QLineEdit:focus {"
        "  border: 1px solid #4A90D9;"
        "}");
    inputLayout->addWidget(m_inputField);

    m_sendBtn = new QPushButton(QStringLiteral(u"发送"), this);
    m_sendBtn->setFixedWidth(50);
    m_sendBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #4A90D9;"
        "  color: #FFFFFF;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 6px;"
        "  font-size: 13px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #5BA0E9;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #3A80C9;"
        "}");
    inputLayout->addWidget(m_sendBtn);

    mainLayout->addLayout(inputLayout);

    parent->installEventFilter(this);

    connect(m_sendBtn, &QPushButton::clicked, this, &ChatWidget::onSendClicked);
    connect(m_inputField, &QLineEdit::returnPressed, this, &ChatWidget::onSendClicked);
}

void ChatWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor(54, 57, 63, 230));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(rect(), 10, 10);
}

bool ChatWidget::eventFilter(QObject *watched, QEvent *event) {
    if (watched == parent()) {
        auto parentWindow = dynamic_cast<QWidget *>(parent());
        if (!parentWindow) return false;

        if (event->type() == QEvent::Resize || event->type() == QEvent::Move) {
            auto parentGeo = parentWindow->geometry();
            int x = parentGeo.right() - width() - 8;
            int y = parentGeo.top() + 65;
            int h = parentGeo.height() - 75;
            setGeometry(x, y, width(), qMax(h, 200));
        }
    }
    return false;
}

void ChatWidget::onSendClicked() {
    QString text = m_inputField->text().trimmed();
    if (text.isEmpty()) return;

    appendUserMessage(text);
    emit sigSendText(text);
    m_inputField->clear();
}

void ChatWidget::appendUserMessage(const QString &text) {
    QTextCursor cursor = m_chatDisplay->textCursor();
    cursor.movePosition(QTextCursor::End);
    if (!m_chatDisplay->document()->isEmpty()) {
        cursor.insertText("\n");
    }
    cursor.insertText(QStringLiteral(u"You: ") + text);
    m_chatDisplay->setTextCursor(cursor);
    m_chatDisplay->ensureCursorVisible();
}

void ChatWidget::appendAgentDelta(const QString &delta) {
    QTextCursor cursor = m_chatDisplay->textCursor();
    cursor.movePosition(QTextCursor::End);
    if (!m_agentMessageInProgress) {
        if (!m_chatDisplay->document()->isEmpty()) {
            cursor.insertText("\n");
        }
        cursor.insertText("Agent: ");
        m_agentMessageInProgress = true;
    }
    cursor.insertText(delta);
    m_chatDisplay->setTextCursor(cursor);
    m_chatDisplay->ensureCursorVisible();
}

void ChatWidget::finishAgentMessage() {
    m_agentMessageInProgress = false;
}

void ChatWidget::clearChat() {
    m_chatDisplay->clear();
    m_agentMessageInProgress = false;
}
