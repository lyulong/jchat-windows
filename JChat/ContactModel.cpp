﻿#include "ContactModel.h"

#include <QxOrm.h>

#include "Util.h"
#include "ModelRange.h"

namespace JChat{

	ContactModel::ContactModel(ClientObjectPtr const& co, QObject* parent /*= nullptr*/)
		:QStandardItemModel(parent)
		, _co(co)
	{
		qApp->setProperty(staticMetaObject.className(), QVariant::fromValue(this));

		_co->onEvent(this, [=](Jmcpp::GroupInfoUpdatedEvent const& e)
		{
			for(auto&& iter : getGroupRootItem() | depthFirst)
			{
				if(iter->data(ConIdRole).value<Jmcpp::ConversationId>() == e.groupId)
				{
					_updateItemGroupInfo(iter.current, e.groupId);
					break;
				}
			}
		});

		connect(co.get(), &ClientObject::userInfoUpdated, this, &ContactModel::onUserInfoUpdated);


		connect(co.get(), &ClientObject::friendListUpdated, this, [=](Jmcpp::UserInfoList const& userInfos)
		{
			auto userItem = getFriendRootItem();
			userItem->removeRows(0, userItem->rowCount());
			for(auto&& info : userInfos)
			{
				auto item = new QStandardItem();
				auto img = _co->getConversationImage(info.userId);

				item->setData(QVariant::fromValue(Jmcpp::ConversationId(info.userId)), Role::ConIdRole);
				item->setData(QVariant::fromValue(img), Role::ImageRole);
				item->setData(getUserDisplayName(info), Role::TitleRole);
				item->setData(QVariant::fromValue(info), Role::InfoRole);
				userItem->appendRow(item);
			}
		});
		connect(co.get(), &ClientObject::groupListUpdated, this, [=](Jmcpp::GroupInfoList const & groups)
		{
			auto groupItem = getGroupRootItem();
			groupItem->removeRows(0, groupItem->rowCount());
			for(auto&& info : groups)
			{
				auto item = new QStandardItem();

				auto img = _co->getConversationImage(info.groupId);

				item->setData(QVariant::fromValue(Jmcpp::ConversationId(info.groupId)), Role::ConIdRole);

				item->setData(QVariant::fromValue(img), Role::ImageRole);
				if(!info.groupName.empty())
					item->setData(QString::fromStdString(info.groupName), Role::TitleRole);

				item->setData(QVariant::fromValue(info), Role::InfoRole);
				groupItem->appendRow(item);
			}
		});

		{
			auto item = new QStandardItem();
			item->setText(u8"验证消息");
			auto icon = QIcon(u8":/image/resource/RequestMsg.png");
			item->setIcon(icon);
			this->appendRow(item);
		}

		{
			auto item = new QStandardItem();
			item->setText(u8"联系人");
			auto icon = QIcon(u8":/image/resource/Friend.png");
			item->setIcon(icon);
			this->appendRow(item);
		}
		{
			auto item = new QStandardItem();
			item->setText(u8"群组");
			auto icon = QIcon(u8":/image/resource/GroupIcon.png");
			item->setIcon(icon);
			this->appendRow(item);
		}
	}

	ContactModel::~ContactModel()
	{

	}


	void ContactModel::onUserInfoUpdated(Jmcpp::UserId const& userId)
	{
		for(auto&& iter : getFriendRootItem() | depthFirst)
		{
			if(iter->data(ConIdRole).value<Jmcpp::ConversationId>() == userId)
			{
				_updateItemUserInfo(iter.current, userId);
				break;
			}
		}
	}


	None ContactModel::_updateItemUserInfo(QStandardItem* item, Jmcpp::UserId userId)
	{
		auto self = this | qTrack;
		auto co = _co;

		auto index = QPersistentModelIndex(item->index());

		auto info = co_await co->getCacheUserInfo(userId);
		auto img = co_await co->getCacheUserAvatar(userId, info.avatar);
		co_await self;
		if(!index.isValid())
		{
			co_return;
		}

		item = self->itemFromIndex(index);
		item->setData(QVariant::fromValue(img), Role::ImageRole);
		item->setData(getUserDisplayName(info), Role::TitleRole);
		item->setData(QVariant::fromValue(info), Role::InfoRole);
	}

	None ContactModel::_updateItemGroupInfo(QStandardItem* item, Jmcpp::GroupId groupId)
	{
		auto self = this | qTrack;
		auto co = _co;
		auto index = QPersistentModelIndex(item->index());

		auto info = co_await co->getCacheGroupInfo(groupId);
		auto img = co_await co->getCacheGroupAvatar(groupId, info.avatar);
		co_await self;
		if(!index.isValid())
		{
			co_return;
		}
		item = self->itemFromIndex(index);
		item->setData(QVariant::fromValue(img), Role::ImageRole);
		if(!info.groupName.empty())
			item->setData(QString::fromStdString(info.groupName), Role::TitleRole);
		item->setData(QVariant::fromValue(info), Role::InfoRole);
	}

	void ContactDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
	{
		QStyledItemDelegate::paint(painter, option, index);
		painter->save();

		QStyleOptionViewItem opt = option;
		initStyleOption(&opt, index);

		auto font = painter->font();
		font.setPointSize(12);
		painter->setFont(font);
		painter->setPen(Qt::black);
		painter->setRenderHints(QPainter::SmoothPixmapTransform | QPainter::TextAntialiasing);

		auto info = index.data(ContactModel::Role::InfoRole);
		if(info.canConvert<Jmcpp::UserInfo>())
		{
			auto userInfo = info.value<Jmcpp::UserInfo>();

			{
				auto rect = opt.rect.adjusted(3, 3, 0, 0);
				rect.setSize({ 44,44 });

				QPixmap img = index.data(ContactModel::ImageRole).value<QPixmap>();
				painter->drawPixmap(rect, img);
			}

			{
				auto rect = opt.rect.adjusted(55, 0, 0, 0);
				painter->drawText(rect, Qt::AlignVCenter, getUserDisplayName(userInfo));
			}
		}
		else if(info.canConvert<Jmcpp::GroupInfo>())
		{
			auto groupInfo = info.value<Jmcpp::GroupInfo>();

			{
				auto rect = opt.rect.adjusted(3, 3, 0, 0);
				rect.setSize({ 44,44 });

				QPixmap img = index.data(ContactModel::ImageRole).value<QPixmap>();
				painter->drawPixmap(rect, img);
			}

			{
				auto rect = opt.rect.adjusted(55, 0, 0, 0);
				painter->drawText(rect, Qt::AlignVCenter, index.data(ContactModel::TitleRole).toString());
			}
		}


		painter->restore();

	}

	QSize ContactDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
	{
		auto size = QStyledItemDelegate::sizeHint(option, index);
		size.setHeight(50);
		return size;
	}

}
