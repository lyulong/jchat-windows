﻿
#include "LoginWidget.h"
#include <QMessageBox>
#include <QxOrm.h>

#include "MainWidget.h"
#include "Emoji.h"
#include "BusyIndicator.h"
#include "RememberedAccount.h"


namespace JChat{

	LoginWidget::LoginWidget(QWidget *parent)
		: QWidget(parent)
	{
		ui.setupUi(this);
		setAttribute(Qt::WA_DeleteOnClose);

		this->setWindowFlags(Qt::WindowType::CustomizeWindowHint | Qt::WindowType::WindowCloseButtonHint);

		auto userIcon = QIcon(u8":/image/resource/用户名 icon.png");
		auto passwordIcon = QIcon(u8":/image/resource/密码 icon.png");

		ui.username->addAction(userIcon, QLineEdit::LeadingPosition);
		ui.password->addAction(passwordIcon, QLineEdit::LeadingPosition);
		ui.btnLogin->setEnabled(false);

		ui.usernameR->addAction(userIcon, QLineEdit::LeadingPosition);
		ui.password1->addAction(passwordIcon, QLineEdit::LeadingPosition);
		ui.password2->addAction(passwordIcon, QLineEdit::LeadingPosition);
		ui.btnRegister->setEnabled(false);

		connect(ui.username, &QLineEdit::returnPressed, ui.btnLogin, &QPushButton::click);
		connect(ui.password, &QLineEdit::returnPressed, ui.btnLogin, &QPushButton::click);


		connect(ui.usernameR, &QLineEdit::returnPressed, ui.btnRegister, &QPushButton::click);
		connect(ui.password1, &QLineEdit::returnPressed, ui.btnRegister, &QPushButton::click);
		connect(ui.password2, &QLineEdit::returnPressed, ui.btnRegister, &QPushButton::click);


		RememberedAccount data;
		auto accounts = data.getRememberedUsers();
		if(!accounts.empty())
		{
			auto&& account = accounts.front();
			if(!account.username.isEmpty() && !account.password.isEmpty())
			{
				ui.checkBox->setCheckState(Qt::Checked);
				ui.username->setText(account.username);
				ui.password->setText(account.password);
				ui.password->setProperty("encoded", true);
			}
		}

		updateLoginButtonState();

		connect(ui.username, &QLineEdit::textChanged, [=](QString const& str){	updateLoginButtonState(); });
		connect(ui.password, &QLineEdit::textChanged, [=](QString const& str){	updateLoginButtonState(); ui.password->setProperty("encoded", false);   });

		connect(ui.usernameR, &QLineEdit::textChanged, [=](QString const& str){	updateRegisterButtonState(); });
		connect(ui.password1, &QLineEdit::textChanged, [=](QString const& str){	updateRegisterButtonState(); });
		connect(ui.password2, &QLineEdit::textChanged, [=](QString const& str){	updateRegisterButtonState(); });


		_co = std::make_shared<JChat::ClientObject>(ClientObject::getSDKConfig());

		connect(ui.btnLogin, &QPushButton::clicked, this, [=]() mutable
		{

			try
			{
				auto lock = std::make_unique<QSharedMemory>(ui.username->text());
				if(!lock->create(1))
				{
					QMessageBox::warning(this, "", QString(u8"用户[%1]已登录,不能重复登录").arg(ui.username->text()), QMessageBox::Ok);
					return;
				}

				BusyIndicator busy(this);
				ui.btnLogin->setEnabled(false);

				auto mwPtr = std::make_unique<MainWidget>(_co);

				qAwait(pplx::create_task([]{
					Emoji::getSingleton()->moveToThread(qApp->thread());
				}));

				qAwait(_co->login(ui.username->text().toStdString(),
								  ui.password->text().toStdString(), ClientObject::getAuthorization(),
								  ui.password->property("encoded").toBool()));

				lock.release();

				Account accout;
				accout.username = ui.username->text();
				accout.password = ui.password->property("encoded").toBool() ? ui.password->text() : QCryptographicHash::hash(ui.password->text().toUtf8(), QCryptographicHash::Md5).toHex();

				if(ui.checkBox->isChecked())
				{
					data.addRememberedUsers(accout);
				}
				else
				{
					data.removeRememberedUsers(accout);
				}
				mwPtr->show();
				mwPtr.release();

				this->close();


				return;
			}
			catch(Jmcpp::ServerException& e)
			{
				if(e.code() == 880104 || e.code() == 880103)
				{
					QMessageBox::warning(this, tr("warning"), u8"用户名或密码错误!");
				}
				else
				{
					QMessageBox::warning(this, tr("warning"), e.what());
				}
			}
			catch(std::runtime_error& e)
			{
				QMessageBox::warning(this, tr("warning"), e.what());
			}

			ui.btnLogin->setEnabled(true);
		});


		connect(ui.switchLogin, &QPushButton::clicked, this, [=]
		{
			ui.stackedWidget->setCurrentWidget(ui.pageLogin);
		});


		connect(ui.btnRegister, &QPushButton::clicked, this, [=]
		{
			try
			{
				if(ui.password1->text() != ui.password2->text())
				{
					QMessageBox::warning(this, tr("warning"), u8"您两次输入的密码不一致!");
					return;
				}

				ui.btnRegister->setEnabled(false);
				qAwait(_co->registers(ui.usernameR->text().toStdString(), ui.password1->text().toStdString(), ClientObject::getAuthorization()));
				QMessageBox::information(this, tr("info"), u8"注册成功!");
			}
			catch(Jmcpp::ServerException& e)
			{
				if(e.code() == 882002)
				{
					QMessageBox::warning(this, tr("warning"), u8"用户名已存在!");
				}
				else
				{
					QMessageBox::warning(this, tr("warning"), e.what());
				}
			}
			catch(std::system_error& e)
			{

			}

			ui.btnRegister->setEnabled(true);
		});

		connect(ui.switchRegister, &QPushButton::clicked, this, [=]
		{
			ui.stackedWidget->setCurrentWidget(ui.pageRegister);
		});
	}

	LoginWidget::~LoginWidget()
	{
		_co.reset();
	}

	void LoginWidget::closeEvent(QCloseEvent *event)
	{
		if(!_co->isLogined())
		{
			qApp->quit();
		}
	}
}