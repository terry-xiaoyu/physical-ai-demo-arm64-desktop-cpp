#include <QMouseEvent>
#include "RoomMainWidget.h"
#include "LoginWidget.h"
#include "OperateWidget.h"
#include "AgentClient.h"
#include <QDebug>
#include <vector>
#include <QTimer>
#include "VideoWidget.h"
#include "ChatWidget.h"
#include <QMessageBox>

/**
 * VolcEngineRTC 视频通话的主页面
 * 本示例不限制房间内最大用户数；同时最多渲染四个用户的视频数据（自己和三个远端用户视频数据）；
 *
 * 包含如下简单功能：
 * - 创建引擎
 * - 设置视频发布参数
 * - 渲染自己的视频数据
 * - 创建房间
 * - 加入音视频通话房间
 * - 打开/关闭麦克风
 * - 打开/关闭摄像头
 * - 渲染远端用户的视频数据
 * - 离开房间
 * - 销毁引擎
 *
 * 实现一个基本的音视频通话的流程如下：
 * 1.创建 IRTCVideo 实例。 bytertc::IRTCVideo* bytertc::createRTCVideo(
 *                          const char* app_id,
 *                          bytertc::IRTCVideoEventHandler *event_handler,
 *                          const char* parameters)
 * 2.视频发布端设置推送多路流时各路流的参数，包括分辨率、帧率、码率、缩放模式、网络不佳时的回退策略等。
 *   bytertc::IRTCVideo::setVideoEncoderConfig(
 *       const VideoEncoderConfig& max_solution)
 * 3.开启本地视频采集。 bytertc::IRTCVideo::startVideoCapture()
 * 4.设置本地视频渲染时，使用的视图，并设置渲染模式。
 *   bytertc::IRTCVideo::setLocalVideoCanvas(
 *       StreamIndex index,
 *       const VideoCanvas& canvas)
 * 5.创建房间。IRTCRoom* bytertc::IRTCVideo::createRTCRoom(
 *               const char* room_id)
 * 6.加入音视频通话房间。bytertc::IRTCRoom::joinRoom(
 *                        const char* token,
 *                        const UserInfo& user_info,
 *                        const RTCRoomConfig& config)
 * 7.SDK 接收并解码远端视频流首帧后，设置用户的视频渲染视图。
 *   bytertc::IRTCVideo::setRemoteStreamVideoCanvas(
 *       RemoteStreamKey stream_key,
 *       const VideoCanvas& canvas)
 * 8.在用户离开房间之后移出用户的视频渲染视图。
 * 9.离开音视频通话房间。bytertc::IRTCRoom::leaveRoom()
 * 10.销毁房间。bytertc::IRTCRoom::destroy()
 * 11.销毁引擎。bytertc::destroyRTCVideo();
 *
 * 详细的API文档参见: https://www.volcengine.com/docs/6348/85515
 */

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
    m_operateWidget = QSharedPointer<OperateWidget>::create(this);

    // 创建灯指示器（圆形控件，通过背景色表示开/关状态）
    m_lightIndicator = new QWidget(ui.mainWidget);
    m_lightIndicator->setObjectName("lightIndicator");
    m_lightIndicator->setFixedSize(50, 50);
    m_lightIndicator->move(15, 15);
    m_lightIndicator->setAttribute(Qt::WA_StyledBackground, true);
    m_lightIndicator->setStyleSheet(
        "QWidget#lightIndicator {"
        "  background-color: #555555;"
        "  border-radius: 25px;"
        "  border: 3px solid #777777;"
        "}");
    m_lightIndicator->setToolTip(QStringLiteral(u"灯"));
    m_lightIndicator->raise();
    m_lightIndicator->setVisible(false);

    // 创建文本聊天面板（浮动在窗口右侧）
    m_chatWidget = QSharedPointer<ChatWidget>::create(this);
    m_chatWidget->setVisible(false);
    toggleShowFloatWidget(false);
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
    m_bLeftBtnPressed = false;
}

void RoomMainWidget::slotOnStartVoiceChat(const QString &brokerUrl, const QString &agentId, const QString &clientId) {
    toggleShowFloatWidget(true);

    m_agentClient = new AgentClient(this);

    connect(m_agentClient, &AgentClient::voiceChatReady,
            this, &RoomMainWidget::slotOnVoiceChatReady);

    connect(m_agentClient, &AgentClient::voiceChatStopped,
            this, &RoomMainWidget::slotOnHangup);

    connect(m_agentClient, &AgentClient::lightStateChanged,
            this, [this](bool on) {
        if (m_lightIndicator) {
            if (on) {
                m_lightIndicator->setStyleSheet(
                    "QWidget#lightIndicator {"
                    "  background-color: #FFD700;"
                    "  border-radius: 25px;"
                    "  border: 3px solid #FFA500;"
                    "}");
            } else {
                m_lightIndicator->setStyleSheet(
                    "QWidget#lightIndicator {"
                    "  background-color: #555555;"
                    "  border-radius: 25px;"
                    "  border: 3px solid #777777;"
                    "}");
            }
        }
    });

    connect(m_agentClient, &AgentClient::errorOccurred,
            this, [this](const QString &error) {
        QMessageBox::warning(this, QStringLiteral(u"错误"), error, QStringLiteral(u"确定"));
        if (!m_isInRoom) {
            toggleShowFloatWidget(false);
            if (m_agentClient) {
                m_agentClient->deleteLater();
                m_agentClient = nullptr;
            }
        }
    });

    // 文本聊天信号连接（先断开旧连接，防止重复）
    disconnect(m_chatWidget.get(), &ChatWidget::sigSendText, nullptr, nullptr);
    connect(m_chatWidget.get(), &ChatWidget::sigSendText,
            this, [this](const QString &text) {
        if (m_agentClient && m_agentClient->isConnected()) {
            m_agentClient->sendTextTalk(text);
        }
    });

    connect(m_agentClient, &AgentClient::textDeltaReceived,
            this, [this](const QString &delta) {
        m_chatWidget->appendAgentDelta(delta);
    });

    connect(m_agentClient, &AgentClient::textFinished,
            this, [this]() {
        m_chatWidget->finishAgentMessage();
    });

    m_agentClient->start(brokerUrl, agentId, clientId);
}

void RoomMainWidget::slotOnVoiceChatReady(const QString &appId, const QString &roomId,
                                           const QString &token, const QString &userId,
                                           const QString &targetUserId) {
    // 使用服务端返回的 targetUserId 作为本端的 userId
    m_uid = targetUserId.toStdString();
    m_roomId = roomId.toStdString();
    m_appId = appId.toStdString();
    std::string tokenStr = token.toStdString();

    ui.roomIdLabel->setText(roomId);

    // 创建引擎（使用服务端返回的 appId）
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
    // 设置视频发布参数
    m_rtc_video->setVideoEncoderConfig(conf);

    // 如果是空的stream_id，RTC会自动生成
    std::string stream_id = "";

    setRenderCanvas(true, (void *) ui.localWidget->getVideoWidget()->winId(), stream_id, m_uid);
    ui.localWidget->showVideo(m_uid.c_str());
    // 开启本地视频采集
    m_rtc_video->startVideoCapture();
    // 开启本地音频采集
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
    // 加入房间（使用服务端返回的 token，targetUserId 作为本端 uid）
    m_rtc_room->joinRoom(tokenStr.c_str(), userInfo, true, roomConfig);
    m_isInRoom = true;

    qDebug() << "joinRoom: appId=" << m_appId.c_str()
             << ", roomId=" << m_roomId.c_str()
             << ", uid(targetUserId)=" << m_uid.c_str();
}

void RoomMainWidget::toggleShowFloatWidget(bool isEnterRoom) {
    m_loginWidget->setVisible(!isEnterRoom);
    m_operateWidget->setVisible(isEnterRoom);
    ui.roomIdLabel->setVisible(isEnterRoom);
    m_lightIndicator->setVisible(isEnterRoom);
    m_chatWidget->setVisible(isEnterRoom);
}

void RoomMainWidget::slotOnHangup() {
    // 先置标志位，防止排队中的跨线程信号在引擎销毁后继续操作 SDK
    m_isInRoom = false;

    toggleShowFloatWidget(false);

    // 重置灯指示器为关闭状态
    if (m_lightIndicator) {
        m_lightIndicator->setStyleSheet(
            "QWidget#lightIndicator {"
            "  background-color: #555555;"
            "  border-radius: 25px;"
            "  border: 3px solid #777777;"
            "}");
    }

    // 清空聊天记录
    m_chatWidget->clearChat();

    // 停止 MQTT 智能体客户端（发送 stopVoiceChat + destroySession）
    if (m_agentClient) {
        m_agentClient->stop();
        m_agentClient->deleteLater();
        m_agentClient = nullptr;
    }

    if (m_rtc_room) {
        // 先摘除回调，阻止 SDK 线程继续派发新的事件到 this
        m_rtc_room->setRTCRoomEventHandler(nullptr);
        // 离开房间
        m_rtc_room->leaveRoom();
        m_rtc_room->destroy();
        m_rtc_room = nullptr;
    }
    // 销毁引擎
    bytertc::IRTCEngine::destroyRTCEngine();
    m_rtc_video = nullptr;
    if (m_operateWidget)
    {
        m_operateWidget->reset();
    }

    clearVideoView();
}

void RoomMainWidget::onRoomStateChanged(
            const char* room_id, const char* uid, int state, const char* extra_info){
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
        // 设置本地视频渲染视图
        m_rtc_video->setLocalVideoCanvas(canvas);
    } else {
        // 设置远端用户视频渲染视图
        // bytertc::RemoteStreamKey remote_stream_key;
        // remote_stream_key.room_id = m_roomId.c_str();
        // remote_stream_key.user_id = user_id.c_str();
        // remote_stream_key.stream_index = bytertc::kStreamIndexMain;
        m_rtc_video->setRemoteVideoCanvas(stream_id.c_str(), canvas);
    }
}

void RoomMainWidget::setupSignals() {
    connect(this, &RoomMainWidget::sigUserEnter, this, [=](const QString &streamID, const QString &userID) {
        // 引擎已销毁或正在销毁，丢弃来自 SDK 线程的排队信号
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

    connect(m_operateWidget.get(), &OperateWidget::sigMuteAudio, this, [this](bool bMute) {
        if (m_rtc_room) {
            if (bMute) {
                // 关闭本地音频发送
                m_rtc_room->publishStreamAudio(false);
            }
            else {
                // 开启本地音频发送
                m_rtc_room->publishStreamAudio(true);
            }
        }
    });

    connect(m_operateWidget.get(), &OperateWidget::sigMuteVideo, this, [this](bool bMute) {
        if (m_rtc_video) {
            if (bMute) {
                // 关闭视频采集
                m_rtc_video->stopVideoCapture();
            } else {
                // 开启视频采集
                m_rtc_video->startVideoCapture();
            }
            QTimer::singleShot(10, this, [=] {
                ui.localWidget->update();
            });
        }
    });

    connect(this, &RoomMainWidget::sigError, this, [this](int errorCode) {
        QString errorInfo = "error:";
        errorInfo += QString::number(errorCode);
        QMessageBox::warning(this, QStringLiteral(u"提示"),errorInfo ,QStringLiteral(u"确定"));
    });
}

void RoomMainWidget::clearVideoView() {
    ui.localWidget->hideVideo();
    for (auto uid : m_activeWidgetMap.keys()) {
        m_activeWidgetMap[uid]->hideVideo();
    }
    m_activeWidgetMap.clear();
}
