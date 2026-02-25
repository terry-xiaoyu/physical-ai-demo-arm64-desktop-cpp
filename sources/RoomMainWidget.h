#pragma once

#include <QtWidgets/QMainWindow>
#include <QSharedPointer>
#include "ui_RoomMainWidget.h"
#include "bytertc_engine.h"
#include "bytertc_room.h"
#include "bytertc_room_event_handler.h"

class LoginWidget;
class OperateWidget;
class AgentClient;

class RoomMainWidget : public QWidget, public bytertc::IRTCRoomEventHandler, public bytertc::IRTCEngineEventHandler {
    Q_OBJECT

public:
    RoomMainWidget(QWidget *parent = Q_NULLPTR);

private
    slots:
            void on_closeBtn_clicked();
protected:
    void mousePressEvent(QMouseEvent *event) override;

    void mouseMoveEvent(QMouseEvent *event) override;

    void mouseReleaseEvent(QMouseEvent *event) override;

protected:
    void onRoomStateChanged(
            const char* room_id, const char* uid, int state, const char* extra_info) override;
    void onError(int err) override;

    void onUserJoined(const bytertc::UserInfo &user_info) override;

    void onUserLeave(const char *uid, bytertc::UserOfflineReason reason) override;

    void onFirstLocalVideoFrameCaptured(bytertc::IVideoSource* video_source, const bytertc::VideoFrameInfo& info) override;

    void onFirstRemoteVideoFrameDecoded(const char* stream_id, const bytertc::StreamInfo& stream_info, const bytertc::VideoFrameInfo& info) override;

public
    slots:
            void slotOnStartVoiceChat(
    const QString &brokerUrl,
    const QString &agentId,
    const QString &clientId
    );

    void slotOnVoiceChatReady(
    const QString &appId,
    const QString &roomId,
    const QString &token,
    const QString &userId,
    const QString &targetUserId
    );

    void slotOnHangup();

    signals:
            void sigJoinChannelSuccess(std::string
    channel,
    std::string uid,
    int elapsed
    );

    void sigJoinChannelFailed(std::string room_id, std::string uid, int error_code);

    void sigRoomJoinChannelSuccess(std::string channel, std::string uid, int elapsed);

    void sigUserEnter(const QString &stream_id, const QString &uid);

    void sigUserLeave(const QString &uid);

    void sigError(int errorCode);
private:
    void setupView();

    void setupSignals();

    void toggleShowFloatWidget(bool isEnterRoom);

    void leaveRoom();

    void setRenderCanvas(bool isLocal, void *view, const std::string &stream_id, const std::string &id);

    void clearVideoView();

private:
    Ui::RoomMainForm ui;
    bool m_bLeftBtnPressed = false;
    QPoint m_prevGlobalPoint;
    QSharedPointer <LoginWidget> m_loginWidget;
    QSharedPointer <OperateWidget> m_operateWidget;
    AgentClient *m_agentClient = nullptr;
    bytertc::IRTCEngine* m_rtc_video = nullptr;
    bytertc::IRTCRoom* m_rtc_room = nullptr;
    std::string m_appId;
    std::string m_uid;
    std::string m_roomId;
    bool m_isInRoom = false;
    QList<VideoWidget *> m_videoWidgetList;
    QMap<QString, VideoWidget *> m_activeWidgetMap;
};
