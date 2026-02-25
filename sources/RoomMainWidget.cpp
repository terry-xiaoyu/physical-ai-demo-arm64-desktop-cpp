#include <QMouseEvent>
#include "RoomMainWidget.h"
#include "LoginWidget.h"
#include "AgentClient.h"
#include <QDebug>
#include <vector>
#include <QTimer>
#include "VideoWidget.h"
#include <QMessageBox>
#include <QTextCursor>

RoomMainWidget::RoomMainWidget(QWidget *parent)
        : QWidget(parent) {
    ui.setupUi(this);
    setWindowFlags(Qt::FramelessWindowHint | windowFlags());

    setupView();
    setupSignals();
}

void RoomMainWidget::leaveRoom() {
}

void RoomMainWidget::setupView() {
    m_videoWidgetList.append(ui.remoteWidget1);
    m_videoWidgetList.append(ui.remoteWidget2);
    m_videoWidgetList.append(ui.remoteWidget3);

    m_loginWidget = QSharedPointer<LoginWidget>::create(this);

    // lightDot needs WA_StyledBackground for stylesheet to work
    ui.lightDot->setAttribute(Qt::WA_StyledBackground, true);

    toggleCallUI(false);
    ui.sdkVersionLabel->setText(QStringLiteral(u"VolcEngineRTC v") + QString(bytertc::IRTCEngine::getSDKVersion()));
}

void RoomMainWidget::on_closeBtn_clicked() {
    qDebug() << "receive close event";
    slotOnHangup();
    close();
}

void RoomMainWidget::mousePressEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton && ui.titleWidget->rect().contains(event->pos())) {
        m_prevGlobalPoint = event->globalPos();
        m_bLeftBtnPressed = true;
    }
}

void RoomMainWidget::mouseMoveEvent(QMouseEvent *event) {
    if (m_bLeftBtnPressed) {
        auto offset = event->globalPos() - m_prevGlobalPoint;
        m_prevGlobalPoint = event->globalPos();
        move(pos() + offset);
    }
}

void RoomMainWidget::mouseReleaseEvent(QMouseEvent *event) {
    Q_UNUSED(event);
    m_bLeftBtnPressed = false;
}

void RoomMainWidget::slotOnStartVoiceChat(const QString &brokerUrl, const QString &agentId, const QString &clientId) {
    toggleCallUI(true);

    m_agentClient = new AgentClient(this);

    connect(m_agentClient, &AgentClient::voiceChatReady,
            this, &RoomMainWidget::slotOnVoiceChatReady);

    connect(m_agentClient, &AgentClient::voiceChatStopped,
            this, &RoomMainWidget::slotOnHangup);

    connect(m_agentClient, &AgentClient::lightStateChanged,
            this, &RoomMainWidget::setLightState);

    connect(m_agentClient, &AgentClient::errorOccurred,
            this, [this](const QString &error) {
        QMessageBox::warning(this, QStringLiteral(u"错误"), error, QStringLiteral(u"确定"));
        if (!m_isInRoom) {
            toggleCallUI(false);
            if (m_agentClient) {
                m_agentClient->deleteLater();
                m_agentClient = nullptr;
            }
        }
    });

    connect(m_agentClient, &AgentClient::textDeltaReceived,
            this, &RoomMainWidget::appendAgentDelta);

    connect(m_agentClient, &AgentClient::textFinished,
            this, &RoomMainWidget::finishAgentMessage);

    m_agentClient->start(brokerUrl, agentId, clientId);
}

void RoomMainWidget::slotOnVoiceChatReady(const QString &appId, const QString &roomId,
                                           const QString &token, const QString &userId,
                                           const QString &targetUserId) {
    m_uid = targetUserId.toStdString();
    m_roomId = roomId.toStdString();
    m_appId = appId.toStdString();
    std::string tokenStr = token.toStdString();

    ui.roomIdLabel->setText(roomId);

    bytertc::EngineConfig config;
    config.app_id = m_appId.c_str();
    config.parameters = "";
    m_rtc_video = bytertc::IRTCEngine::createRTCEngine(config, this);
    if (m_rtc_video == nullptr) {
        qWarning() << "create engine failed";
        return;
    }

    bytertc::VideoEncoderConfig conf;
    conf.frame_rate = 15;
    conf.width = 360;
    conf.height = 640;
    m_rtc_video->setVideoEncoderConfig(conf);

    std::string stream_id = "";

    setRenderCanvas(true, (void *) ui.localWidget->getVideoWidget()->winId(), stream_id, m_uid);
    ui.localWidget->showVideo(m_uid.c_str());
    m_rtc_video->startVideoCapture();
    m_rtc_video->startAudioCapture();

    m_rtc_room = m_rtc_video->createRTCRoom(m_roomId.c_str());
    m_rtc_room->setRTCRoomEventHandler(this);
    bytertc::UserInfo userInfo;
    userInfo.uid = m_uid.c_str();
    userInfo.extra_info = nullptr;

    bytertc::RTCRoomConfig roomConfig;
    roomConfig.stream_id = stream_id.c_str();
    roomConfig.is_auto_publish_audio = true;
    roomConfig.is_auto_publish_video = true;
    roomConfig.is_auto_subscribe_audio = true;
    roomConfig.is_auto_subscribe_video = true;
    roomConfig.room_profile_type = bytertc::kRoomProfileTypeCommunication;
    m_rtc_room->joinRoom(tokenStr.c_str(), userInfo, true, roomConfig);
    m_isInRoom = true;

    qDebug() << "joinRoom: appId=" << m_appId.c_str()
             << ", roomId=" << m_roomId.c_str()
             << ", uid(targetUserId)=" << m_uid.c_str();
}

void RoomMainWidget::toggleCallUI(bool inCall) {
    m_loginWidget->setVisible(!inCall);
    ui.sidePanel->setVisible(inCall);
    ui.controlBar->setVisible(inCall);
    ui.roomIdLabel->setVisible(inCall);
}

void RoomMainWidget::slotOnHangup() {
    m_isInRoom = false;

    toggleCallUI(false);
    setLightState(false);
    clearChat();

    if (m_agentClient) {
        m_agentClient->stop();
        m_agentClient->deleteLater();
        m_agentClient = nullptr;
    }

    if (m_rtc_room) {
        m_rtc_room->setRTCRoomEventHandler(nullptr);
        m_rtc_room->leaveRoom();
        m_rtc_room->destroy();
        m_rtc_room = nullptr;
    }
    bytertc::IRTCEngine::destroyRTCEngine();
    m_rtc_video = nullptr;

    // Reset mute buttons
    ui.muteAudioBtn->blockSignals(true);
    ui.muteAudioBtn->setChecked(false);
    ui.muteAudioBtn->blockSignals(false);

    ui.muteVideoBtn->blockSignals(true);
    ui.muteVideoBtn->setChecked(false);
    ui.muteVideoBtn->blockSignals(false);

    clearVideoView();
}

void RoomMainWidget::onRoomStateChanged(
            const char* room_id, const char* uid, int state, const char* extra_info) {
    qDebug() << "onRoomStateChanged,roomid:" << room_id << ",uid:" << uid << ",state:" << state;
}

void RoomMainWidget::onError(int err) {
    qDebug() << "bytertc::OnError err" << err;
    emit sigError(err);
}

void RoomMainWidget::onUserJoined(const bytertc::UserInfo &user_info) {
    qDebug() << "bytertc::OnUserJoined " << user_info.uid;
}

void RoomMainWidget::onUserLeave(const char *uid, bytertc::UserOfflineReason reason) {
    qDebug() << "user leave id = " << uid;
    emit sigUserLeave(uid);
}

void RoomMainWidget::onFirstLocalVideoFrameCaptured(bytertc::IVideoSource* video_source, const bytertc::VideoFrameInfo& info) {
    qDebug() << "first local video frame is rendered";
}

void RoomMainWidget::onFirstRemoteVideoFrameDecoded(const char* stream_id, const bytertc::StreamInfo& stream_info, const bytertc::VideoFrameInfo& info) {
    qDebug() << "first remote video frame is decoded stream_id = " << stream_id << ", user_id = " << stream_info.user_id;
    emit sigUserEnter(stream_id, stream_info.user_id);
}

void RoomMainWidget::setRenderCanvas(bool isLocal, void *view, const std::string &stream_id, const std::string &user_id) {
    if (m_rtc_video == nullptr) {
        qDebug() << "byte engine is null ptr";
        return;
    }

    bytertc::VideoCanvas canvas;
    canvas.view = view;
    canvas.render_mode = bytertc::RenderMode::kRenderModeFit;

    if (isLocal) {
        m_rtc_video->setLocalVideoCanvas(canvas);
    } else {
        m_rtc_video->setRemoteVideoCanvas(stream_id.c_str(), canvas);
    }
}

void RoomMainWidget::setupSignals() {
    connect(this, &RoomMainWidget::sigUserEnter, this, [=](const QString &streamID, const QString &userID) {
        if (!m_isInRoom) {
            qDebug() << "ignore sigUserEnter after leaving room";
            return;
        }

        if (m_activeWidgetMap.size() >= 3) {
            qDebug() << "ignore add stream event,for users above 4";
            return;
        }

        if (m_activeWidgetMap.contains(userID)) {
            qDebug() << "user id = " << userID << " is exist";
            return;
        }

        for (int i = 0; i < m_videoWidgetList.size(); i++) {
            if (!m_videoWidgetList[i]->isActive()) {
                m_activeWidgetMap[userID] = m_videoWidgetList[i];
                m_videoWidgetList[i]->showVideo(userID);
                setRenderCanvas(false, (void *) m_videoWidgetList[i]->getVideoWidget()->winId(), streamID.toStdString(), userID.toStdString());
                break;
            }
        }
    });

    connect(this, &RoomMainWidget::sigUserLeave, this, [=](const QString &userID) {
        if (!m_isInRoom) {
            qDebug() << "ignore sigUserLeave after leaving room";
            return;
        }

        if (m_activeWidgetMap.contains(userID)) {
            auto videoView = m_activeWidgetMap[userID];
            videoView->hideVideo();
            m_activeWidgetMap.remove(userID);
        } else {
            qDebug() << "ignore user leave event";
        }
    });

    connect(this, &RoomMainWidget::sigError, this, [this](int errorCode) {
        QString errorInfo = "error:";
        errorInfo += QString::number(errorCode);
        QMessageBox::warning(this, QStringLiteral(u"提示"), errorInfo, QStringLiteral(u"确定"));
    });

    // Enter key in input field triggers send
    connect(ui.inputField, &QLineEdit::returnPressed, this, &RoomMainWidget::on_sendBtn_clicked);
}

void RoomMainWidget::clearVideoView() {
    ui.localWidget->hideVideo();
    for (auto uid : m_activeWidgetMap.keys()) {
        m_activeWidgetMap[uid]->hideVideo();
    }
    m_activeWidgetMap.clear();
}

// --- Control bar slots ---

void RoomMainWidget::on_hangupBtn_clicked() {
    slotOnHangup();
}

void RoomMainWidget::on_muteAudioBtn_clicked() {
    bool bMute = ui.muteAudioBtn->isChecked();
    if (m_rtc_room) {
        m_rtc_room->publishStreamAudio(!bMute);
    }
}

void RoomMainWidget::on_muteVideoBtn_clicked() {
    bool bMute = ui.muteVideoBtn->isChecked();
    if (m_rtc_video) {
        if (bMute) {
            m_rtc_video->stopVideoCapture();
        } else {
            m_rtc_video->startVideoCapture();
        }
        QTimer::singleShot(10, this, [=] {
            ui.localWidget->update();
        });
    }
}

// --- Chat methods ---

void RoomMainWidget::on_sendBtn_clicked() {
    QString text = ui.inputField->text().trimmed();
    if (text.isEmpty()) return;

    appendUserMessage(text);
    if (m_agentClient && m_agentClient->isConnected()) {
        m_agentClient->sendTextTalk(text);
    }
    ui.inputField->clear();
}

void RoomMainWidget::appendUserMessage(const QString &text) {
    QTextCursor cursor = ui.chatDisplay->textCursor();
    cursor.movePosition(QTextCursor::End);
    if (!ui.chatDisplay->document()->isEmpty()) {
        cursor.insertText("\n");
    }
    cursor.insertText(QStringLiteral(u"You: ") + text);
    ui.chatDisplay->setTextCursor(cursor);
    ui.chatDisplay->ensureCursorVisible();
}

void RoomMainWidget::appendAgentDelta(const QString &delta) {
    QTextCursor cursor = ui.chatDisplay->textCursor();
    cursor.movePosition(QTextCursor::End);
    if (!m_agentMessageInProgress) {
        if (!ui.chatDisplay->document()->isEmpty()) {
            cursor.insertText("\n");
        }
        cursor.insertText("Agent: ");
        m_agentMessageInProgress = true;
    }
    cursor.insertText(delta);
    ui.chatDisplay->setTextCursor(cursor);
    ui.chatDisplay->ensureCursorVisible();
}

void RoomMainWidget::finishAgentMessage() {
    m_agentMessageInProgress = false;
}

void RoomMainWidget::clearChat() {
    ui.chatDisplay->clear();
    m_agentMessageInProgress = false;
}

// --- Light helper ---

void RoomMainWidget::setLightState(bool on) {
    if (on) {
        ui.lightDot->setStyleSheet(
            "QWidget#lightDot {"
            "  background-color: #FFD700;"
            "  border-radius: 6px;"
            "}");
        ui.lightLabel->setText(QStringLiteral(u"灯: 开启"));
    } else {
        ui.lightDot->setStyleSheet(
            "QWidget#lightDot {"
            "  background-color: #555555;"
            "  border-radius: 6px;"
            "}");
        ui.lightLabel->setText(QStringLiteral(u"灯: 关闭"));
    }
}
