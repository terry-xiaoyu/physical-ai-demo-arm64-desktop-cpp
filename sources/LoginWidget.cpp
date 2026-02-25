#include "LoginWidget.h"
#include <QMouseEvent>
#include <QPainter>
#include <QDebug>
#include <QMessageBox>

/**
 * MQTT 配置与语音通话入口页面
 *
 * 包含如下功能：
 * - 输入 MQTT Broker 地址、智能体 ID、客户端 ID
 * - 校验输入项非空
 * - 点击按钮后通过 MQTT 发起语音通话
 */

LoginWidget::LoginWidget(QWidget *parent)
    : QWidget(parent)
{
    ui.setupUi(this);
	setAttribute(Qt::WA_StyledBackground);
	parent->installEventFilter(this);

	connect(this, SIGNAL(sigStartVoiceChat(const QString &, const QString &, const QString &)),
	        parent, SLOT(slotOnStartVoiceChat(const QString &, const QString &, const QString &)));
}

bool LoginWidget::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == parent())
	{
		auto parentWindow = dynamic_cast<QWidget*>(parent());
		if (parentWindow == nullptr)
		{
			return false;
		}
		if (event->type() == QEvent::Resize)
		{
			//update login geometry
			auto selfRect = this->rect();
			auto parentGem = parentWindow->rect();
			selfRect.moveCenter(parentGem.center());
			setGeometry(selfRect);
		}
	}
	return false;
}

void LoginWidget::on_startVoiceChatBtn_clicked()
{
	auto checkNotEmpty = [=](const QString &typeName, const QString &str) -> bool
	{
		if (str.trimmed().isEmpty())
		{
			QMessageBox::warning(this, QStringLiteral(u"提示"),
				typeName + QStringLiteral(u"不能为空！"),
				QStringLiteral(u"确定"));
			return false;
		}
		return true;
	};

	if (!checkNotEmpty(QStringLiteral(u"Broker 地址"), ui.brokerUrlLineEdit->text()))
		return;

	if (!checkNotEmpty(QStringLiteral(u"Agent ID"), ui.agentIdLineEdit->text()))
		return;

	if (!checkNotEmpty(QStringLiteral(u"Client ID"), ui.clientIdLineEdit->text()))
		return;

	emit sigStartVoiceChat(
		ui.brokerUrlLineEdit->text().trimmed(),
		ui.agentIdLineEdit->text().trimmed(),
		ui.clientIdLineEdit->text().trimmed());
}