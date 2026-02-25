#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_LoginWidget.h"

class LoginWidget : public QWidget {
    Q_OBJECT

public:
    LoginWidget(QWidget *parent = Q_NULLPTR);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    //void paintEvent(QPaintEvent *event)override;
private
    slots:
            void on_startVoiceChatBtn_clicked();
    signals:
            void sigStartVoiceChat(const QString &brokerUrl, const QString &agentId, const QString &clientId);
private:
    Ui::LoginForm ui;
};
