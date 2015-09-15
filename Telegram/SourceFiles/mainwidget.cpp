/*
This file is part of Telegram Desktop,
the official desktop version of Telegram messaging app, see https://telegram.org

Telegram Desktop is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

It is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

Full license: https://github.com/telegramdesktop/tdesktop/blob/master/LICENSE
Copyright (c) 2014 John Preston, https://desktop.telegram.org
*/
#include "stdafx.h"
#include "style.h"
#include "lang.h"

#include "boxes/addcontactbox.h"
#include "application.h"
#include "window.h"
#include "settingswidget.h"
#include "mainwidget.h"
#include "boxes/confirmbox.h"
#include "boxes/stickersetbox.h"
#include "boxes/contactsbox.h"

#include "localstorage.h"

#include "audio.h"

TopBarWidget::TopBarWidget(MainWidget *w) : TWidget(w),
	a_over(0), _drawShadow(true), _selPeer(0), _selCount(0), _selStrLeft(-st::topBarButton.width / 2), _selStrWidth(0), _animating(false),
	_clearSelection(this, lang(lng_selected_clear), st::topBarButton),
	_forward(this, lang(lng_selected_forward), st::topBarActionButton),
	_delete(this, lang(lng_selected_delete), st::topBarActionButton),
	_selectionButtonsWidth(_clearSelection.width() + _forward.width() + _delete.width()), _forwardDeleteWidth(qMax(_forward.textWidth(), _delete.textWidth())),
	_info(this, lang(lng_topbar_info), st::topBarButton),
	_edit(this, lang(lng_profile_edit_contact), st::topBarButton),
	_leaveGroup(this, lang(lng_profile_delete_and_exit), st::topBarButton),
	_addContact(this, lang(lng_profile_add_contact), st::topBarButton),
	_deleteContact(this, lang(lng_profile_delete_contact), st::topBarButton),
	_mediaType(this, lang(lng_media_type), st::topBarButton) {

	connect(&_forward, SIGNAL(clicked()), this, SLOT(onForwardSelection()));
	connect(&_delete, SIGNAL(clicked()), this, SLOT(onDeleteSelection()));
	connect(&_clearSelection, SIGNAL(clicked()), this, SLOT(onClearSelection()));
	connect(&_info, SIGNAL(clicked()), this, SLOT(onInfoClicked()));
	connect(&_addContact, SIGNAL(clicked()), this, SLOT(onAddContact()));
	connect(&_deleteContact, SIGNAL(clicked()), this, SLOT(onDeleteContact()));
	connect(&_edit, SIGNAL(clicked()), this, SLOT(onEdit()));
	connect(&_leaveGroup, SIGNAL(clicked()), this, SLOT(onDeleteAndExit()));

	setCursor(style::cur_pointer);
	showAll();
}

void TopBarWidget::onForwardSelection() {
	if (App::main()) App::main()->forwardSelectedItems();
}

void TopBarWidget::onDeleteSelection() {
	if (App::main()) App::main()->deleteSelectedItems();
}

void TopBarWidget::onClearSelection() {
	if (App::main()) App::main()->clearSelectedItems();
}

void TopBarWidget::onInfoClicked() {
	PeerData *p = App::main() ? App::main()->historyPeer() : 0;
	if (p) App::main()->showPeerProfile(p);
}

void TopBarWidget::onAddContact() {
	PeerData *p = App::main() ? App::main()->profilePeer() : 0;
	UserData *u = p ? p->asUser() : 0;
	if (u) App::wnd()->showLayer(new AddContactBox(u->firstName, u->lastName, u->phone.isEmpty() ? App::phoneFromSharedContact(peerToUser(u->id)) : u->phone));
}

void TopBarWidget::onEdit() {
	PeerData *p = App::main() ? App::main()->profilePeer() : 0;
	if (p) App::wnd()->showLayer(new AddContactBox(p));
}

void TopBarWidget::onDeleteContact() {
	PeerData *p = App::main() ? App::main()->profilePeer() : 0;
	UserData *u = p ? p->asUser() : 0;
	if (u) {
		ConfirmBox *box = new ConfirmBox(lng_sure_delete_contact(lt_contact, p->name));
		connect(box, SIGNAL(confirmed()), this, SLOT(onDeleteContactSure()));
		App::wnd()->showLayer(box);
	}
}

void TopBarWidget::onDeleteContactSure() {
	PeerData *p = App::main() ? App::main()->profilePeer() : 0;
	UserData *u = p ? p->asUser() : 0;
	if (u) {
		App::main()->showDialogs();
		App::wnd()->hideLayer();
		MTP::send(MTPcontacts_DeleteContact(u->inputUser), App::main()->rpcDone(&MainWidget::deletedContact, u));
	}
}

void TopBarWidget::onDeleteAndExit() {
	PeerData *p = App::main() ? App::main()->profilePeer() : 0;
	ChatData *c = p ? p->asChat() : 0;
	if (c) {
		ConfirmBox *box = new ConfirmBox(lng_sure_delete_and_exit(lt_group, p->name));
		connect(box, SIGNAL(confirmed()), this, SLOT(onDeleteAndExitSure()));
		App::wnd()->showLayer(box);
	}
}

void TopBarWidget::onDeleteAndExitSure() {
	PeerData *p = App::main() ? App::main()->profilePeer() : 0;
	ChatData *c = p ? p->asChat() : 0;
	if (c) {
		App::main()->showDialogs();
		App::wnd()->hideLayer();
		MTP::send(MTPmessages_DeleteChatUser(c->inputChat, App::self()->inputUser), App::main()->rpcDone(&MainWidget::deleteHistoryAfterLeave, p), App::main()->rpcFail(&MainWidget::leaveChatFailed, p));
	}
}

void TopBarWidget::enterEvent(QEvent *e) {
	a_over.start(1);
	anim::start(this);
}

void TopBarWidget::enterFromChildEvent(QEvent *e) {
	a_over.start(1);
	anim::start(this);
}

void TopBarWidget::leaveEvent(QEvent *e) {
	a_over.start(0);
	anim::start(this);
}

void TopBarWidget::leaveToChildEvent(QEvent *e) {
	a_over.start(0);
	anim::start(this);
}

bool TopBarWidget::animStep(float64 ms) {
	float64 dt = ms / st::topBarDuration;
	bool res = true;
	if (dt >= 1) {
		a_over.finish();
		res = false;
	} else {
		a_over.update(dt, anim::linear);
	}
	update();
	return res;
}

void TopBarWidget::enableShadow(bool enable) {
	_drawShadow = enable;
}

void TopBarWidget::paintEvent(QPaintEvent *e) {
	QPainter p(this);

	if (e->rect().top() < st::topBarHeight) { // optimize shadow-only drawing
		p.fillRect(QRect(0, 0, width(), st::topBarHeight), st::topBarBG->b);
		if (_clearSelection.isHidden()) {
			p.save();
			main()->paintTopBar(p, a_over.current(), _info.isHidden() ? 0 : _info.width());
			p.restore();
		} else {
			p.setFont(st::linkFont->f);
			p.setPen(st::btnDefLink.color->p);
			p.drawText(_selStrLeft, st::topBarButton.textTop + st::linkFont->ascent, _selStr);
		}
	}
	if (_drawShadow) {
		int32 shadowCoord = 0;
		float64 shadowOpacity = 1.;
		main()->topBarShadowParams(shadowCoord, shadowOpacity);

		p.setOpacity(shadowOpacity);
		if (cWideMode()) {
			p.fillRect(shadowCoord + st::titleShadow, st::topBarHeight, width() - st::titleShadow, st::titleShadow, st::titleShadowColor->b);
		} else {
			p.fillRect(shadowCoord, st::topBarHeight, width(), st::titleShadow, st::titleShadowColor->b);
		}
	}
}

void TopBarWidget::mousePressEvent(QMouseEvent *e) {
	PeerData *p = App::main() ? App::main()->profilePeer() : 0;
	if (e->button() == Qt::LeftButton && e->pos().y() < st::topBarHeight && (p || !_selCount)) {
		emit clicked();
	}
}

void TopBarWidget::resizeEvent(QResizeEvent *e) {
	int32 r = width();
	if (!_forward.isHidden() || !_delete.isHidden()) {
		int32 fullW = r - (_selectionButtonsWidth + (_selStrWidth - st::topBarButton.width) + st::topBarActionSkip);
		int32 selectedClearWidth = st::topBarButton.width, forwardDeleteWidth = st::topBarActionButton.width - _forwardDeleteWidth, skip = st::topBarActionSkip;
		while (fullW < 0) {
			int fit = 0;
			if (selectedClearWidth < -2 * (st::topBarMinPadding + 1)) {
				fullW += 4;
				selectedClearWidth += 2;
			} else if (selectedClearWidth < -2 * st::topBarMinPadding) {
				fullW += (-2 * st::topBarMinPadding - selectedClearWidth) * 2;
				selectedClearWidth = -2 * st::topBarMinPadding;
			} else {
				++fit;
			}
			if (fullW >= 0) break;

			if (forwardDeleteWidth > 2 * (st::topBarMinPadding + 1)) {
				fullW += 4;
				forwardDeleteWidth -= 2;
			} else if (forwardDeleteWidth > 2 * st::topBarMinPadding) {
				fullW += (forwardDeleteWidth - 2 * st::topBarMinPadding) * 2;
				forwardDeleteWidth = 2 * st::topBarMinPadding;
			} else {
				++fit;
			}
			if (fullW >= 0) break;

			if (skip > st::topBarMinPadding) {
				--skip;
				++fullW;
			} else {
				++fit;
			}
			if (fullW >= 0 || fit >= 3) break;
		}
		_clearSelection.setWidth(selectedClearWidth);
		_forward.setWidth(_forwardDeleteWidth + forwardDeleteWidth);
		_delete.setWidth(_forwardDeleteWidth + forwardDeleteWidth);
		_selStrLeft = -selectedClearWidth / 2;

		int32 availX = _selStrLeft + _selStrWidth, availW = r - (_clearSelection.width() + selectedClearWidth / 2) - availX;
		if (_forward.isHidden()) {
			_delete.move(availX + (availW - _delete.width()) / 2, (st::topBarHeight - _forward.height()) / 2);
		} else if (_delete.isHidden()) {
			_forward.move(availX + (availW - _forward.width()) / 2, (st::topBarHeight - _forward.height()) / 2);
		} else {
			_forward.move(availX + (availW - _forward.width() - _delete.width() - skip) / 2, (st::topBarHeight - _forward.height()) / 2);
			_delete.move(availX + (availW + _forward.width() - _delete.width() + skip) / 2, (st::topBarHeight - _forward.height()) / 2);
		}
		_clearSelection.move(r -= _clearSelection.width(), 0);
	}
	if (!_info.isHidden()) _info.move(r -= _info.width(), 0);
	if (!_deleteContact.isHidden()) _deleteContact.move(r -= _deleteContact.width(), 0);
	if (!_leaveGroup.isHidden()) _leaveGroup.move(r -= _leaveGroup.width(), 0);
	if (!_edit.isHidden()) _edit.move(r -= _edit.width(), 0);
	if (!_addContact.isHidden()) _addContact.move(r -= _addContact.width(), 0);
	if (!_mediaType.isHidden()) _mediaType.move(r -= _mediaType.width(), 0);
}

void TopBarWidget::startAnim() {
	_info.hide();
	_edit.hide();
	_leaveGroup.hide();
	_addContact.hide();
	_deleteContact.hide();
    _clearSelection.hide();
    _delete.hide();
    _forward.hide();
	_mediaType.hide();
    _animating = true;
}

void TopBarWidget::stopAnim() {
    _animating = false;
    showAll();
}

void TopBarWidget::showAll() {
    if (_animating) {
        resizeEvent(0);
        return;
    }
	PeerData *p = App::main() ? App::main()->profilePeer() : 0, *h = App::main() ? App::main()->historyPeer() : 0, *o = App::main() ? App::main()->overviewPeer() : 0;
	if (p && (p->isChat() || (p->isUser() && (p->asUser()->contact >= 0 || !App::phoneFromSharedContact(peerToUser(p->id)).isEmpty())))) {
		if (p->isChat()) {
			if (p->asChat()->forbidden) {
				_edit.hide();
			} else {
				_edit.show();
			}
			_leaveGroup.show();
			_addContact.hide();
			_deleteContact.hide();
		} else if (p->asUser()->contact > 0) {
			_edit.show();
			_leaveGroup.hide();
			_addContact.hide();
			_deleteContact.show();
		} else {
			_edit.hide();
			_leaveGroup.hide();
			_addContact.show();
			_deleteContact.hide();
		}
		_clearSelection.hide();
		_info.hide();
		_delete.hide();
		_forward.hide();
		_mediaType.hide();
	} else {
		if (p && p->isChannel() && p->asChannel()->adminned) {
			_edit.show();
		} else {
			_edit.hide();
		}
		_leaveGroup.hide();
		_addContact.hide();
		_deleteContact.hide();
		if (!p && _selCount) {
			_clearSelection.show();
			if ((h && h->isChannel() && !h->asChannel()->adminned) || (o && o->isChannel() && !o->asChannel()->adminned)) {
				_delete.hide();
			} else {
				_delete.show();
			}
			_forward.show();
			_mediaType.hide();
		} else {
			_clearSelection.hide();
			_delete.hide();
			_forward.hide();
			if (App::main() && App::main()->mediaTypeSwitch()) {
				_mediaType.show();
			} else {
				_mediaType.hide();
			}
		}
        if (App::main() && App::main()->historyPeer() && !o && !p && _clearSelection.isHidden() && !cWideMode()) {
			_info.show();
		} else {
			_info.hide();
		}
	}
	resizeEvent(0);
}

void TopBarWidget::showSelected(uint32 selCount) {
	PeerData *p = App::main() ? App::main()->profilePeer() : 0;
	_selPeer = App::main()->overviewPeer() ? App::main()->overviewPeer() : App::main()->peer();
	_selCount = selCount;
	_selStr = (_selCount > 0) ? lng_selected_count(lt_count, _selCount) : QString();
	_selStrWidth = st::btnDefLink.font->m.width(_selStr);
	setCursor((!p && _selCount) ? style::cur_default : style::cur_pointer);
	showAll();
}

FlatButton *TopBarWidget::mediaTypeButton() {
	return &_mediaType;
}

MainWidget *TopBarWidget::main() {
	return static_cast<MainWidget*>(parentWidget());
}

MainWidget::MainWidget(Window *window) : QWidget(window),
_started(0), failedObjId(0), _toForwardNameVersion(0), _dialogsWidth(st::dlgMinWidth),
dialogs(this), history(this), profile(0), overview(0), _player(this), _topBar(this),
_forwardConfirm(0), _hider(0), _peerInStack(0), _msgIdInStack(0),
_playerHeight(0), _contentScrollAddToY(0), _mediaType(this), _mediaTypeMask(0),
updDate(0), updQts(-1), updSeq(0), _getDifferenceTimeByPts(0), _getDifferenceTimeAfterFail(0),
_onlineRequest(0), _lastWasOnline(false), _lastSetOnline(0), _isIdle(false),
_failDifferenceTimeout(1), _lastUpdateTime(0), _cachedX(0), _cachedY(0), _background(0), _api(new ApiWrap(this)) {
	setGeometry(QRect(0, st::titleHeight, App::wnd()->width(), App::wnd()->height() - st::titleHeight));

	_ptsWaiter.setRequesting(true);
	updateScrollColors();

	connect(window, SIGNAL(resized(const QSize &)), this, SLOT(onParentResize(const QSize &)));
	connect(&dialogs, SIGNAL(cancelled()), this, SLOT(dialogsCancelled()));
	connect(&history, SIGNAL(cancelled()), &dialogs, SLOT(activate()));
	connect(this, SIGNAL(peerPhotoChanged(PeerData*)), this, SIGNAL(dialogsUpdated()));
	connect(&noUpdatesTimer, SIGNAL(timeout()), this, SLOT(mtpPing()));
	connect(&_onlineTimer, SIGNAL(timeout()), this, SLOT(updateOnline()));
	connect(&_onlineUpdater, SIGNAL(timeout()), this, SLOT(updateOnlineDisplay()));
	connect(&_idleFinishTimer, SIGNAL(timeout()), this, SLOT(checkIdleFinish()));
	connect(&_bySeqTimer, SIGNAL(timeout()), this, SLOT(getDifference()));
	connect(&_byPtsTimer, SIGNAL(timeout()), this, SLOT(onGetDifferenceTimeByPts()));
	connect(&_failDifferenceTimer, SIGNAL(timeout()), this, SLOT(onGetDifferenceTimeAfterFail()));
	connect(_api, SIGNAL(fullPeerUpdated(PeerData*)), this, SIGNAL(peerUpdated(PeerData*)));
	connect(this, SIGNAL(peerUpdated(PeerData*)), &history, SLOT(peerUpdated(PeerData*)));
	connect(&_topBar, SIGNAL(clicked()), this, SLOT(onTopBarClick()));
	connect(&history, SIGNAL(peerShown(PeerData*)), this, SLOT(onPeerShown(PeerData*)));
	connect(&updateNotifySettingTimer, SIGNAL(timeout()), this, SLOT(onUpdateNotifySettings()));
	connect(this, SIGNAL(showPeerAsync(quint64,qint32)), this, SLOT(showPeerHistory(quint64,qint32)), Qt::QueuedConnection);
	if (audioPlayer()) {
		connect(audioPlayer(), SIGNAL(updated(const AudioMsgId&)), this, SLOT(audioPlayProgress(const AudioMsgId&)));
		connect(audioPlayer(), SIGNAL(stopped(const AudioMsgId&)), this, SLOT(audioPlayProgress(const AudioMsgId&)));
		connect(audioPlayer(), SIGNAL(updated(const SongMsgId&)), this, SLOT(documentPlayProgress(const SongMsgId&)));
		connect(audioPlayer(), SIGNAL(stopped(const SongMsgId&)), this, SLOT(documentPlayProgress(const SongMsgId&)));
	}
	connect(&_updateMutedTimer, SIGNAL(timeout()), this, SLOT(onUpdateMuted()));

	_webPageUpdater.setSingleShot(true);
	connect(&_webPageUpdater, SIGNAL(timeout()), this, SLOT(webPagesUpdate()));

	connect(&_cacheBackgroundTimer, SIGNAL(timeout()), this, SLOT(onCacheBackground()));

	dialogs.show();
	if (cWideMode()) {
		history.show();
	} else {
		history.hide();
	}
	App::wnd()->getTitle()->updateBackButton();
	_topBar.hide();

	_player.hide();

	orderWidgets();

	MTP::setGlobalFailHandler(rpcFail(&MainWidget::updateFail));

	_mediaType.hide();
	_topBar.mediaTypeButton()->installEventFilter(&_mediaType);

	show();
	setFocus();

	_api->init();
}

bool MainWidget::onForward(const PeerId &peer, ForwardWhatMessages what) {
	PeerData *p = App::peer(peer);
	if (!peer || (p->isChannel() && !p->asChannel()->adminned) || (p->isChat() && p->asChat()->forbidden) || (p->isUser() && p->asUser()->access == UserNoAccess)) {
		App::wnd()->showLayer(new ConfirmBox(lang(lng_forward_cant), true));
		return false;
	}
	history.cancelReply();
	_toForward.clear();
	if (what == ForwardSelectedMessages) {
		if (overview) {
			overview->fillSelectedItems(_toForward, false);
		} else {
			history.fillSelectedItems(_toForward, false);
		}
	} else {
		HistoryItem *item = 0;
		if (what == ForwardContextMessage) {
			item = App::contextItem();
		} else if (what == ForwardPressedMessage) {
			item = App::pressedItem();
		} else if (what == ForwardPressedLinkMessage) {
			item = App::pressedLinkItem();
		}
		if (dynamic_cast<HistoryMessage*>(item) && item->id > 0) {
			_toForward.insert(item->id, item);
		}
	}
	updateForwardingTexts();
	showPeerHistory(peer, ShowAtUnreadMsgId);
	history.onClearSelected();
	history.updateForwarding();
	return true;
}

bool MainWidget::hasForwardingItems() {
	return !_toForward.isEmpty();
}

void MainWidget::fillForwardingInfo(Text *&from, Text *&text, bool &serviceColor, ImagePtr &preview) {
	if (_toForward.isEmpty()) return;
	int32 version = 0;
	for (SelectedItemSet::const_iterator i = _toForward.cbegin(), e = _toForward.cend(); i != e; ++i) {
		version += i.value()->from()->nameVersion;
	}
	if (version != _toForwardNameVersion) {
		updateForwardingTexts();
	}
	from = &_toForwardFrom;
	text = &_toForwardText;
	serviceColor = (_toForward.size() > 1) || _toForward.cbegin().value()->getMedia() || _toForward.cbegin().value()->serviceMsg();
	if (_toForward.size() < 2 && _toForward.cbegin().value()->getMedia() && _toForward.cbegin().value()->getMedia()->hasReplyPreview()) {
		preview = _toForward.cbegin().value()->getMedia()->replyPreview();
	}
}

void MainWidget::updateForwardingTexts() {
	int32 version = 0;
	QString from, text;
	if (!_toForward.isEmpty()) {
		QMap<PeerData*, bool> fromUsersMap;
		QVector<PeerData*> fromUsers;
		fromUsers.reserve(_toForward.size());
		for (SelectedItemSet::const_iterator i = _toForward.cbegin(), e = _toForward.cend(); i != e; ++i) {
			if (!fromUsersMap.contains(i.value()->from())) {
				fromUsersMap.insert(i.value()->from(), true);
				fromUsers.push_back(i.value()->from());
			}
			version += i.value()->from()->nameVersion;
		}
		if (fromUsers.size() > 2) {
			from = lng_forwarding_from(lt_user, fromUsers.at(0)->shortName(), lt_count, fromUsers.size() - 1);
		} else if (fromUsers.size() < 2) {
			from = fromUsers.at(0)->name;
		} else {
			from = lng_forwarding_from_two(lt_user, fromUsers.at(0)->shortName(), lt_second_user, fromUsers.at(1)->shortName());
		}

		if (_toForward.size() < 2) {
			text = _toForward.cbegin().value()->inReplyText();
		} else {
			text = lng_forward_messages(lt_count, _toForward.size());
		}
	}
	_toForwardFrom.setText(st::msgServiceNameFont, from, _textNameOptions);
	_toForwardText.setText(st::msgFont, text, _textDlgOptions);
	_toForwardNameVersion = version;
}

void MainWidget::cancelForwarding() {
	if (_toForward.isEmpty()) return;

	_toForward.clear();
	history.cancelForwarding();
}

void MainWidget::finishForwarding(History *hist) {
	if (!hist) return;
	if (!_toForward.isEmpty()) {
		bool genClientSideMessage = (_toForward.size() < 2);
		PeerData *forwardFrom = _toForward.cbegin().value()->history()->peer;
		App::main()->readServerHistory(hist, false);

		QVector<MTPint> ids;
		QVector<MTPlong> randomIds;
		ids.reserve(_toForward.size());
		randomIds.reserve(_toForward.size());
		for (SelectedItemSet::const_iterator i = _toForward.cbegin(), e = _toForward.cend(); i != e; ++i) {
			uint64 randomId = MTP::nonce<uint64>();
			if (genClientSideMessage) {
				FullMsgId newId(peerToChannel(hist->peer->id), clientMsgId());
				HistoryMessage *msg = static_cast<HistoryMessage*>(_toForward.cbegin().value());
				hist->addToBackForwarded(newId.msg, date(MTP_int(unixtime())), hist->peer->isChannel() ? 0 : MTP::authedId(), msg);
				if (HistorySticker *sticker = dynamic_cast<HistorySticker*>(msg->getMedia())) {
					App::main()->incrementSticker(sticker->document());
				}
				App::historyRegRandom(randomId, newId);
			}
			ids.push_back(MTP_int(i.key()));
			randomIds.push_back(MTP_long(randomId));
		}
		int32 flags = hist->peer->isChannel() ? MTPmessages_ForwardMessages_flag_broadcast : 0;
		hist->sendRequestId = MTP::send(MTPmessages_ForwardMessages(MTP_int(flags), forwardFrom->input, MTP_vector<MTPint>(ids), MTP_vector<MTPlong>(randomIds), hist->peer->input), rpcDone(&MainWidget::sentUpdatesReceived), RPCFailHandlerPtr(), 0, 0, hist->sendRequestId);

		if (history.peer() == hist->peer) history.peerMessagesUpdated();

		cancelForwarding();
	}

	historyToDown(hist);
	dialogsToUp();
	history.peerMessagesUpdated(hist->peer->id);
}

void MainWidget::webPageUpdated(WebPageData *data) {
	_webPagesUpdated.insert(data->id, true);
	_webPageUpdater.start(0);
}

void MainWidget::webPagesUpdate() {
	if (_webPagesUpdated.isEmpty()) return;

	_webPageUpdater.stop();
	const WebPageItems &items(App::webPageItems());
	for (QMap<WebPageId, bool>::const_iterator i = _webPagesUpdated.cbegin(), e = _webPagesUpdated.cend(); i != e; ++i) {
		WebPageItems::const_iterator j = items.constFind(App::webPage(i.key()));
		if (j != items.cend()) {
			for (HistoryItemsMap::const_iterator k = j.value().cbegin(), e = j.value().cend(); k != e; ++k) {
				k.key()->initDimensions();
				itemResized(k.key());
			}
		}
	}
	_webPagesUpdated.clear();
}

void MainWidget::updateMutedIn(int32 seconds) {
	if (seconds > 86400) seconds = 86400;
	int32 ms = seconds * 1000;
	if (_updateMutedTimer.isActive() && _updateMutedTimer.remainingTime() <= ms) return;
	_updateMutedTimer.start(ms);
}

void MainWidget::updateStickers() {
	history.updateStickers();
}

void MainWidget::botCommandsChanged(UserData *bot) {
	history.botCommandsChanged(bot);
}

void MainWidget::onUpdateMuted() {
	App::updateMuted();
}

void MainWidget::onShareContact(const PeerId &peer, UserData *contact) {
	history.onShareContact(peer, contact);
}

void MainWidget::onSendPaths(const PeerId &peer) {
	history.onSendPaths(peer);
}

void MainWidget::onFilesOrForwardDrop(const PeerId &peer, const QMimeData *data) {
	if (data->hasFormat(qsl("application/x-td-forward-selected"))) {
		onForward(peer, ForwardSelectedMessages);
	} else if (data->hasFormat(qsl("application/x-td-forward-pressed-link"))) {
		onForward(peer, ForwardPressedLinkMessage);
	} else if (data->hasFormat(qsl("application/x-td-forward-pressed"))) {
		onForward(peer, ForwardPressedMessage);
	} else {
		showPeerHistory(peer, ShowAtTheEndMsgId);
		history.onFilesDrop(data);
	}
}

void MainWidget::noHider(HistoryHider *destroyed) {
	if (_hider == destroyed) {
		_hider = 0;
		if (cWideMode()) {
			if (_forwardConfirm) {
				_forwardConfirm->deleteLater();
				_forwardConfirm = 0;
			}
		} else {
			if (_forwardConfirm) {
				_forwardConfirm->startHide();
				_forwardConfirm = 0;
			}
			onPeerShown(history.peer());
			if (profile || overview || (history.peer() && history.peer()->id)) {
				dialogs.enableShadow(false);
				QPixmap animCache = myGrab(this, QRect(0, _playerHeight + st::topBarHeight, _dialogsWidth, height() - _playerHeight - st::topBarHeight)),
					animTopBarCache = myGrab(this, QRect(_topBar.x(), _topBar.y(), _topBar.width(), st::topBarHeight));
				dialogs.enableShadow();
				_topBar.enableShadow();
				dialogs.hide();
				if (overview) {
					overview->show();
					overview->animShow(animCache, animTopBarCache);
				} else if (profile) {
					profile->show();
					profile->animShow(animCache, animTopBarCache);
				} else {
					history.show();
					history.animShow(animCache, animTopBarCache);
				}
			}
			App::wnd()->getTitle()->updateBackButton();
		}
	}
}

void MainWidget::hiderLayer(HistoryHider *h) {
	if (App::passcoded()) {
		delete h;
		return;
	}

	_hider = h;
	connect(_hider, SIGNAL(forwarded()), &dialogs, SLOT(onCancelSearch()));
	if (cWideMode()) {
		_hider->show();
		resizeEvent(0);
		dialogs.activate();
	} else {
		dialogsToUp();

		_hider->hide();
		dialogs.enableShadow(false);
		QPixmap animCache = myGrab(this, QRect(0, _playerHeight, _dialogsWidth, height() - _playerHeight));
		dialogs.enableShadow();
		_topBar.enableShadow();

		onPeerShown(0);
		if (overview) {
			overview->hide();
		} else if (profile) {
			profile->hide();
		} else {
			history.hide();
		}
		dialogs.show();
		resizeEvent(0);
		dialogs.animShow(animCache);
		App::wnd()->getTitle()->updateBackButton();
	}
}

void MainWidget::forwardLayer(int32 forwardSelected) {
	hiderLayer((forwardSelected < 0) ? (new HistoryHider(this)) : (new HistoryHider(this, forwardSelected > 0)));
}

void MainWidget::deleteLayer(int32 selectedCount) {
	QString str((selectedCount < 0) ? lang(selectedCount < -1 ? lng_selected_cancel_sure_this : lng_selected_delete_sure_this) : lng_selected_delete_sure(lt_count, selectedCount));
	ConfirmBox *box = new ConfirmBox((selectedCount < 0) ? str : str.arg(selectedCount), lang(lng_selected_delete_confirm));
	if (selectedCount < 0) {
		connect(box, SIGNAL(confirmed()), overview ? overview : static_cast<QWidget*>(&history), SLOT(onDeleteContextSure()));
	} else {
		connect(box, SIGNAL(confirmed()), overview ? overview : static_cast<QWidget*>(&history), SLOT(onDeleteSelectedSure()));
	}
	App::wnd()->showLayer(box);
}

void MainWidget::shareContactLayer(UserData *contact) {
	hiderLayer(new HistoryHider(this, contact));
}

bool MainWidget::selectingPeer(bool withConfirm) {
	return _hider ? (withConfirm ? _hider->withConfirm() : true) : false;
}

void MainWidget::offerPeer(PeerId peer) {
	App::wnd()->hideLayer();
	if (_hider->offerPeer(peer) && !cWideMode()) {
		_forwardConfirm = new ConfirmBox(_hider->offeredText(), lang(lng_forward));
		connect(_forwardConfirm, SIGNAL(confirmed()), _hider, SLOT(forward()));
		connect(_forwardConfirm, SIGNAL(cancelled()), this, SLOT(onForwardCancel()));
		connect(_forwardConfirm, SIGNAL(destroyed(QObject*)), this, SLOT(onForwardCancel(QObject*)));
		App::wnd()->showLayer(_forwardConfirm);
	}
}

void MainWidget::onForwardCancel(QObject *obj) {
	if (!obj || obj == _forwardConfirm) {
		if (_forwardConfirm) {
			if (!obj) _forwardConfirm->startHide();
			_forwardConfirm = 0;
		}
		if (_hider) _hider->offerPeer(0);
	}
}

void MainWidget::dialogsActivate() {
	dialogs.activate();
}

DragState MainWidget::getDragState(const QMimeData *mime) {
	return history.getDragState(mime);
}

bool MainWidget::leaveChatFailed(PeerData *peer, const RPCError &error) {
	if (error.type().startsWith(qsl("FLOOD_WAIT_"))) return false;

	if (error.type() == "CHAT_ID_INVALID") { // left this chat already
		if ((profile && profile->peer() == peer) || (overview && overview->peer() == peer) || _stack.contains(peer) || history.peer() == peer) {
			showDialogs();
		}
		dialogs.removePeer(peer);
		App::history(peer->id)->clear();
		MTP::send(MTPmessages_DeleteHistory(peer->input, MTP_int(0)), rpcDone(&MainWidget::deleteHistoryPart, peer));
		return true;
	}
	return false;
}

void MainWidget::deleteHistoryAfterLeave(PeerData *peer, const MTPUpdates &updates) {
	sentUpdatesReceived(updates);
	if ((profile && profile->peer() == peer) || (overview && overview->peer() == peer) || _stack.contains(peer) || history.peer() == peer) {
		showDialogs();
	}
	dialogs.removePeer(peer);
	App::history(peer->id)->clear();
	MTP::send(MTPmessages_DeleteHistory(peer->input, MTP_int(0)), rpcDone(&MainWidget::deleteHistoryPart, peer));
}

void MainWidget::deleteHistoryPart(PeerData *peer, const MTPmessages_AffectedHistory &result) {
	const MTPDmessages_affectedHistory &d(result.c_messages_affectedHistory());
	ptsUpdated(d.vpts.v, d.vpts_count.v);

	int32 offset = d.voffset.v;
	if (!MTP::authedId()) return;
	if (offset <= 0) {
		cRefReportSpamStatuses().remove(peer->id);
		Local::writeReportSpamStatuses();
		return;
	}

	MTP::send(MTPmessages_DeleteHistory(peer->input, d.voffset), rpcDone(&MainWidget::deleteHistoryPart, peer));
}

void MainWidget::deleteMessages(PeerData *peer, const QVector<MTPint> &ids) {
	if (peer->isChannel()) {
		MTP::send(MTPmessages_DeleteChannelMessages(peer->asChannel()->input, MTP_vector<MTPint>(ids)), rpcDone(&MainWidget::messagesAffected, peer));
	} else {
		MTP::send(MTPmessages_DeleteMessages(MTP_vector<MTPint>(ids)), rpcDone(&MainWidget::messagesAffected, peer));
	}
}

void MainWidget::deletedContact(UserData *user, const MTPcontacts_Link &result) {
	const MTPDcontacts_link &d(result.c_contacts_link());
	App::feedUsers(MTP_vector<MTPUser>(1, d.vuser), false);
	App::feedUserLink(MTP_int(peerToUser(user->id)), d.vmy_link, d.vforeign_link, false);
	App::emitPeerUpdated();
}

void MainWidget::deleteConversation(PeerData *peer) {
	dialogs.removePeer(peer);
	History *h = App::history(peer->id);
	h->clear();
	h->newLoaded = h->oldLoaded = true;
	showDialogs();
	MTP::send(MTPmessages_DeleteHistory(peer->input, MTP_int(0)), rpcDone(&MainWidget::deleteHistoryPart, peer));
}

void MainWidget::clearHistory(PeerData *peer) {
	History *h = App::history(peer->id);
	if (h->lastMsg) {
		Local::addSavedPeer(h->peer, h->lastMsg->date);
	}
	h->clear();
	h->newLoaded = h->oldLoaded = true;
	showPeerHistory(peer->id, ShowAtUnreadMsgId);
	MTP::send(MTPmessages_DeleteHistory(peer->input, MTP_int(0)), rpcDone(&MainWidget::deleteHistoryPart, peer));
}

void MainWidget::removeContact(UserData *user) {
	dialogs.removeContact(user);
}

void MainWidget::addParticipants(ChatData *chat, const QVector<UserData*> &users) {
	for (QVector<UserData*>::const_iterator i = users.cbegin(), e = users.cend(); i != e; ++i) {
		MTP::send(MTPmessages_AddChatUser(chat->inputChat, (*i)->inputUser, MTP_int(ForwardOnAdd)), rpcDone(&MainWidget::sentUpdatesReceived), rpcFail(&MainWidget::addParticipantFail, *i), 0, 5);
	}
	App::wnd()->hideLayer();
	showPeerHistory(chat->id, ShowAtTheEndMsgId);
}

bool MainWidget::addParticipantFail(UserData *user, const RPCError &error) {
	if (error.type().startsWith(qsl("FLOOD_WAIT_"))) return false;

	QString text = lang(lng_failed_add_participant);
	if (error.type() == "USER_LEFT_CHAT") { // trying to return banned user to his group
	} else if (error.type() == "USER_NOT_MUTUAL_CONTACT") { // trying to return user who does not have me in contacts
		text = lang(lng_failed_add_not_mutual);
	} else if (error.type() == "USER_ALREADY_PARTICIPANT" && user->botInfo) {
		text = lang(lng_bot_already_in_group);
	} else if (error.type() == "PEER_FLOOD") {
		text = lng_cant_invite_not_contact(lt_more_info, textcmdLink(qsl("https://telegram.org/faq?_hash=can-39t-send-messages-to-non-contacts"), lang(lng_cant_more_info)));
	}
	App::wnd()->showLayer(new ConfirmBox(text, true));
	return false;
}

void MainWidget::kickParticipant(ChatData *chat, UserData *user) {
	MTP::send(MTPmessages_DeleteChatUser(chat->inputChat, user->inputUser), rpcDone(&MainWidget::sentUpdatesReceived), rpcFail(&MainWidget::kickParticipantFail, chat));
	App::wnd()->hideLayer();
	showPeerHistory(chat->id, ShowAtTheEndMsgId);
}

bool MainWidget::kickParticipantFail(ChatData *chat, const RPCError &error) {
	if (error.type().startsWith(qsl("FLOOD_WAIT_"))) return false;

	error.type();
	return false;
}

void MainWidget::checkPeerHistory(PeerData *peer) {
	if (peer->isChannel()) {
		MTP::send(MTPmessages_GetImportantHistory(peer->input, MTP_int(0), MTP_int(0), MTP_int(1), MTP_int(0), MTP_int(0)), rpcDone(&MainWidget::checkedHistory, peer));
	} else {
		MTP::send(MTPmessages_GetHistory(peer->input, MTP_int(0), MTP_int(0), MTP_int(1), MTP_int(0), MTP_int(0)), rpcDone(&MainWidget::checkedHistory, peer));
	}
}

void MainWidget::checkedHistory(PeerData *peer, const MTPmessages_Messages &result) {
	const QVector<MTPMessage> *v = 0;
	switch (result.type()) {
	case mtpc_messages_messages: {
		const MTPDmessages_messages &d(result.c_messages_messages());
		App::feedUsers(d.vusers);
		App::feedChats(d.vchats);
		v = &d.vmessages.c_vector().v;
	} break;

	case mtpc_messages_messagesSlice: {
		const MTPDmessages_messagesSlice &d(result.c_messages_messagesSlice());
		App::feedUsers(d.vusers);
		App::feedChats(d.vchats);
		v = &d.vmessages.c_vector().v;
	} break;

	case mtpc_messages_channelMessages: {
		const MTPDmessages_channelMessages &d(result.c_messages_channelMessages());
		if (peer && peer->isChannel()) {
			peer->asChannel()->ptsReceived(d.vpts.v);
		} else {
			LOG(("App Error: received messages.channelMessages in MainWidget::checkedHistory when no channel was passed!"));
		}

		App::feedUsers(d.vusers);
		App::feedChats(d.vchats);
		v = &d.vmessages.c_vector().v;
	} break;
	}
	if (!v) return;

	if (v->isEmpty()) {
		if ((profile && profile->peer() == peer) || (overview && overview->peer() == peer) || _stack.contains(peer) || history.peer() == peer) {
			showDialogs();
		}
		if (peer->isChat() && peer->asChat()->left) {
			dialogs.removePeer(peer);
		} else {
			History *h = App::historyLoaded(peer->id);
			if (h) Local::addSavedPeer(peer, h->lastMsgDate);
		}
	} else {
		History *h = App::historyLoaded(peer->id);
		if (!h->lastMsg) {
			h->addToBack((*v)[0], 0);
		}
	}
}

bool MainWidget::sendPhotoFail(uint64 randomId, const RPCError &error) {
	if (mtpIsFlood(error)) return false;

	if (error.type() == qsl("PHOTO_INVALID_DIMENSIONS")) {
		if (_resendImgRandomIds.isEmpty()) {
			ConfirmBox *box = new ConfirmBox(lang(lng_bad_image_for_photo));
			connect(box, SIGNAL(confirmed()), this, SLOT(onResendAsDocument()));
			connect(box, SIGNAL(cancelled()), this, SLOT(onCancelResend()));
			connect(box, SIGNAL(destroyed(QObject*)), this, SLOT(onCancelResend()));
			App::wnd()->showLayer(box);
		}
		_resendImgRandomIds.push_back(randomId);
		return true;
	}
	return sendMessageFail(error);
}

bool MainWidget::sendMessageFail(const RPCError &error) {
	if (mtpIsFlood(error)) return false;

	if (error.type() == qsl("PEER_FLOOD")) {
		App::wnd()->showLayer(new ConfirmBox(lng_cant_send_to_not_contact(lt_more_info, textcmdLink(qsl("https://telegram.org/faq?_hash=can-39t-send-messages-to-non-contacts"), lang(lng_cant_more_info))), true));
		return true;
	}
	return false;
}

void MainWidget::onResendAsDocument() {
	QList<uint64> tmp = _resendImgRandomIds;
	_resendImgRandomIds.clear();
	for (int32 i = 0, l = tmp.size(); i < l; ++i) {
		if (HistoryItem *item = App::histItemById(App::histItemByRandom(tmp.at(i)))) {
			if (HistoryPhoto *media = dynamic_cast<HistoryPhoto*>(item->getMedia())) {
				PhotoData *photo = media->photo();
				if (!photo->full->isNull()) {
					photo->full->forget();
					QByteArray data = photo->full->savedData();
					if (!data.isEmpty()) {
						history.uploadMedia(data, ToPrepareDocument, item->history()->peer->id);
					}
				}
			}
			item->destroy();
		}
	}
	App::wnd()->layerHidden();
}

void MainWidget::onCancelResend() {
	QList<uint64> tmp = _resendImgRandomIds;
	_resendImgRandomIds.clear();
	for (int32 i = 0, l = tmp.size(); i < l; ++i) {
		if (HistoryItem *item = App::histItemById(App::histItemByRandom(tmp.at(i)))) {
			item->destroy();
		}
	}
}

void MainWidget::onCacheBackground() {
	const QPixmap &bg(*cChatBackground());
	if (cTileBackground()) {
		QImage result(_willCacheFor.width() * cIntRetinaFactor(), _willCacheFor.height() * cIntRetinaFactor(), QImage::Format_RGB32);
        result.setDevicePixelRatio(cRetinaFactor());
		{
			QPainter p(&result);
			int left = 0, top = 0, right = _willCacheFor.width(), bottom = _willCacheFor.height();
			float64 w = bg.width() / cRetinaFactor(), h = bg.height() / cRetinaFactor();
			int sx = 0, sy = 0, cx = qCeil(_willCacheFor.width() / w), cy = qCeil(_willCacheFor.height() / h);
			for (int i = sx; i < cx; ++i) {
				for (int j = sy; j < cy; ++j) {
					p.drawPixmap(QPointF(i * w, j * h), bg);
				}
			}
		}
		_cachedX = 0;
		_cachedY = 0;
		_cachedBackground = QPixmap::fromImage(result);
	} else {
		QRect to, from;
		backgroundParams(_willCacheFor, to, from);
		_cachedX = to.x();
		_cachedY = to.y();
		_cachedBackground = QPixmap::fromImage(bg.toImage().copy(from).scaled(to.width() * cIntRetinaFactor(), to.height() * cIntRetinaFactor(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
		_cachedBackground.setDevicePixelRatio(cRetinaFactor());
	}
	_cachedFor = _willCacheFor;
}

void MainWidget::forwardSelectedItems() {
	if (overview) {
		overview->onForwardSelected();
	} else {
		history.onForwardSelected();
	}
}

void MainWidget::deleteSelectedItems() {
	if (overview) {
		overview->onDeleteSelected();
	} else {
		history.onDeleteSelected();
	}
}

void MainWidget::clearSelectedItems() {
	if (overview) {
		overview->onClearSelected();
	} else {
		history.onClearSelected();
	}
}

DialogsIndexed &MainWidget::contactsList() {
	return dialogs.contactsList();
}

DialogsIndexed &MainWidget::dialogsList() {
	return dialogs.dialogsList();
}

QString cleanMessage(const QString &text) {
	QString result = text;
	QChar *_start = result.data(), *_end = _start + result.size(), *start = _start, *end = _end, *ch = start, *copy = 0;
	for (; ch != end; ++ch) {
		if (ch->unicode() == '\r') {
			copy = ch + 1;
			break;
		} else if (MessageField::replaceCharBySpace(ch->unicode())) {
			*ch = ' ';
		}
	}
	if (copy) {
		for (; copy != end; ++copy) {
			if (copy->unicode() == '\r') {
				continue;
			} else if (MessageField::replaceCharBySpace(copy->unicode())) {
				*ch++ = ' ';
			} else {
				*ch++ = *copy;
			}
		}
		end = ch;
	}

	// PHP trim() removes [ \t\n\r\x00\x0b], we have removed [\t\r\x00\x0b] before, so
	for (; start != end; ++start) {
		if (start->unicode() != ' ' && start->unicode() != '\n') {
			break;
		}
	}
	for (QChar *e = end - 1; start != end; end = e) {
		if (e->unicode() != ' ' && e->unicode() != '\n') {
			break;
		}
		--e;
	}
	if (start == end) {
		return QString();
	} else if (start > _start) {
		return QString(start, end - start);
	} else if (end < _end) {
		result.resize(end - start);
	}
	return result;
}

void MainWidget::sendPreparedText(History *hist, const QString &text, MsgId replyTo, WebPageId webPageId) {
	saveRecentHashtags(text);
	QString sendingText, leftText = text;
	if (replyTo < 0) replyTo = history.replyToId();
	while (textSplit(sendingText, leftText, MaxMessageSize)) {
		FullMsgId newId(peerToChannel(hist->peer->id), clientMsgId());
		uint64 randomId = MTP::nonce<uint64>();

		sendingText = cleanMessage(sendingText);

		App::historyRegRandom(randomId, newId);
		App::historyRegSentData(randomId, hist->peer->id, sendingText);

		MTPstring msgText(MTP_string(sendingText));
		int32 flags = newMessageFlags(hist->peer) | MTPDmessage::flag_entities; // unread, out
		int32 sendFlags = 0;
		if (replyTo) {
			flags |= MTPDmessage::flag_reply_to_msg_id;
			sendFlags |= MTPmessages_SendMessage::flag_reply_to_msg_id;
		}
		MTPMessageMedia media = MTP_messageMediaEmpty();
		if (webPageId == 0xFFFFFFFFFFFFFFFFULL) {
			sendFlags |= MTPmessages_SendMessage_flag_skipWebPage;
		} else if (webPageId) {
			WebPageData *page = App::webPage(webPageId);
			media = MTP_messageMediaWebPage(MTP_webPagePending(MTP_long(page->id), MTP_int(page->pendingTill)));
			flags |= MTPDmessage::flag_media;
		}
		bool fromChannelName = hist->peer->isChannel();
		if (fromChannelName) {
			sendFlags |= MTPmessages_SendMessage_flag_broadcast;
			flags |= MTPDmessage::flag_views;
		} else {
			flags |= MTPDmessage::flag_from_id;
		}
		MTPVector<MTPMessageEntity> localEntities = linksToMTP(textParseLinks(sendingText, itemTextParseOptions(hist, App::self()).flags));
		hist->addToBack(MTP_message(MTP_int(flags), MTP_int(newId.msg), MTP_int(fromChannelName ? 0 : MTP::authedId()), peerToMTP(hist->peer->id), MTPPeer(), MTPint(), MTP_int(replyTo), MTP_int(unixtime()), msgText, media, MTPnullMarkup, localEntities, MTP_int(1)));
		hist->sendRequestId = MTP::send(MTPmessages_SendMessage(MTP_int(sendFlags), hist->peer->input, MTP_int(replyTo), msgText, MTP_long(randomId), MTPnullMarkup, localEntities), rpcDone(&MainWidget::sentUpdatesReceived, randomId), rpcFail(&MainWidget::sendMessageFail), 0, 0, hist->sendRequestId);
	}

	finishForwarding(hist);
}

void MainWidget::sendMessage(History *hist, const QString &text, MsgId replyTo) {
	hist->getReadyFor(ShowAtTheEndMsgId);
	readServerHistory(hist, false);
	sendPreparedText(hist, history.prepareMessage(text), replyTo);
}

void MainWidget::saveRecentHashtags(const QString &text) {
	bool found = false;
	QRegularExpressionMatch m;
	RecentHashtagPack recent(cRecentWriteHashtags());
	for (int32 i = 0, next = 0; (m = reHashtag().match(text, i)).hasMatch(); i = next) {
		i = m.capturedStart();
		next = m.capturedEnd();
		if (m.hasMatch()) {
			if (!m.capturedRef(1).isEmpty()) {
				++i;
			}
			if (!m.capturedRef(2).isEmpty()) {
				--next;
			}
		}
		if (!found && cRecentWriteHashtags().isEmpty() && cRecentSearchHashtags().isEmpty()) {
			Local::readRecentHashtags();
			recent = cRecentWriteHashtags();
		}
		found = true;
		incrementRecentHashtag(recent, text.mid(i + 1, next - i - 1));
	}
	if (found) {
		cSetRecentWriteHashtags(recent);
		Local::writeRecentHashtags();
	}
}

void MainWidget::readServerHistory(History *hist, bool force) {
	if (!hist || (!force && !hist->unreadCount)) return;
    
    ReadRequests::const_iterator i = _readRequests.constFind(hist->peer);
	MsgId upTo = hist->inboxRead(0);
    if (i == _readRequests.cend()) {
		sendReadRequest(hist->peer, upTo);
	} else {
		ReadRequestsPending::iterator i = _readRequestsPending.find(hist->peer);
		if (i == _readRequestsPending.cend()) {
			_readRequestsPending.insert(hist->peer, upTo);
		} else if (i.value() < upTo) {
			i.value() = upTo;
		}
	}
}

uint64 MainWidget::animActiveTime(MsgId id) const {
	return history.animActiveTime(id);
}

void MainWidget::stopAnimActive() {
	history.stopAnimActive();
}

void MainWidget::sendBotCommand(const QString &cmd, MsgId replyTo) {
	history.sendBotCommand(cmd, replyTo);
}

void MainWidget::insertBotCommand(const QString &cmd) {
	history.insertBotCommand(cmd);
}

void MainWidget::searchMessages(const QString &query) {
	App::wnd()->hideMediaview();
	dialogs.searchMessages(query);
	if (!cWideMode()) showDialogs();
}

void MainWidget::preloadOverviews(PeerData *peer) {
	History *h = App::history(peer->id);
	bool sending[OverviewCount] = { false };
	for (int32 i = 0; i < OverviewCount; ++i) {
		if (h->_overviewCount[i] < 0) {
			if (_overviewPreload[i].constFind(peer) == _overviewPreload[i].cend()) {
				sending[i] = true;
			}
		}
	}
	int32 last = OverviewCount;
	while (last > 0) {
		if (sending[--last]) break;
	}
	for (int32 i = 0; i < OverviewCount; ++i) {
		if (sending[i]) {
			MediaOverviewType type = MediaOverviewType(i);
			MTPMessagesFilter filter = typeToMediaFilter(type);
			if (type == OverviewCount) break;

			int32 flags = peer->isChannel() ? MTPmessages_Search_flag_only_important : 0;
			_overviewPreload[i].insert(peer, MTP::send(MTPmessages_Search(MTP_int(flags), peer->input, MTP_string(""), filter, MTP_int(0), MTP_int(0), MTP_int(0), MTP_int(0), MTP_int(0)), rpcDone(&MainWidget::overviewPreloaded, peer), rpcFail(&MainWidget::overviewFailed, peer), 0, (i == last) ? 0 : 10));
		}
	}
}

void MainWidget::overviewPreloaded(PeerData *peer, const MTPmessages_Messages &result, mtpRequestId req) {
	MediaOverviewType type = OverviewCount;
	for (int32 i = 0; i < OverviewCount; ++i) {
		OverviewsPreload::iterator j = _overviewPreload[i].find(peer);
		if (j != _overviewPreload[i].end() && j.value() == req) {
			type = MediaOverviewType(i);
			_overviewPreload[i].erase(j);
			break;
		}
	}

	if (type == OverviewCount) return;

	History *h = App::history(peer->id);
	switch (result.type()) {
	case mtpc_messages_messages: {
		const MTPDmessages_messages &d(result.c_messages_messages());
		App::feedUsers(d.vusers);
		App::feedChats(d.vchats);
		h->_overviewCount[type] = d.vmessages.c_vector().v.size();
	} break;

	case mtpc_messages_messagesSlice: {
		const MTPDmessages_messagesSlice &d(result.c_messages_messagesSlice());
		App::feedUsers(d.vusers);
		App::feedChats(d.vchats);
		h->_overviewCount[type] = d.vcount.v;
	} break;

	case mtpc_messages_channelMessages: {
		const MTPDmessages_channelMessages &d(result.c_messages_channelMessages());
		if (peer && peer->isChannel()) {
			peer->asChannel()->ptsReceived(d.vpts.v);
		} else {
			LOG(("App Error: received messages.channelMessages in MainWidget::overviewPreloaded when no channel was passed!"));
		}

		App::feedUsers(d.vusers);
		App::feedChats(d.vchats);
		h->_overviewCount[type] = d.vcount.v;
	} break;

	default: return;
	}

	if (h->_overviewCount[type] > 0) {
		for (History::MediaOverviewIds::const_iterator i = h->_overviewIds[type].cbegin(), e = h->_overviewIds[type].cend(); i != e; ++i) {
			if (i.key() < 0) {
				++h->_overviewCount[type];
			} else {
				break;
			}
		}
	}

	mediaOverviewUpdated(peer, type);
}

void MainWidget::mediaOverviewUpdated(PeerData *peer, MediaOverviewType type) {
	if (profile) profile->mediaOverviewUpdated(peer, type);
	if (!_player.isHidden()) _player.mediaOverviewUpdated(peer, type);
	if (overview && overview->peer() == peer) {
		overview->mediaOverviewUpdated(peer, type);

		int32 mask = 0;
		History *h = peer ? App::historyLoaded(peer->id) : 0;
		if (h) {
			for (int32 i = 0; i < OverviewCount; ++i) {
				if (!h->_overview[i].isEmpty() || h->_overviewCount[i] > 0 || i == overview->type()) {
					mask |= (1 << i);
				}
			}
		}
		if (mask != _mediaTypeMask) {
			_mediaType.resetButtons();
			for (int32 i = 0; i < OverviewCount; ++i) {
				if (mask & (1 << i)) {
					switch (i) {
					case OverviewPhotos: connect(_mediaType.addButton(new IconedButton(this, st::dropdownMediaPhotos, lang(lng_media_type_photos))), SIGNAL(clicked()), this, SLOT(onPhotosSelect())); break;
					case OverviewVideos: connect(_mediaType.addButton(new IconedButton(this, st::dropdownMediaVideos, lang(lng_media_type_videos))), SIGNAL(clicked()), this, SLOT(onVideosSelect())); break;
					case OverviewDocuments: connect(_mediaType.addButton(new IconedButton(this, st::dropdownMediaDocuments, lang(lng_media_type_files))), SIGNAL(clicked()), this, SLOT(onDocumentsSelect())); break;
					case OverviewAudios: connect(_mediaType.addButton(new IconedButton(this, st::dropdownMediaAudios, lang(lng_media_type_audios))), SIGNAL(clicked()), this, SLOT(onAudiosSelect())); break;
					case OverviewLinks: connect(_mediaType.addButton(new IconedButton(this, st::dropdownMediaLinks, lang(lng_media_type_links))), SIGNAL(clicked()), this, SLOT(onLinksSelect())); break;
					}
				}
			}
			_mediaTypeMask = mask;
			_mediaType.move(width() - _mediaType.width(), st::topBarHeight);
			overview->updateTopBarSelection();
		}
	}
}

void MainWidget::changingMsgId(HistoryItem *row, MsgId newId) {
	if (overview) overview->changingMsgId(row, newId);
}

void MainWidget::itemRemoved(HistoryItem *item) {
	api()->itemRemoved(item);
	dialogs.itemRemoved(item);
	if (history.peer() == item->history()->peer) {
		history.itemRemoved(item);
	}
	if (overview && overview->peer() == item->history()->peer) {
		overview->itemRemoved(item);
	}
	itemRemovedGif(item);
	if (!_toForward.isEmpty()) {
		SelectedItemSet::iterator i = _toForward.find(item->id);
		if (i != _toForward.end()) {
			_toForward.erase(i);
			updateForwardingTexts();
		}
	}
}

void MainWidget::itemReplaced(HistoryItem *oldItem, HistoryItem *newItem) {
	api()->itemReplaced(oldItem, newItem);
	dialogs.itemReplaced(oldItem, newItem);
	if (history.peer() == newItem->history()->peer) {
		history.itemReplaced(oldItem, newItem);
	}
	itemReplacedGif(oldItem, newItem);
	if (!_toForward.isEmpty()) {
		SelectedItemSet::iterator i = _toForward.find(oldItem->id);
		if (i != _toForward.end()) {
			i.value() = newItem;
		}
	}
}

void MainWidget::itemResized(HistoryItem *row, bool scrollToIt) {
	if (!row || (history.peer() == row->history()->peer && !row->detached())) {
		history.itemResized(row, scrollToIt);
	} else if (row) {
		row->history()->width = 0;
		if (history.peer() == row->history()->peer) {
			history.resizeEvent(0);
		}
	}
	if (overview) {
		overview->itemResized(row, scrollToIt);
	}
	if (row) msgUpdated(row->history()->peer->id, row);
}

bool MainWidget::overviewFailed(PeerData *peer, const RPCError &error, mtpRequestId req) {
	if (error.type().startsWith(qsl("FLOOD_WAIT_"))) return false;

	MediaOverviewType type = OverviewCount;
	for (int32 i = 0; i < OverviewCount; ++i) {
		OverviewsPreload::iterator j = _overviewPreload[i].find(peer);
		if (j != _overviewPreload[i].end() && j.value() == req) {
			_overviewPreload[i].erase(j);
			break;
		}
	}
	return true;
}

void MainWidget::loadMediaBack(PeerData *peer, MediaOverviewType type, bool many) {
	if (_overviewLoad[type].constFind(peer) != _overviewLoad[type].cend()) return;

	MsgId minId = 0;
	History *hist = App::history(peer->id);
	if (hist->_overviewCount[type] == 0) return; // all loaded

	for (History::MediaOverviewIds::const_iterator i = hist->_overviewIds[type].cbegin(), e = hist->_overviewIds[type].cend(); i != e; ++i) {
		if (i.key() > 0) {
			minId = i.key();
			break;
		}
	}
	int32 limit = many ? SearchManyPerPage : (hist->_overview[type].size() > MediaOverviewStartPerPage) ? SearchPerPage : MediaOverviewStartPerPage;
	MTPMessagesFilter filter = typeToMediaFilter(type);
	if (type == OverviewCount) return;

	int32 flags = peer->isChannel() ? MTPmessages_Search_flag_only_important : 0;
	_overviewLoad[type].insert(peer, MTP::send(MTPmessages_Search(MTP_int(flags), peer->input, MTPstring(), filter, MTP_int(0), MTP_int(0), MTP_int(0), MTP_int(minId), MTP_int(limit)), rpcDone(&MainWidget::photosLoaded, hist)));
}

void MainWidget::peerUsernameChanged(PeerData *peer) {
	if (profile && profile->peer() == peer) {
		profile->update();
	}
	if (App::settings() && peer == App::self()) {
		App::settings()->usernameChanged();
	}
}

void MainWidget::checkLastUpdate(bool afterSleep) {
	uint64 n = getms(true);
	if (_lastUpdateTime && n > _lastUpdateTime + (afterSleep ? NoUpdatesAfterSleepTimeout : NoUpdatesTimeout)) {
		_lastUpdateTime = n;
		MTP::ping();
	}
}

void MainWidget::showAddContact() {
	dialogs.onAddContact();
}

void MainWidget::showNewGroup() {
	dialogs.onNewGroup();
}

void MainWidget::photosLoaded(History *h, const MTPmessages_Messages &msgs, mtpRequestId req) {
	OverviewsPreload::iterator it;
	MediaOverviewType type = OverviewCount;
	for (int32 i = 0; i < OverviewCount; ++i) {
		it = _overviewLoad[i].find(h->peer);
		if (it != _overviewLoad[i].cend()) {
			type = MediaOverviewType(i);
			_overviewLoad[i].erase(it);
			break;
		}
	}
	if (type == OverviewCount) return;

	const QVector<MTPMessage> *v = 0;
	switch (msgs.type()) {
	case mtpc_messages_messages: {
		const MTPDmessages_messages &d(msgs.c_messages_messages());
		App::feedUsers(d.vusers);
		App::feedChats(d.vchats);
		v = &d.vmessages.c_vector().v;
		h->_overviewCount[type] = 0;
	} break;

	case mtpc_messages_messagesSlice: {
		const MTPDmessages_messagesSlice &d(msgs.c_messages_messagesSlice());
		App::feedUsers(d.vusers);
		App::feedChats(d.vchats);
		h->_overviewCount[type] = d.vcount.v;
		v = &d.vmessages.c_vector().v;
	} break;

	case mtpc_messages_channelMessages: {
		const MTPDmessages_channelMessages &d(msgs.c_messages_channelMessages());
		if (h && h->peer->isChannel()) {
			h->peer->asChannel()->ptsReceived(d.vpts.v);
		} else {
			LOG(("App Error: received messages.channelMessages in MainWidget::photosLoaded when no channel was passed!"));
		}

		App::feedUsers(d.vusers);
		App::feedChats(d.vchats);
		h->_overviewCount[type] = d.vcount.v;
		v = &d.vmessages.c_vector().v;
	} break;

	default: return;
	}

	if (h->_overviewCount[type] > 0) {
		for (History::MediaOverviewIds::const_iterator i = h->_overviewIds[type].cbegin(), e = h->_overviewIds[type].cend(); i != e; ++i) {
			if (i.key() < 0) {
				++h->_overviewCount[type];
			} else {
				break;
			}
		}
	}
	if (v->isEmpty()) {
		h->_overviewCount[type] = 0;
	}

	for (QVector<MTPMessage>::const_iterator i = v->cbegin(), e = v->cend(); i != e; ++i) {
		HistoryItem *item = App::histories().addToBack(*i, -1);
		if (item && h->_overviewIds[type].constFind(item->id) == h->_overviewIds[type].cend()) {
			h->_overviewIds[type].insert(item->id, NullType());
			h->_overview[type].push_front(item->id);
		}
	}
	if (App::wnd()) App::wnd()->mediaOverviewUpdated(h->peer, type);
}

void MainWidget::sendReadRequest(PeerData *peer, MsgId upTo) {
	if (!MTP::authedId()) return;
	if (peer->isChannel()) {
		_readRequests.insert(peer, MTP::send(MTPmessages_ReadChannelHistory(peer->input, MTP_int(upTo)), rpcDone(&MainWidget::channelWasRead, peer), rpcFail(&MainWidget::readRequestFail, peer)));
	} else {
		_readRequests.insert(peer, MTP::send(MTPmessages_ReadHistory(peer->input, MTP_int(upTo), MTP_int(0)), rpcDone(&MainWidget::partWasRead, peer), rpcFail(&MainWidget::readRequestFail, peer)));
	}
}

void MainWidget::channelWasRead(PeerData *peer, const MTPBool &result) {
	readRequestDone(peer);
}

void MainWidget::partWasRead(PeerData *peer, const MTPmessages_AffectedHistory &result) {
	const MTPDmessages_affectedHistory &d(result.c_messages_affectedHistory());
	ptsUpdated(d.vpts.v, d.vpts_count.v);
    
	int32 offset = d.voffset.v;
	if (!MTP::authedId() || offset <= 0 || true) {
		readRequestDone(peer);
    } else {
//        _readRequests[peer] = MTP::send(MTPmessages_ReadHistory(peer->input, MTP_int(upTo), MTP_int(offset)), rpcDone(&MainWidget::partWasRead, peer));
    }
}

bool MainWidget::readRequestFail(PeerData *peer, const RPCError &error) {
	if (error.type().startsWith(qsl("FLOOD_WAIT_"))) return false;

	readRequestDone(peer);
	return false;
}

void MainWidget::readRequestDone(PeerData *peer) {
	_readRequests.remove(peer);
	ReadRequestsPending::iterator i = _readRequestsPending.find(peer);
	if (i != _readRequestsPending.cend()) {
		sendReadRequest(peer, i.value());
		_readRequestsPending.erase(i);
	}
}

void MainWidget::messagesAffected(PeerData *peer, const MTPmessages_AffectedMessages &result) {
	const MTPDmessages_affectedMessages &d(result.c_messages_affectedMessages());
	if (peer && peer->isChannel()) {
		peer->asChannel()->ptsUpdated(d.vpts.v, d.vpts_count.v);
	} else {
		ptsUpdated(d.vpts.v, d.vpts_count.v);
	}
}

void MainWidget::videoLoadProgress(mtpFileLoader *loader) {
	VideoData *video = App::video(loader->objId());
	if (video->loader) {
		if (video->loader->done()) {
			video->finish();
			QString already = video->already();
			if (!already.isEmpty() && video->openOnSave) {
				QPoint pos(QCursor::pos());
				if (video->openOnSave < 0 && !psShowOpenWithMenu(pos.x(), pos.y(), already)) {
					psOpenFile(already, true);
				} else {
					psOpenFile(already, video->openOnSave < 0);
				}
			}
		}
	}
	const VideoItems &items(App::videoItems());
	VideoItems::const_iterator i = items.constFind(video);
	if (i != items.cend()) {
		for (HistoryItemsMap::const_iterator j = i->cbegin(), e = i->cend(); j != e; ++j) {
			msgUpdated(j.key()->history()->peer->id, j.key());
		}
	}
}

void MainWidget::loadFailed(mtpFileLoader *loader, bool started, const char *retrySlot) {
	failedObjId = loader->objId();
	failedFileName = loader->fileName();
	ConfirmBox *box = new ConfirmBox(lang(started ? lng_download_finish_failed : lng_download_path_failed), started ? QString() : lang(lng_download_path_settings));
	if (started) {
		connect(box, SIGNAL(confirmed()), this, retrySlot);
	} else {
		connect(box, SIGNAL(confirmed()), App::wnd(), SLOT(showSettings()));
	}
	App::wnd()->showLayer(box);
}

void MainWidget::videoLoadFailed(mtpFileLoader *loader, bool started) {
	loadFailed(loader, started, SLOT(videoLoadRetry()));
	VideoData *video = App::video(loader->objId());
	if (video && video->loader) video->finish();
}

void MainWidget::videoLoadRetry() {
	App::wnd()->hideLayer();
	VideoData *video = App::video(failedObjId);
	if (video) video->save(failedFileName);
}

void MainWidget::audioLoadProgress(mtpFileLoader *loader) {
	AudioData *audio = App::audio(loader->objId());
	if (audio->loader) {
		if (audio->loader->done()) {
			audio->finish();
			QString already = audio->already();
			bool play = audio->openOnSave > 0 && audio->openOnSaveMsgId.msg && audioPlayer();
			if ((!already.isEmpty() && audio->openOnSave) || (!audio->data.isEmpty() && play)) {
				if (play) {
					AudioMsgId playing;
					AudioPlayerState state = AudioPlayerStopped;
					audioPlayer()->currentState(&playing, &state);
					if (playing.msgId == audio->openOnSaveMsgId && !(state & AudioPlayerStoppedMask) && state != AudioPlayerFinishing) {
						audioPlayer()->pauseresume(OverviewAudios);
					} else {
						audioPlayer()->play(AudioMsgId(audio, audio->openOnSaveMsgId));
						if (App::main()) App::main()->audioMarkRead(audio);
					}
				} else {
					QPoint pos(QCursor::pos());
					if (audio->openOnSave < 0 && !psShowOpenWithMenu(pos.x(), pos.y(), already)) {
						psOpenFile(already, true);
					} else {
						psOpenFile(already, audio->openOnSave < 0);
					}
					if (App::main()) App::main()->audioMarkRead(audio);
				}
			}
		}
	}
	const AudioItems &items(App::audioItems());
	AudioItems::const_iterator i = items.constFind(audio);
	if (i != items.cend()) {
		for (HistoryItemsMap::const_iterator j = i->cbegin(), e = i->cend(); j != e; ++j) {
			msgUpdated(j.key()->history()->peer->id, j.key());
		}
	}
}

void MainWidget::audioPlayProgress(const AudioMsgId &audioId) {
	AudioMsgId playing;
	AudioPlayerState state = AudioPlayerStopped;
	audioPlayer()->currentState(&playing, &state);
	if (playing == audioId && state == AudioPlayerStoppedAtStart) {
		audioPlayer()->clearStoppedAtStart(audioId);

		AudioData *audio = audioId.audio;
		QString already = audio->already(true);
		if (already.isEmpty() && !audio->data.isEmpty()) {
			bool mp3 = (audio->mime == qstr("audio/mp3"));
			QString filename = saveFileName(lang(lng_save_audio), mp3 ? qsl("MP3 Audio (*.mp3);;All files (*.*)") : qsl("OGG Opus Audio (*.ogg);;All files (*.*)"), qsl("audio"), mp3 ? qsl(".mp3") : qsl(".ogg"), false);
			if (!filename.isEmpty()) {
				QFile f(filename);
				if (f.open(QIODevice::WriteOnly)) {
					if (f.write(audio->data) == audio->data.size()) {
						f.close();
						already = filename;
						audio->location = FileLocation(mtpToStorageType(mtpc_storage_filePartial), filename);
						Local::writeFileLocation(mediaKey(mtpToLocationType(mtpc_inputAudioFileLocation), audio->dc, audio->id), FileLocation(mtpToStorageType(mtpc_storage_filePartial), filename));
					}
				}
			}
		}
		if (!already.isEmpty()) {
			psOpenFile(already);
		}
	}

	if (HistoryItem *item = App::histItemById(audioId.msgId)) {
		msgUpdated(item->history()->peer->id, item);
	}
}

void MainWidget::documentPlayProgress(const SongMsgId &songId) {
	SongMsgId playing;
	AudioPlayerState playingState = AudioPlayerStopped;
	int64 playingPosition = 0, playingDuration = 0;
	int32 playingFrequency = 0;
	audioPlayer()->currentState(&playing, &playingState, &playingPosition, &playingDuration, &playingFrequency);
	if (playing == songId && playingState == AudioPlayerStoppedAtStart) {
		playingState = AudioPlayerStopped;
		audioPlayer()->clearStoppedAtStart(songId);

		DocumentData *document = songId.song;
		QString already = document->already(true);
		if (already.isEmpty() && !document->data.isEmpty()) {
			QString name = document->name, filter;
			MimeType mimeType = mimeTypeForName(document->mime);
			QStringList p = mimeType.globPatterns();
			QString pattern = p.isEmpty() ? QString() : p.front();
			if (name.isEmpty()) {
				name = pattern.isEmpty() ? qsl(".unknown") : pattern.replace('*', QString());
			}
			if (pattern.isEmpty()) {
				filter = qsl("All files (*.*)");
			} else {
				filter = mimeType.filterString() + qsl(";;All files (*.*)");
			}
			QString filename = saveFileName(lang(lng_save_file), filter, qsl("doc"), name, false);
			if (!filename.isEmpty()) {
				QFile f(filename);
				if (f.open(QIODevice::WriteOnly)) {
					if (f.write(document->data) == document->data.size()) {
						f.close();
						already = filename;
						document->location = FileLocation(mtpToStorageType(mtpc_storage_filePartial), filename);
						Local::writeFileLocation(mediaKey(mtpToLocationType(mtpc_inputDocumentFileLocation), document->dc, document->id), FileLocation(mtpToStorageType(mtpc_storage_filePartial), filename));
					}
				}
			}
		}
		if (!already.isEmpty()) {
			psOpenFile(already);
		}
	}

	if (playing == songId) {
		_player.updateState(playing, playingState, playingPosition, playingDuration, playingFrequency);

		if (!(playingState & AudioPlayerStoppedMask) && playingState != AudioPlayerFinishing) {
			if (_player.isHidden()) {
				_player.clearSelection();
				_player.show();
				_playerHeight = _contentScrollAddToY = _player.height();
				resizeEvent(0);
			}
		}
	}

	if (HistoryItem *item = App::histItemById(songId.msgId)) {
		msgUpdated(item->history()->peer->id, item);
	}
}

void MainWidget::hidePlayer() {
	if (!_player.isHidden()) {
		_player.hide();
		_contentScrollAddToY = -_player.height();
		_playerHeight = 0;
		resizeEvent(0);
	}
}

void MainWidget::audioLoadFailed(mtpFileLoader *loader, bool started) {
	loadFailed(loader, started, SLOT(audioLoadRetry()));
	AudioData *audio = App::audio(loader->objId());
	if (audio) {
		audio->status = FileFailed;
		if (audio->loader) audio->finish();
	}
}

void MainWidget::audioLoadRetry() {
	App::wnd()->hideLayer();
	AudioData *audio = App::audio(failedObjId);
	if (audio) audio->save(failedFileName);
}

void MainWidget::documentLoadProgress(mtpFileLoader *loader) {
	bool songPlayActivated = false;
	DocumentData *document = App::document(loader->objId());
	if (document->loader) {
		if (document->loader->done()) {
			document->finish();
			QString already = document->already();

			HistoryItem *item = (document->openOnSave && document->openOnSaveMsgId.msg) ? App::histItemById(document->openOnSaveMsgId) : 0;
			bool play = document->song() && audioPlayer() && document->openOnSave && item;
			if ((!already.isEmpty() || (!document->data.isEmpty() && play)) && document->openOnSave) {
				if (play) {
					SongMsgId playing;
					AudioPlayerState playingState = AudioPlayerStopped;
					audioPlayer()->currentState(&playing, &playingState);
					if (playing.msgId == item->fullId() && !(playingState & AudioPlayerStoppedMask) && playingState != AudioPlayerFinishing) {
						audioPlayer()->pauseresume(OverviewDocuments);
					} else {
						SongMsgId song(document, item->fullId());
						audioPlayer()->play(song);
						if (App::main()) App::main()->documentPlayProgress(song);
					}

					songPlayActivated = true;
				} else if(document->openOnSave > 0 && document->size < MediaViewImageSizeLimit) {
					QImageReader reader(already);
					if (reader.canRead()) {
						if (reader.supportsAnimation() && reader.imageCount() > 1 && item) {
							startGif(item, already);
						} else if (item) {
							App::wnd()->showDocument(document, item);
						} else {
							psOpenFile(already);
						}
					} else {
						psOpenFile(already);
					}
				} else {
					QPoint pos(QCursor::pos());
					if (document->openOnSave < 0 && !psShowOpenWithMenu(pos.x(), pos.y(), already)) {
						psOpenFile(already, true);
					} else {
						psOpenFile(already, document->openOnSave < 0);
					}
				}
			}
		}
	}
	const DocumentItems &items(App::documentItems());
	DocumentItems::const_iterator i = items.constFind(document);
	if (i != items.cend()) {
		for (HistoryItemsMap::const_iterator j = i->cbegin(), e = i->cend(); j != e; ++j) {
			msgUpdated(j.key()->history()->peer->id, j.key());
		}
	}
	App::wnd()->documentUpdated(document);

	if (!songPlayActivated && audioPlayer()) {
		SongMsgId playing;
		AudioPlayerState playingState = AudioPlayerStopped;
		int64 playingPosition = 0, playingDuration = 0;
		int32 playingFrequency = 0;
		audioPlayer()->currentState(&playing, &playingState, &playingPosition, &playingDuration, &playingFrequency);
		if (playing.song == document && !_player.isHidden()) {
			if (document->loader) {
				_player.updateState(playing, playingState, playingPosition, playingDuration, playingFrequency);
			} else {
				audioPlayer()->play(playing);
			}
		}
	}
}

void MainWidget::documentLoadFailed(mtpFileLoader *loader, bool started) {
	loadFailed(loader, started, SLOT(documentLoadRetry()));
	DocumentData *document = App::document(loader->objId());
	if (document) {
		if (document->loader) document->finish();
		document->status = FileFailed;
	}
}

void MainWidget::documentLoadRetry() {
	App::wnd()->hideLayer();
	DocumentData *document = App::document(failedObjId);
	if (document) document->save(failedFileName);
}

void MainWidget::audioMarkRead(AudioData *data) {
	const AudioItems &items(App::audioItems());
	AudioItems::const_iterator i = items.constFind(data);
	if (i != items.cend()) {
		mediaMarkRead(i.value());
	}
}

void MainWidget::videoMarkRead(VideoData *data) {
	const VideoItems &items(App::videoItems());
	VideoItems::const_iterator i = items.constFind(data);
	if (i != items.cend()) {
		mediaMarkRead(i.value());
	}
}

void MainWidget::mediaMarkRead(const HistoryItemsMap &items) {
	QVector<MTPint> markedIds;
	markedIds.reserve(items.size());
	for (HistoryItemsMap::const_iterator j = items.cbegin(), e = items.cend(); j != e; ++j) {
		if (!j.key()->out() && j.key()->isMediaUnread()) {
			j.key()->markMediaRead();
			if (j.key()->id > 0) {
				markedIds.push_back(MTP_int(j.key()->id));
			}
		}
	}
	if (!markedIds.isEmpty()) {
		MTP::send(MTPmessages_ReadMessageContents(MTP_vector<MTPint>(markedIds)), rpcDone(&MainWidget::messagesAffected, (PeerData*)0));
	}
}

void MainWidget::onParentResize(const QSize &newSize) {
	resize(newSize);
}

void MainWidget::updateOnlineDisplay() {
	if (this != App::main()) return;
	history.updateOnlineDisplay(history.x(), width() - history.x() - st::sysBtnDelta * 2 - st::sysCls.img.pxWidth() - st::sysRes.img.pxWidth() - st::sysMin.img.pxWidth());
	if (profile) profile->updateOnlineDisplay();
	if (App::wnd()->settingsWidget()) App::wnd()->settingsWidget()->updateOnlineDisplay();
}

void MainWidget::confirmShareContact(bool ctrlShiftEnter, const QString &phone, const QString &fname, const QString &lname, MsgId replyTo) {
	history.confirmShareContact(ctrlShiftEnter, phone, fname, lname, replyTo);
}

void MainWidget::confirmSendImage(const ReadyLocalMedia &img) {
	bool lastKeyboardUsed = history.lastForceReplyReplied(FullMsgId(peerToChannel(img.peer), img.replyTo));
	history.confirmSendImage(img);
	history.cancelReply(lastKeyboardUsed);
}

void MainWidget::confirmSendImageUncompressed(bool ctrlShiftEnter, MsgId replyTo) {
	history.uploadConfirmImageUncompressed(ctrlShiftEnter, replyTo);
}

void MainWidget::cancelSendImage() {
	history.cancelSendImage();
}

void MainWidget::dialogsCancelled() {
	if (_hider) {
		_hider->startHide();
		noHider(_hider);
	}
	history.activate();
}

void MainWidget::serviceNotification(const QString &msg, const MTPMessageMedia &media, bool unread) {
	int32 flags = (unread ? MTPDmessage_flag_unread : 0) | MTPDmessage::flag_entities | MTPDmessage::flag_from_id;
	QString sendingText, leftText = msg;
	HistoryItem *item = 0;
	while (textSplit(sendingText, leftText, MaxMessageSize)) {
		MTPVector<MTPMessageEntity> localEntities = linksToMTP(textParseLinks(sendingText, _historyTextOptions.flags));
		item = App::histories().addToBack(MTP_message(MTP_int(flags), MTP_int(clientMsgId()), MTP_int(ServiceUserId), MTP_peerUser(MTP_int(MTP::authedId())), MTPPeer(), MTPint(), MTPint(), MTP_int(unixtime()), MTP_string(sendingText), media, MTPnullMarkup, localEntities, MTPint()), unread ? 1 : 2);
	}
	if (item) {
		history.peerMessagesUpdated(item->history()->peer->id);
	}
}

void MainWidget::serviceHistoryDone(const MTPmessages_Messages &msgs) {
	switch (msgs.type()) {
	case mtpc_messages_messages: {
		const MTPDmessages_messages &d(msgs.c_messages_messages());
		App::feedUsers(d.vusers);
		App::feedChats(d.vchats);
		App::feedMsgs(d.vmessages);
	} break;

	case mtpc_messages_messagesSlice: {
		const MTPDmessages_messagesSlice &d(msgs.c_messages_messagesSlice());
		App::feedUsers(d.vusers);
		App::feedChats(d.vchats);
		App::feedMsgs(d.vmessages);
	} break;

	case mtpc_messages_channelMessages: {
		const MTPDmessages_channelMessages &d(msgs.c_messages_channelMessages());
		LOG(("App Error: received messages.channelMessages in MainWidget::serviceHistoryDone!"));

		App::feedUsers(d.vusers);
		App::feedChats(d.vchats);
		App::feedMsgs(d.vmessages);
	} break;
	}

	App::wnd()->showDelayedServiceMsgs();
}

bool MainWidget::serviceHistoryFail(const RPCError &error) {
	if (error.type().startsWith(qsl("FLOOD_WAIT_"))) return false;

	App::wnd()->showDelayedServiceMsgs();
	return false;
}

bool MainWidget::isIdle() const {
	return _isIdle;
}

void MainWidget::clearCachedBackground() {
	_cachedBackground = QPixmap();
	_cacheBackgroundTimer.stop();
}

QPixmap MainWidget::cachedBackground(const QRect &forRect, int &x, int &y) {
	if (!_cachedBackground.isNull() && forRect == _cachedFor) {
		x = _cachedX;
		y = _cachedY;
		return _cachedBackground;
	}
	if (_willCacheFor != forRect || !_cacheBackgroundTimer.isActive()) {
		_willCacheFor = forRect;
		_cacheBackgroundTimer.start(CacheBackgroundTimeout);
	}
	return QPixmap();
}

void MainWidget::backgroundParams(const QRect &forRect, QRect &to, QRect &from) const {
	const QSize &bg(cChatBackground()->size());
	if (uint64(bg.width()) * forRect.height() > uint64(bg.height()) * forRect.width()) {
		float64 pxsize = forRect.height() / float64(bg.height());
		int takewidth = qCeil(forRect.width() / pxsize);
		if (takewidth > bg.width()) {
			takewidth = bg.width();
		} else if ((bg.width() % 2) != (takewidth % 2)) {
			++takewidth;
		}
		to = QRect(int((forRect.width() - takewidth * pxsize) / 2.), 0, qCeil(takewidth * pxsize), forRect.height());
		from = QRect((bg.width() - takewidth) / 2, 0, takewidth, bg.height());
	} else {
		float64 pxsize = forRect.width() / float64(bg.width());
		int takeheight = qCeil(forRect.height() / pxsize);
		if (takeheight > bg.height()) {
			takeheight = bg.height();
		} else if ((bg.height() % 2) != (takeheight % 2)) {
			++takeheight;
		}
		to = QRect(0, int((forRect.height() - takeheight * pxsize) / 2.), forRect.width(), qCeil(takeheight * pxsize));
		from = QRect(0, (bg.height() - takeheight) / 2, bg.width(), takeheight);
	}
}

void MainWidget::updateScrollColors() {
	history.updateScrollColors();
	if (overview) overview->updateScrollColors();
}

void MainWidget::setChatBackground(const App::WallPaper &wp) {
	_background = new App::WallPaper(wp);
	_background->full->load();
	checkChatBackground();
}

bool MainWidget::chatBackgroundLoading() {
	return !!_background;
}

void MainWidget::checkChatBackground() {
	if (_background) {
		if (_background->full->loaded()) {
			if (_background->full->isNull()) {
				App::initBackground();
			} else if (_background->id == 0 || _background->id == DefaultChatBackground) {
				App::initBackground(_background->id);
			} else {
				App::initBackground(_background->id, _background->full->pix().toImage());
			}
			delete _background;
			_background = 0;
			QTimer::singleShot(0, this, SLOT(update()));
		}
	}
}

ImagePtr MainWidget::newBackgroundThumb() {
	return _background ? _background->thumb : ImagePtr();
}

ApiWrap *MainWidget::api() {
	return _api;
}

void MainWidget::updateReplyTo() {
	history.updateReplyTo(true);
}

void MainWidget::updateBotKeyboard() {
	history.updateBotKeyboard();
}

void MainWidget::pushReplyReturn(HistoryItem *item) {
	history.pushReplyReturn(item);
}

void MainWidget::setInnerFocus() {
	if (_hider || !history.peer()) {
		if (_hider && _hider->wasOffered()) {
			_hider->setFocus();
		} else if (overview) {
			overview->activate();
		} else if (profile) {
			profile->activate();
		} else {
			dialogsActivate();
		}
	} else if (profile) {
		profile->setFocus();
	} else {
		history.activate();
	}
}

void MainWidget::createDialogAtTop(History *history, int32 unreadCount) {
	dialogs.createDialogAtTop(history, unreadCount);
}

void MainWidget::choosePeer(PeerId peerId, MsgId showAtMsgId) {
	if (selectingPeer()) {
		offerPeer(peerId);
	} else {
		showPeerHistory(peerId, showAtMsgId);
	}
}

void MainWidget::clearBotStartToken(PeerData *peer) {
	if (peer && peer->isUser() && peer->asUser()->botInfo) {
		peer->asUser()->botInfo->startToken = QString();
	}
}

void MainWidget::contactsReceived() {
	history.contactsReceived();
}

void MainWidget::updateAfterDrag() {
	if (overview) {
		overview->updateAfterDrag();
	} else {
		history.updateAfterDrag();
	}
}

void MainWidget::showPeerHistory(quint64 peerId, qint32 showAtMsgId, bool back) {
	if (!back && (!peerId || (_stack.size() == 1 && _stack[0]->type() == HistoryStackItem && _stack[0]->peer->id == peerId))) {
		back = true;
	}

	App::wnd()->hideLayer();
	if (_hider) {
		_hider->startHide();
		_hider = 0;
	}

	QPixmap animCache, animTopBarCache;
	if (!animating() && ((history.isHidden() && (profile || overview)) || (!cWideMode() && (history.isHidden() || !peerId)))) {
		dialogs.enableShadow(false);
		if (peerId) {
			_topBar.enableShadow(false);
			if (cWideMode()) {
				animCache = myGrab(this, QRect(_dialogsWidth, _playerHeight + st::topBarHeight, width() - _dialogsWidth, height() - _playerHeight - st::topBarHeight));
			} else {
				animCache = myGrab(this, QRect(0, _playerHeight + st::topBarHeight, _dialogsWidth, height() - _playerHeight - st::topBarHeight));
			}
		} else if (cWideMode()) {
			animCache = myGrab(this, QRect(_dialogsWidth, _playerHeight, width() - _dialogsWidth, height() - _playerHeight));
		} else {
			animCache = myGrab(this, QRect(0, _playerHeight, _dialogsWidth, height() - _playerHeight));
		}
		if (peerId || cWideMode()) {
			animTopBarCache = myGrab(this, QRect(_topBar.x(), _topBar.y(), _topBar.width(), st::topBarHeight));
		}
		dialogs.enableShadow();
		_topBar.enableShadow();
		history.show();
	}
	if (history.peer() && history.peer()->id != peerId) clearBotStartToken(history.peer());
	history.showPeerHistory(peerId, showAtMsgId);

	bool noPeer = (!history.peer() || !history.peer()->id), onlyDialogs = noPeer && !cWideMode();
	if (profile || overview) {
		if (profile) {
			profile->hide();
			profile->clear();
			profile->deleteLater();
			profile->rpcInvalidate();
			profile = 0;
		}
		if (overview) {
			overview->hide();
			overview->clear();
			overview->deleteLater();
			overview->rpcInvalidate();
			overview = 0;
		}
		clearBotStartToken(_peerInStack);
		_peerInStack = 0;
		_msgIdInStack = 0;
		_stack.clear();
	}
	if (onlyDialogs) {
		_topBar.hide();
		history.hide();
		if (!animating()) {
			dialogs.show();
			if (!animCache.isNull()) {
				dialogs.animShow(animCache);
			}
		}
	} else {
		if (noPeer) {
			_topBar.hide();
			resizeEvent(0);
		}
		if (!cWideMode() && !dialogs.isHidden()) dialogs.hide();
		if (!animating()) {
			if (history.isHidden()) history.show();
			if (!animCache.isNull()) {
				history.animShow(animCache, animTopBarCache, back);
			} else {
				QTimer::singleShot(0, this, SLOT(setInnerFocus()));
			}
		}
	}

	if (!dialogs.isHidden()) {
		dialogs.scrollToPeer(peerId, showAtMsgId);
		dialogs.update();
	}
	App::wnd()->getTitle()->updateBackButton();
}

void MainWidget::peerBefore(const PeerData *inPeer, MsgId inMsg, PeerData *&outPeer, MsgId &outMsg) {
	if (selectingPeer()) {
		outPeer = 0;
		outMsg = 0;
		return;
	}
	dialogs.peerBefore(inPeer, inMsg, outPeer, outMsg);
}

void MainWidget::peerAfter(const PeerData *inPeer, MsgId inMsg, PeerData *&outPeer, MsgId &outMsg) {
	if (selectingPeer()) {
		outPeer = 0;
		outMsg = 0;
		return;
	}
	dialogs.peerAfter(inPeer, inMsg, outPeer, outMsg);
}

PeerData *MainWidget::historyPeer() {
	return history.peer();
}

PeerData *MainWidget::peer() {
	return overview ? overview->peer() : history.peer();
}

PeerData *MainWidget::activePeer() {
	return history.peer() ? history.peer() : _peerInStack;
}

MsgId MainWidget::activeMsgId() {
	return history.peer() ? history.msgId() : _msgIdInStack;
}

PeerData *MainWidget::profilePeer() {
	return profile ? profile->peer() : 0;
}

PeerData *MainWidget::overviewPeer() {
	return overview ? overview->peer() : 0;
}

bool MainWidget::mediaTypeSwitch() {
	if (!overview || (overview->type() == OverviewAudioDocuments)) return false;

	for (int32 i = 0; i < OverviewCount; ++i) {
		if (!(_mediaTypeMask & ~(1 << i))) {
			return false;
		}
	}
	return true;
}

void MainWidget::showMediaOverview(PeerData *peer, MediaOverviewType type, bool back, int32 lastScrollTop) {
	App::wnd()->hideSettings();
	if (overview && overview->peer() == peer) {
		if (overview->type() != type) {
			overview->switchType(type);
		} else if (type == OverviewAudioDocuments) { // hack for player
			showBackFromStack();
		}
		return;
	}

	dialogs.enableShadow(false);
	_topBar.enableShadow(false);
	QRect topBarRect = QRect(_topBar.x(), _topBar.y(), _topBar.width(), st::topBarHeight);
	QRect historyRect = QRect(history.x(), topBarRect.y() + topBarRect.height(), history.width(), history.y() + history.height() - topBarRect.y() - topBarRect.height());
	QPixmap animCache, animTopBarCache;
	if (!animating() && (!cWideMode() || profile || overview || history.peer())) {
		animCache = myGrab(this, historyRect);
		animTopBarCache = myGrab(this, topBarRect);
	}
	dialogs.enableShadow();
	_topBar.enableShadow();
	if (!back) {
		if (overview) {
			_stack.push_back(new StackItemOverview(overview->peer(), overview->type(), overview->lastWidth(), overview->lastScrollTop()));
		} else if (profile) {
			_stack.push_back(new StackItemProfile(profile->peer(), profile->lastScrollTop()));
		} else if (history.peer()) {
			_peerInStack = history.peer();
			_msgIdInStack = history.msgId();
			_stack.push_back(new StackItemHistory(_peerInStack, _msgIdInStack, history.replyReturns(), history.kbWasHidden()));
		}
	}
	if (overview) {
		overview->hide();
		overview->clear();
		overview->deleteLater();
		overview->rpcInvalidate();
	}
	if (profile) {
		profile->hide();
		profile->clear();
		profile->deleteLater();
		profile->rpcInvalidate();
		profile = 0;
	}
	overview = new OverviewWidget(this, peer, type);
	_mediaTypeMask = 0;
	_topBar.show();
	resizeEvent(0);
	mediaOverviewUpdated(peer, type);
	if (!animCache.isNull()) {
		overview->animShow(animCache, animTopBarCache, back, lastScrollTop);
	} else {
		overview->fastShow();
	}
	history.animStop();
	if (back) clearBotStartToken(history.peer());
	history.showPeerHistory(0, 0);
	history.hide();
	if (!cWideMode()) dialogs.hide();

	orderWidgets();

	App::wnd()->getTitle()->updateBackButton();
}

void MainWidget::showPeerProfile(PeerData *peer, bool back, int32 lastScrollTop) {
	App::wnd()->hideSettings();
	if (profile && profile->peer() == peer) return;

	dialogs.enableShadow(false);
	_topBar.enableShadow(false);
	QPixmap animCache = myGrab(this, history.geometry()), animTopBarCache = myGrab(this, QRect(_topBar.x(), _topBar.y(), _topBar.width(), st::topBarHeight));
	dialogs.enableShadow();
	_topBar.enableShadow();
	if (!back) {
		if (overview) {
			_stack.push_back(new StackItemOverview(overview->peer(), overview->type(), overview->lastWidth(), overview->lastScrollTop()));
		} else if (profile) {
			_stack.push_back(new StackItemProfile(profile->peer(), profile->lastScrollTop()));
		} else {
			_peerInStack = history.peer();
			_msgIdInStack = history.msgId();
			_stack.push_back(new StackItemHistory(_peerInStack, _msgIdInStack, history.replyReturns(), history.kbWasHidden()));
		}
	}
	if (overview) {
		overview->hide();
		overview->clear();
		overview->deleteLater();
		overview->rpcInvalidate();
		overview = 0;
	}
	if (profile) {
		profile->hide();
		profile->clear();
		profile->deleteLater();
		profile->rpcInvalidate();
	}
	profile = new ProfileWidget(this, peer);
	_topBar.show();
	resizeEvent(0);
	profile->animShow(animCache, animTopBarCache, back, lastScrollTop);
	history.animStop();
	if (back) clearBotStartToken(history.peer());
	history.showPeerHistory(0, 0);
	history.hide();

	orderWidgets();

	App::wnd()->getTitle()->updateBackButton();
}

void MainWidget::showBackFromStack() {
	if (selectingPeer()) return;
	if (_stack.isEmpty()) {
		showDialogs();
		QTimer::singleShot(0, this, SLOT(setInnerFocus()));
		return;
	}
	StackItem *item = _stack.back();
	_stack.pop_back();
	if (item->type() == HistoryStackItem) {
		_peerInStack = 0;
		_msgIdInStack = 0;
		for (int32 i = _stack.size(); i > 0;) {
			if (_stack.at(--i)->type() == HistoryStackItem) {
				_peerInStack = static_cast<StackItemHistory*>(_stack.at(i))->peer;
				_msgIdInStack = static_cast<StackItemHistory*>(_stack.at(i))->msgId;
				break;
			}
		}
		StackItemHistory *histItem = static_cast<StackItemHistory*>(item);
		showPeerHistory(histItem->peer->id, App::main()->activeMsgId(), true);
		history.setReplyReturns(histItem->peer->id, histItem->replyReturns);
		if (histItem->kbWasHidden) history.setKbWasHidden();
	} else if (item->type() == ProfileStackItem) {
		StackItemProfile *profItem = static_cast<StackItemProfile*>(item);
		showPeerProfile(profItem->peer, true, profItem->lastScrollTop);
	} else if (item->type() == OverviewStackItem) {
		StackItemOverview *overItem = static_cast<StackItemOverview*>(item);
		showMediaOverview(overItem->peer, overItem->mediaType, true, overItem->lastScrollTop);
	}
	delete item;
}

void MainWidget::orderWidgets() {
	_topBar.raise();
	_player.raise();
	dialogs.raise();
	_mediaType.raise();
	if (_hider) _hider->raise();
}

QRect MainWidget::historyRect() const {
	QRect r(history.historyRect());
	r.moveLeft(r.left() + history.x());
	r.moveTop(r.top() + history.y());
	return r;
}

void MainWidget::dlgUpdated(DialogRow *row) {
	dialogs.dlgUpdated(row);
}

void MainWidget::dlgUpdated(History *row) {
	dialogs.dlgUpdated(row);
}

void MainWidget::windowShown() {
	history.windowShown();
}

void MainWidget::sentUpdatesReceived(uint64 randomId, const MTPUpdates &result) {
	handleUpdates(result, randomId);
	App::emitPeerUpdated();
}

void MainWidget::msgUpdated(PeerId peer, const HistoryItem *msg) {
	if (!msg) return;
	history.msgUpdated(peer, msg);
	if (!msg->history()->dialogs.isEmpty()) dialogs.dlgUpdated(msg->history()->dialogs[0]);
	if (overview) overview->msgUpdated(peer, msg);
}

void MainWidget::historyToDown(History *hist) {
	history.historyToDown(hist);
}

void MainWidget::dialogsToUp() {
	dialogs.dialogsToUp();
}

void MainWidget::newUnreadMsg(History *hist, HistoryItem *item) {
	history.newUnreadMsg(hist, item);
}

void MainWidget::historyWasRead() {
	history.historyWasRead(false);
}

void MainWidget::historyCleared(History *hist) {
	history.historyCleared(hist);
}

void MainWidget::animShow(const QPixmap &bgAnimCache, bool back) {
	_bgAnimCache = bgAnimCache;

	anim::stop(this);
	showAll();
	_animCache = myGrab(this, rect());

	a_coord = back ? anim::ivalue(-st::introSlideShift, 0) : anim::ivalue(st::introSlideShift, 0);
	a_alpha = anim::fvalue(0, 1);
	a_bgCoord = back ? anim::ivalue(0, st::introSlideShift) : anim::ivalue(0, -st::introSlideShift);
	a_bgAlpha = anim::fvalue(1, 0);

	hideAll();
	anim::start(this);
	show();
}

bool MainWidget::animStep(float64 ms) {
	float64 fullDuration = st::introSlideDelta + st::introSlideDuration, dt = ms / fullDuration;
	float64 dt1 = (ms > st::introSlideDuration) ? 1 : (ms / st::introSlideDuration), dt2 = (ms > st::introSlideDelta) ? (ms - st::introSlideDelta) / (st::introSlideDuration) : 0;
	bool res = true;
	if (dt2 >= 1) {
		res = false;
		a_bgCoord.finish();
		a_bgAlpha.finish();
		a_coord.finish();
		a_alpha.finish();

		_animCache = _bgAnimCache = QPixmap();

		anim::stop(this);
		showAll();
		activate();
	} else {
		a_bgCoord.update(dt1, st::introHideFunc);
		a_bgAlpha.update(dt1, st::introAlphaHideFunc);
		a_coord.update(dt2, st::introShowFunc);
		a_alpha.update(dt2, st::introAlphaShowFunc);
	}
	update();
	return res;
}

void MainWidget::paintEvent(QPaintEvent *e) {
	if (_background) checkChatBackground();

	QPainter p(this);
	if (animating()) {
		p.setOpacity(a_bgAlpha.current());
		p.drawPixmap(a_bgCoord.current(), 0, _bgAnimCache);
		p.setOpacity(a_alpha.current());
		p.drawPixmap(a_coord.current(), 0, _animCache);
	} else {
	}
}

void MainWidget::hideAll() {
	dialogs.hide();
	history.hide();
	if (profile) {
		profile->hide();
	}
	if (overview) {
		overview->hide();
	}
	_topBar.hide();
	_mediaType.hide();
}

void MainWidget::showAll() {
	if (cPasswordRecovered()) {
		cSetPasswordRecovered(false);
		App::wnd()->showLayer(new ConfirmBox(lang(lng_signin_password_removed), true, lang(lng_continue)));
	}
	if (cWideMode()) {
		if (_hider) {
			_hider->show();
			if (_forwardConfirm) {
				App::wnd()->hideLayer(true);
				_forwardConfirm = 0;
			}
		}
		dialogs.show();
		if (overview) {
			overview->show();
		} else if (profile) {
			profile->show();
		} else {
			history.show();
			history.resizeEvent(0);
		}
		if (profile || overview || history.peer()) {
			_topBar.show();
		}
	} else {
		if (_hider) {
			_hider->hide();
			if (!_forwardConfirm && _hider->wasOffered()) {
				_forwardConfirm = new ConfirmBox(_hider->offeredText(), lang(lng_forward));
				connect(_forwardConfirm, SIGNAL(confirmed()), _hider, SLOT(forward()));
				connect(_forwardConfirm, SIGNAL(cancelled()), this, SLOT(onForwardCancel()));
				App::wnd()->showLayer(_forwardConfirm, true);
			}
		}
		if (selectingPeer()) {
			dialogs.show();
			history.hide();
			if (overview) overview->hide();
			if (profile) profile->hide();
			_topBar.hide();
		} else if (overview) {
			overview->show();
		} else if (profile) {
			profile->show();
		} else if (history.peer()) {
			history.show();
			history.resizeEvent(0);
		} else {
			dialogs.show();
			history.hide();
		}
		if (!selectingPeer() && (profile || overview || history.peer())) {
			_topBar.show();
			dialogs.hide();
		}
	}
	App::wnd()->checkHistoryActivation();
}

void MainWidget::resizeEvent(QResizeEvent *e) {
	int32 tbh = _topBar.isHidden() ? 0 : st::topBarHeight;
	if (cWideMode()) {
		_dialogsWidth = snap<int>((width() * 5) / 14, st::dlgMinWidth, st::dlgMaxWidth);
		dialogs.setGeometry(0, 0, _dialogsWidth + st::dlgShadow, height());
		_player.setGeometry(_dialogsWidth, 0, width() - _dialogsWidth, _player.height());
		_topBar.setGeometry(_dialogsWidth, _playerHeight, width() - _dialogsWidth, st::topBarHeight + st::titleShadow);
		history.setGeometry(_dialogsWidth, _playerHeight + tbh, width() - _dialogsWidth, height() - _playerHeight - tbh);
		if (_hider) _hider->setGeometry(QRect(_dialogsWidth, 0, width() - _dialogsWidth, height()));
	} else {
		_dialogsWidth = width();
		_player.setGeometry(0, 0, _dialogsWidth, _player.height());
		dialogs.setGeometry(0, _playerHeight, _dialogsWidth + st::dlgShadow, height() - _playerHeight);
		_topBar.setGeometry(0, _playerHeight, _dialogsWidth, st::topBarHeight + st::titleShadow);
		history.setGeometry(0, _playerHeight + tbh, _dialogsWidth, height() - _playerHeight - tbh);
		if (_hider) _hider->setGeometry(QRect(0, 0, _dialogsWidth, height()));
	}
	_mediaType.move(width() - _mediaType.width(), _playerHeight + st::topBarHeight);
	if (profile) profile->setGeometry(history.geometry());
	if (overview) overview->setGeometry(history.geometry());
	_contentScrollAddToY = 0;
}

int32 MainWidget::contentScrollAddToY() const {
	return _contentScrollAddToY;
}

void MainWidget::keyPressEvent(QKeyEvent *e) {
}

void MainWidget::updateWideMode() {
	showAll();
	_topBar.showAll();
}

bool MainWidget::needBackButton() {
	return overview || profile || (history.peer() && history.peer()->id);
}

void MainWidget::showDialogs() {
	showPeerHistory(0, 0);
}

void MainWidget::paintTopBar(QPainter &p, float64 over, int32 decreaseWidth) {
	if (profile) {
		profile->paintTopBar(p, over, decreaseWidth);
	} else if (overview) {
		overview->paintTopBar(p, over, decreaseWidth);
	} else {
		history.paintTopBar(p, over, decreaseWidth);
	}
}

void MainWidget::topBarShadowParams(int32 &x, float64 &o) {
	if (!cWideMode() && dialogs.isHidden()) {
		if (profile) {
			if (!_peerInStack) profile->topBarShadowParams(x, o);
		} else if (overview) {
			if (!_peerInStack) overview->topBarShadowParams(x, o);
		} else {
			history.topBarShadowParams(x, o);
		}
	}
}

void MainWidget::onPhotosSelect() {
	if (overview) overview->switchType(OverviewPhotos);
	_mediaType.hideStart();
}

void MainWidget::onVideosSelect() {
	if (overview) overview->switchType(OverviewVideos);
	_mediaType.hideStart();
}

void MainWidget::onDocumentsSelect() {
	if (overview) overview->switchType(OverviewDocuments);
	_mediaType.hideStart();
}

void MainWidget::onAudiosSelect() {
	if (overview) overview->switchType(OverviewAudios);
	_mediaType.hideStart();
}

void MainWidget::onLinksSelect() {
	if (overview) overview->switchType(OverviewLinks);
	_mediaType.hideStart();
}

TopBarWidget *MainWidget::topBar() {
	return &_topBar;
}

PlayerWidget *MainWidget::player() {
	return &_player;
}

void MainWidget::onTopBarClick() {
	if (profile) {
		profile->topBarClick();
	} else if (overview) {
		overview->topBarClick();
	} else {
		history.topBarClick();
	}
}

void MainWidget::onPeerShown(PeerData *peer) {
	if ((cWideMode() || !selectingPeer()) && (profile || overview || (peer && peer->id))) {
		_topBar.show();
	} else {
		_topBar.hide();
	}
	resizeEvent(0);
	if (animating()) {
		_topBar.hide();
	}
}

void MainWidget::searchInPeer(PeerData *peer) {
	dialogs.searchInPeer(peer);
	if (cWideMode()) {
		dialogs.activate();
	} else {
		dialogsToUp();
		showDialogs();
	}
}

void MainWidget::onUpdateNotifySettings() {
	if (this != App::main()) return;
	while (!updateNotifySettingPeers.isEmpty()) {
		PeerData *peer = *updateNotifySettingPeers.begin();
		updateNotifySettingPeers.erase(updateNotifySettingPeers.begin());

		if (peer->notify == UnknownNotifySettings || peer->notify == EmptyNotifySettings) {
			peer->notify = new NotifySettings();
		}
		MTP::send(MTPaccount_UpdateNotifySettings(MTP_inputNotifyPeer(peer->input), MTP_inputPeerNotifySettings(MTP_int(peer->notify->mute), MTP_string(peer->notify->sound), MTP_bool(peer->notify->previews), MTP_int(peer->notify->events))), RPCResponseHandler(), 0, updateNotifySettingPeers.isEmpty() ? 0 : 10);
	}
}

void MainWidget::feedUpdates(const MTPVector<MTPUpdate> &updates, bool skipMessageIds) {
	const QVector<MTPUpdate> &v(updates.c_vector().v);
	for (QVector<MTPUpdate>::const_iterator i = v.cbegin(), e = v.cend(); i != e; ++i) {
		if (skipMessageIds && i->type() == mtpc_updateMessageID) continue;
		feedUpdate(*i);
	}
}

void MainWidget::feedMessageIds(const MTPVector<MTPUpdate> &updates) {
	const QVector<MTPUpdate> &v(updates.c_vector().v);
	for (QVector<MTPUpdate>::const_iterator i = v.cbegin(), e = v.cend(); i != e; ++i) {
		if (i->type() == mtpc_updateMessageID) {
			feedUpdate(*i);
		}
	}
}

bool MainWidget::updateFail(const RPCError &e) {
	if (MTP::authedId()) {
		App::logOut();
	}
	return true;
}

void MainWidget::updSetState(int32 pts, int32 date, int32 qts, int32 seq) {
	if (pts) _ptsWaiter.init(pts);
	if (updDate < date) updDate = date;
	if (qts && updQts < qts) {
		updQts = qts;
	}
	if (seq && seq != updSeq) {
		updSeq = seq;
		if (_bySeqTimer.isActive()) _bySeqTimer.stop();
		for (QMap<int32, MTPUpdates>::iterator i = _bySeqUpdates.begin(); i != _bySeqUpdates.end();) {
			int32 s = i.key();
			if (s <= seq + 1) {
				MTPUpdates v = i.value();
				i = _bySeqUpdates.erase(i);
				if (s == seq + 1) {
					return handleUpdates(v);
				}
			} else {
				if (!_bySeqTimer.isActive()) _bySeqTimer.start(WaitForSkippedTimeout);
				break;
			}
		}
	}
}

void MainWidget::gotChannelDifference(ChannelData *channel, const MTPupdates_ChannelDifference &diff) {
	_channelFailDifferenceTimeout.remove(channel);
	
	int32 timeout = 0, flags = 0;
	switch (diff.type()) {
	case mtpc_updates_channelDifferenceEmpty: {
		const MTPDupdates_channelDifferenceEmpty &d(diff.c_updates_channelDifferenceEmpty());
		if (d.has_timeout()) timeout = d.vtimeout.v;
		flags = d.vflags.v;
		channel->ptsInit(d.vpts.v);
	} break;

	case mtpc_updates_channelDifferenceTooLong: {
		const MTPDupdates_channelDifferenceTooLong &d(diff.c_updates_channelDifferenceTooLong());

		App::feedUsers(d.vusers);
		App::feedChats(d.vchats);
		App::feedMsgs(d.vmessages);
		if (History *h = App::historyLoaded(channel->id)) {
			if (HistoryItem *item = App::histItemById(peerToChannel(channel->id), d.vtop_important_message.v)) {
				h->setLastMessage(item);
			}
			if (d.vunread_important_count.v >= h->unreadCount) {
				h->setUnreadCount(d.vunread_important_count.v, false);
				h->inboxReadBefore = d.vread_inbox_max_id.v + 1;
			}
			h->setNotLoadedAtBottom();
			if (history.peer() == channel) {
				history.updateToEndVisibility();
			}
		}

		if (d.has_timeout()) timeout = d.vtimeout.v;
		flags = d.vflags.v;
		channel->ptsInit(d.vpts.v);
	} break;

	case mtpc_updates_channelDifference: {
		const MTPDupdates_channelDifference &d(diff.c_updates_channelDifference());

		App::feedUsers(d.vusers);
		App::feedChats(d.vchats, false);
		feedMessageIds(d.vother_updates);
		App::feedMsgs(d.vnew_messages, 1);
		feedUpdates(d.vother_updates, true);

		if (d.has_timeout()) timeout = d.vtimeout.v;
		flags = d.vflags.v;
		channel->ptsInit(d.vpts.v);
	} break;
	}

	channel->ptsSetRequesting(false);

	if (!(flags & MTPupdates_ChannelDifference_flag_final)) {
		MTP_LOG(0, ("getChannelDifference { good - after not final channelDifference was received }%1").arg(cTestMode() ? " TESTMODE" : ""));
		getChannelDifference(channel);
	} else if (timeout) {
//		QTimer::singleShot(timeout * 1000, this, SLOT(getDifference()));
	}

	App::emitPeerUpdated();
}

bool MainWidget::failChannelDifference(ChannelData *channel, const RPCError &error) {
	if (mtpIsFlood(error)) return false;

	LOG(("RPC Error in getChannelDifference: %1 %2: %3").arg(error.code()).arg(error.type()).arg(error.description()));
	failDifferenceStartTimerFor(channel);
	return true;
}

void MainWidget::gotState(const MTPupdates_State &state) {
	const MTPDupdates_state &d(state.c_updates_state());
	updSetState(d.vpts.v, d.vdate.v, d.vqts.v, d.vseq.v);

	MTP::setGlobalDoneHandler(rpcDone(&MainWidget::updateReceived));
	_lastUpdateTime = getms(true);
	noUpdatesTimer.start(NoUpdatesTimeout);
	_ptsWaiter.setRequesting(false);

	dialogs.loadDialogs();
	updateOnline();

	App::emitPeerUpdated();
}

void MainWidget::gotDifference(const MTPupdates_Difference &diff) {
	_failDifferenceTimeout = 1;

	switch (diff.type()) {
	case mtpc_updates_differenceEmpty: {
		const MTPDupdates_differenceEmpty &d(diff.c_updates_differenceEmpty());
		updSetState(_ptsWaiter.current(), d.vdate.v, updQts, d.vseq.v);

		MTP::setGlobalDoneHandler(rpcDone(&MainWidget::updateReceived));
		_lastUpdateTime = getms(true);
		noUpdatesTimer.start(NoUpdatesTimeout);

		_ptsWaiter.setRequesting(false);
		
		App::emitPeerUpdated();
	} break;
	case mtpc_updates_differenceSlice: {
		const MTPDupdates_differenceSlice &d(diff.c_updates_differenceSlice());
		feedDifference(d.vusers, d.vchats, d.vnew_messages, d.vother_updates);

		const MTPDupdates_state &s(d.vintermediate_state.c_updates_state());
		updSetState(s.vpts.v, s.vdate.v, s.vqts.v, s.vseq.v);

		_ptsWaiter.setRequesting(false);

		MTP_LOG(0, ("getDifference { good - after a slice of difference was received }%1").arg(cTestMode() ? " TESTMODE" : ""));
		getDifference();

		App::emitPeerUpdated();
	} break;
	case mtpc_updates_difference: {
		const MTPDupdates_difference &d(diff.c_updates_difference());
		feedDifference(d.vusers, d.vchats, d.vnew_messages, d.vother_updates);

		gotState(d.vstate);
	} break;
	};
}

bool MainWidget::getDifferenceTimeChanged(ChannelData *channel, int32 ms, ChannelGetDifferenceTime &channelCurTime, uint64 &curTime) {
	if (channel) {
		if (ms <= 0) {
			ChannelGetDifferenceTime::iterator i = channelCurTime.find(channel);
			if (i != channelCurTime.cend()) {
				channelCurTime.erase(i);
			} else {
				return false;
			}
		} else {
			uint64 when = getms(true) + ms;
			ChannelGetDifferenceTime::iterator i = channelCurTime.find(channel);
			if (i != channelCurTime.cend()) {
				if (i.value() > when) {
					i.value() = when;
				} else {
					return false;
				}
			} else {
				channelCurTime.insert(channel, when);
			}
		}
	} else {
		if (ms <= 0) {
			if (curTime) {
				curTime = 0;
			} else {
				return false;
			}
		} else {
			uint64 when = getms(true) + ms;
			if (!curTime || curTime > when) {
				curTime = when;
			} else {
				return false;
			}
		}
	}
	return true;
}

void MainWidget::ptsWaiterStartTimerFor(ChannelData *channel, int32 ms) {
	if (getDifferenceTimeChanged(channel, ms, _channelGetDifferenceTimeByPts, _getDifferenceTimeByPts)) {
		onGetDifferenceTimeByPts();
	}
}

void MainWidget::failDifferenceStartTimerFor(ChannelData *channel) {
	int32 ms = 0;
	ChannelFailDifferenceTimeout::iterator i;
	if (channel) {
		i = _channelFailDifferenceTimeout.find(channel);
		if (i == _channelFailDifferenceTimeout.cend()) {
			i = _channelFailDifferenceTimeout.insert(channel, 1);
		}
		ms = i.value() * 1000;
	} else {
		ms = _failDifferenceTimeout * 1000;
	}
	if (getDifferenceTimeChanged(channel, ms, _channelGetDifferenceTimeAfterFail, _getDifferenceTimeAfterFail)) {
		onGetDifferenceTimeAfterFail();
	}
	if (channel) {
		if (i.value() < 64) i.value() *= 2;
	} else {
		if (_failDifferenceTimeout < 64) _failDifferenceTimeout *= 2;
	}
}

bool MainWidget::ptsUpdated(int32 pts, int32 ptsCount) { // return false if need to save that update and apply later
	return _ptsWaiter.updated(0, pts, ptsCount);
}

bool MainWidget::ptsUpdated(int32 pts, int32 ptsCount, const MTPUpdates &updates) {
	return _ptsWaiter.updated(0, pts, ptsCount, updates);
}

bool MainWidget::ptsUpdated(int32 pts, int32 ptsCount, const MTPUpdate &update) {
	return _ptsWaiter.updated(0, pts, ptsCount, update);
}

void MainWidget::feedDifference(const MTPVector<MTPUser> &users, const MTPVector<MTPChat> &chats, const MTPVector<MTPMessage> &msgs, const MTPVector<MTPUpdate> &other) {
	App::wnd()->checkAutoLock();
	App::feedUsers(users, false);
	App::feedChats(chats, false);
	feedMessageIds(other);
	App::feedMsgs(msgs, 1);
	feedUpdates(other, true);
	history.peerMessagesUpdated();
}

bool MainWidget::failDifference(const RPCError &error) {
	if (mtpIsFlood(error)) return false;

	LOG(("RPC Error in getDifference: %1 %2: %3").arg(error.code()).arg(error.type()).arg(error.description()));
	failDifferenceStartTimerFor(0);
	return true;
}

void MainWidget::onGetDifferenceTimeByPts() {
	if (!MTP::authedId()) return;

	uint64 now = getms(true), wait = 0;
	if (_getDifferenceTimeByPts) {
		if (_getDifferenceTimeByPts > now) {
			wait = _getDifferenceTimeByPts - now;
		} else {
			getDifference();
		}
	}
	for (ChannelGetDifferenceTime::iterator i = _channelGetDifferenceTimeByPts.begin(); i != _channelGetDifferenceTimeByPts.cend();) {
		if (i.value() > now) {
			wait = wait ? qMin(wait, i.value() - now) : (i.value() - now);
			++i;
		} else {
			getChannelDifference(i.key(), GetChannelDifferenceFromPtsGap);
			i = _channelGetDifferenceTimeByPts.erase(i);
		}
	}
	if (wait) {
		_byPtsTimer.start(wait);
	} else {
		_byPtsTimer.stop();
	}
}

void MainWidget::onGetDifferenceTimeAfterFail() {
	if (!MTP::authedId()) return;

	uint64 now = getms(true), wait = 0;
	if (_getDifferenceTimeAfterFail) {
		if (_getDifferenceTimeAfterFail > now) {
			wait = _getDifferenceTimeAfterFail - now;
		} else {
			_ptsWaiter.setRequesting(false);
			MTP_LOG(0, ("getDifference { force - after get difference failed }%1").arg(cTestMode() ? " TESTMODE" : ""));
			getDifference();
		}
	}
	for (ChannelGetDifferenceTime::iterator i = _channelGetDifferenceTimeAfterFail.begin(); i != _channelGetDifferenceTimeAfterFail.cend();) {
		if (i.value() > now) {
			wait = wait ? qMin(wait, i.value() - now) : (i.value() - now);
			++i;
		} else {
			getChannelDifference(i.key(), GetChannelDifferenceFromFail);
			i = _channelGetDifferenceTimeAfterFail.erase(i);
		}
	}
	if (wait) {
		_failDifferenceTimer.start(wait);
	} else {
		_failDifferenceTimer.stop();
	}
}

void MainWidget::getDifference() {
	if (this != App::main()) return;

	_getDifferenceTimeByPts = 0;

	LOG(("Getting difference! no updates timer: %1, remains: %2").arg(noUpdatesTimer.isActive() ? 1 : 0).arg(noUpdatesTimer.remainingTime()));
	if (_ptsWaiter.requesting()) return;

	_bySeqUpdates.clear();
	_bySeqTimer.stop();

	_ptsWaiter.clearSkippedUpdates();

	noUpdatesTimer.stop();
	_getDifferenceTimeAfterFail = 0;

	LOG(("Getting difference for %1, %2").arg(_ptsWaiter.current()).arg(updDate));
	_ptsWaiter.setRequesting(true);
	MTP::setGlobalDoneHandler(RPCDoneHandlerPtr(0));
	MTP::send(MTPupdates_GetDifference(MTP_int(_ptsWaiter.current()), MTP_int(updDate), MTP_int(updQts)), rpcDone(&MainWidget::gotDifference), rpcFail(&MainWidget::failDifference));
}

void MainWidget::getChannelDifference(ChannelData *channel, GetChannelDifferenceFrom from) {
	if (this != App::main() || !channel) return;

	if (from != GetChannelDifferenceFromPtsGap) {
		_channelGetDifferenceTimeByPts.remove(channel);
	}

	LOG(("Getting channel difference!"));
	if (!channel->ptsInited() || channel->ptsRequesting()) return;

	channel->ptsClearSkippedUpdates();

	if (from != GetChannelDifferenceFromFail) {
		_channelGetDifferenceTimeAfterFail.remove(channel);
	}

	LOG(("Getting channel difference for %1").arg(channel->pts()));
	channel->ptsSetRequesting(true);
	MTP::send(MTPupdates_GetChannelDifference(channel->input, MTP_channelMessagesFilterCollapsed(), MTP_int(channel->pts()), MTP_int(MTPChannelGetDifferenceLimit)), rpcDone(&MainWidget::gotChannelDifference, channel), rpcFail(&MainWidget::failChannelDifference, channel));
}

void MainWidget::mtpPing() {
	MTP::ping();
}

void MainWidget::start(const MTPUser &user) {
	int32 uid = user.c_user().vid.v;
	if (MTP::authedId() != uid) {
		MTP::authed(uid);
		Local::writeMtpData();
	}

	Local::readSavedPeers();

	cSetOtherOnline(0);
	App::feedUsers(MTP_vector<MTPUser>(1, user));
	#ifndef TDESKTOP_DISABLE_AUTOUPDATE
	App::app()->startUpdateCheck();
	#endif
	MTP::send(MTPupdates_GetState(), rpcDone(&MainWidget::gotState));
	update();
	if (!cStartUrl().isEmpty()) {
		openLocalUrl(cStartUrl());
		cSetStartUrl(QString());
	}
	_started = true;
	App::wnd()->sendServiceHistoryRequest();
	Local::readStickers();
	history.start();
}

bool MainWidget::started() {
	return _started;
}

void MainWidget::openLocalUrl(const QString &url) {
	QString u(url.trimmed());
	if (u.startsWith(qstr("tg://resolve"), Qt::CaseInsensitive)) {
		QRegularExpressionMatch m = QRegularExpression(qsl("^tg://resolve/?\\?domain=([a-zA-Z0-9\\.\\_]+)(&(start|startgroup)=([a-zA-Z0-9\\.\\_\\-]+))?(&|$)"), QRegularExpression::CaseInsensitiveOption).match(u);
		if (m.hasMatch()) {
			QString start = m.captured(3), startToken = m.captured(4);
			openPeerByName(m.captured(1), (start == qsl("startgroup")), startToken);
		}
	} else if (u.startsWith(qstr("tg://join"), Qt::CaseInsensitive)) {
		QRegularExpressionMatch m = QRegularExpression(qsl("^tg://join/?\\?invite=([a-zA-Z0-9\\.\\_\\-]+)(&|$)"), QRegularExpression::CaseInsensitiveOption).match(u);
		if (m.hasMatch()) {
			joinGroupByHash(m.captured(1));
		}
	} else if (u.startsWith(qstr("tg://addstickers"), Qt::CaseInsensitive)) {
		QRegularExpressionMatch m = QRegularExpression(qsl("^tg://addstickers/?\\?set=([a-zA-Z0-9\\.\\_]+)(&|$)"), QRegularExpression::CaseInsensitiveOption).match(u);
		if (m.hasMatch()) {
			stickersBox(MTP_inputStickerSetShortName(MTP_string(m.captured(1))));
		}
	}
}

void MainWidget::openPeerByName(const QString &username, bool toProfile, const QString &startToken) {
	App::wnd()->hideMediaview();

	PeerData *peer = App::peerByName(username);
	if (peer) {
		if (toProfile) {
			if (peer->isUser() && peer->asUser()->botInfo && !peer->asUser()->botInfo->cantJoinGroups && !startToken.isEmpty()) {
				peer->asUser()->botInfo->startGroupToken = startToken;
				App::wnd()->showLayer(new ContactsBox(peer->asUser()));
			} else {
				showPeerProfile(peer);
			}
		} else {
			if (peer->isUser() && peer->asUser()->botInfo) {
				peer->asUser()->botInfo->startToken = startToken;
				if (peer == history.peer()) {
					history.updateControlsVisibility();
					history.resizeEvent(0);
				}
			}
			emit showPeerAsync(peer->id, 0);
		}
	} else {
		MTP::send(MTPcontacts_ResolveUsername(MTP_string(username)), rpcDone(&MainWidget::usernameResolveDone, qMakePair(toProfile, startToken)), rpcFail(&MainWidget::usernameResolveFail, username));
	}
}

void MainWidget::joinGroupByHash(const QString &hash) {
	App::wnd()->hideMediaview();
	MTP::send(MTPmessages_CheckChatInvite(MTP_string(hash)), rpcDone(&MainWidget::inviteCheckDone, hash), rpcFail(&MainWidget::inviteCheckFail));
}

void MainWidget::stickersBox(const MTPInputStickerSet &set) {
	App::wnd()->hideMediaview();
	StickerSetBox *box = new StickerSetBox(set);
	connect(box, SIGNAL(installed(uint64)), this, SLOT(onStickersInstalled(uint64)));
	App::wnd()->showLayer(box);
}

void MainWidget::onStickersInstalled(uint64 setId) {
	emit stickersUpdated();
	history.stickersInstalled(setId);
}

void MainWidget::usernameResolveDone(QPair<bool, QString> toProfileStartToken, const MTPcontacts_ResolvedPeer &result) {
	App::wnd()->hideLayer();
	if (result.type() != mtpc_contacts_resolvedPeer) return;

	const MTPDcontacts_resolvedPeer &d(result.c_contacts_resolvedPeer());
	App::feedUsers(d.vusers);
	App::feedChats(d.vchats);
	PeerId peerId = peerFromMTP(d.vpeer);
	if (!peerId) return;

	PeerData *peer = App::peer(peerId);
	if (toProfileStartToken.first) {
		if (peer->isUser() && peer->asUser()->botInfo && !peer->asUser()->botInfo->cantJoinGroups && !toProfileStartToken.second.isEmpty()) {
			peer->asUser()->botInfo->startGroupToken = toProfileStartToken.second;
			App::wnd()->showLayer(new ContactsBox(peer->asUser()));
		} else {
			showPeerProfile(peer);
		}
	} else {
		if (peer->isUser() && peer->asUser()->botInfo) {
			peer->asUser()->botInfo->startToken = toProfileStartToken.second;
			if (peer == history.peer()) {
				history.updateControlsVisibility();
				history.resizeEvent(0);
			}
		}
		showPeerHistory(peer->id, ShowAtUnreadMsgId);
	}
}

bool MainWidget::usernameResolveFail(QString name, const RPCError &error) {
	if (error.type().startsWith(qsl("FLOOD_WAIT_"))) return false;

	if (error.code() == 400) {
		App::wnd()->showLayer(new ConfirmBox(lng_username_not_found(lt_user, name), true));
	}
	return true;
}

void MainWidget::inviteCheckDone(QString hash, const MTPChatInvite &invite) {
	switch (invite.type()) {
	case mtpc_chatInvite: {
		const MTPDchatInvite &d(invite.c_chatInvite());
		ConfirmBox *box = new ConfirmBox(lng_group_invite_want_join(lt_title, qs(d.vtitle)), lang(lng_group_invite_join));
		_inviteHash = hash;
		connect(box, SIGNAL(confirmed()), this, SLOT(onInviteImport()));
		App::wnd()->showLayer(box);
	} break;

	case mtpc_chatInviteAlready: {
		const MTPDchatInviteAlready &d(invite.c_chatInviteAlready());
		PeerData *chat = App::feedChats(MTP_vector<MTPChat>(1, d.vchat));
		if (chat) {
			if ((chat->isChat() && chat->asChat()->left) || (chat->isChannel() && chat->asChannel()->left)) {
				ConfirmBox *box = new ConfirmBox(lng_group_invite_want_join(lt_title, chat->name), lang(lng_group_invite_join));
				_inviteHash = '/' + QString::number(chat->id);
				connect(box, SIGNAL(confirmed()), this, SLOT(onInviteImport()));
				App::wnd()->showLayer(box);
			} else {
				showPeerHistory(chat->id, ShowAtUnreadMsgId);
			}
		}
	} break;
	}
}

bool MainWidget::inviteCheckFail(const RPCError &error) {
	if (error.type().startsWith(qsl("FLOOD_WAIT_"))) return false;

	if (error.code() == 400) {
		App::wnd()->showLayer(new ConfirmBox(lang(lng_group_invite_bad_link), true));
	}
	return true;
}

void MainWidget::onInviteImport() {
	if (_inviteHash.isEmpty()) return;
	if (_inviteHash.at(0) == '/') {
		PeerId id = _inviteHash.midRef(1).toULongLong();
		MTP::send(MTPmessages_AddChatUser(MTP_inputChat(MTP_int(peerToChat(id))), App::self()->inputUser, MTP_int(ForwardOnAdd)), rpcDone(&MainWidget::inviteImportDone), rpcFail(&MainWidget::inviteImportFail), 0, 5);
	} else {
		MTP::send(MTPmessages_ImportChatInvite(MTP_string(_inviteHash)), rpcDone(&MainWidget::inviteImportDone), rpcFail(&MainWidget::inviteImportFail));
	}
}

void MainWidget::inviteImportDone(const MTPUpdates &updates) {
	App::main()->sentUpdatesReceived(updates);

	App::wnd()->hideLayer();
	const QVector<MTPChat> *v = 0;
	switch (updates.type()) {
	case mtpc_updates: v = &updates.c_updates().vchats.c_vector().v; break;
	case mtpc_updatesCombined: v = &updates.c_updatesCombined().vchats.c_vector().v; break;
	case mtpc_updateShort: {
	} break;
	case mtpc_updateShortMessage: {
	} break;
	case mtpc_updateShortChatMessage: {
	} break;
	case mtpc_updateShortSentMessage: {
	} break;
	case mtpc_updatesTooLong: {
	} break;
	}
	if (v && !v->isEmpty() && v->front().type() == mtpc_chat) {
		App::main()->showPeerHistory(peerFromChat(v->front().c_chat().vid.v), ShowAtTheEndMsgId);
	}
}

bool MainWidget::inviteImportFail(const RPCError &error) {
	if (error.type().startsWith(qsl("FLOOD_WAIT_"))) return false;

	if (error.code() == 400) {
		App::wnd()->showLayer(new ConfirmBox(lang(error.type() == qsl("USERS_TOO_MUCH") ? lng_group_invite_no_room : lng_group_invite_bad_link), true), App::wnd()->layerShown());
	}
	return true;
}

void MainWidget::startFull(const MTPVector<MTPUser> &users) {
	const QVector<MTPUser> &v(users.c_vector().v);
	if (v.isEmpty() || v[0].type() != mtpc_user || !(v[0].c_user().vflags.v & MTPDuser_flag_self)) { // wtf?..
		return App::logOut();
	}
	start(v[0]);
}

void MainWidget::applyNotifySetting(const MTPNotifyPeer &peer, const MTPPeerNotifySettings &settings, History *history) {
	switch (settings.type()) {
	case mtpc_peerNotifySettingsEmpty:
		switch (peer.type()) {
		case mtpc_notifyAll: globalNotifyAllPtr = EmptyNotifySettings; break;
		case mtpc_notifyUsers: globalNotifyUsersPtr = EmptyNotifySettings; break;
		case mtpc_notifyChats: globalNotifyChatsPtr = EmptyNotifySettings; break;
		case mtpc_notifyPeer: {
			PeerData *data = App::peerLoaded(peerFromMTP(peer.c_notifyPeer().vpeer));
			if (data && data->notify != EmptyNotifySettings) {
				if (data->notify != UnknownNotifySettings) {
					delete data->notify;
				}
				data->notify = EmptyNotifySettings;
				App::unregMuted(data);
				History *h = App::history(data->id);
				h->setMute(false);
			}
		} break;
		}
	break;
	case mtpc_peerNotifySettings: {
		const MTPDpeerNotifySettings &d(settings.c_peerNotifySettings());
		NotifySettingsPtr setTo = UnknownNotifySettings;
		PeerData *data = 0;
		switch (peer.type()) {
		case mtpc_notifyAll: setTo = globalNotifyAllPtr = &globalNotifyAll; break;
		case mtpc_notifyUsers: setTo = globalNotifyUsersPtr = &globalNotifyUsers; break;
		case mtpc_notifyChats: setTo = globalNotifyChatsPtr = &globalNotifyChats; break;
		case mtpc_notifyPeer: {
			data = App::peerLoaded(peerFromMTP(peer.c_notifyPeer().vpeer));
			if (!data) break;

			if (data->notify == UnknownNotifySettings || data->notify == EmptyNotifySettings) {
				data->notify = new NotifySettings();
			}
			setTo = data->notify;
		} break;
		}
		if (setTo == UnknownNotifySettings) break;

		setTo->mute = d.vmute_until.v;
		setTo->sound = d.vsound.c_string().v;
		setTo->previews = d.vshow_previews.v;
		setTo->events = d.vevents_mask.v;
		if (data) {
			if (!history) history = App::history(data->id);
			int32 changeIn = 0;
			if (isNotifyMuted(setTo, &changeIn)) {
				App::wnd()->notifyClear(history);
				history->setMute(true);
				App::regMuted(data, changeIn);
			} else {
				history->setMute(false);
			}
		}
	} break;
	}

	if (profile) {
		profile->updateNotifySettings();
	}
}

void MainWidget::gotNotifySetting(MTPInputNotifyPeer peer, const MTPPeerNotifySettings &settings) {
	switch (peer.type()) {
	case mtpc_inputNotifyAll: applyNotifySetting(MTP_notifyAll(), settings); break;
	case mtpc_inputNotifyUsers: applyNotifySetting(MTP_notifyUsers(), settings); break;
	case mtpc_inputNotifyChats: applyNotifySetting(MTP_notifyChats(), settings); break;
	case mtpc_inputNotifyPeer:
		switch (peer.c_inputNotifyPeer().vpeer.type()) {
		case mtpc_inputPeerEmpty: applyNotifySetting(MTP_notifyPeer(MTP_peerUser(MTP_int(0))), settings); break;
		case mtpc_inputPeerSelf: applyNotifySetting(MTP_notifyPeer(MTP_peerUser(MTP_int(MTP::authedId()))), settings); break;
		case mtpc_inputPeerUser: applyNotifySetting(MTP_notifyPeer(MTP_peerUser(peer.c_inputNotifyPeer().vpeer.c_inputPeerUser().vuser_id)), settings); break;
		case mtpc_inputPeerChat: applyNotifySetting(MTP_notifyPeer(MTP_peerChat(peer.c_inputNotifyPeer().vpeer.c_inputPeerChat().vchat_id)), settings); break;
		}
	break;
	}
	App::wnd()->notifySettingGot();
}

bool MainWidget::failNotifySetting(MTPInputNotifyPeer peer, const RPCError &error) {
	if (error.type().startsWith(qsl("FLOOD_WAIT_"))) return false;

	gotNotifySetting(peer, MTP_peerNotifySettingsEmpty());
	return true;
}

void MainWidget::updateNotifySetting(PeerData *peer, bool enabled) {
	updateNotifySettingPeers.insert(peer);
	int32 muteFor = 86400 * 365;
	if (peer->notify == EmptyNotifySettings) {
		if (!enabled) {
			peer->notify = new NotifySettings();
			peer->notify->sound = "";
			peer->notify->mute = unixtime() + muteFor;
		}
	} else {
		if (peer->notify == UnknownNotifySettings) {
			peer->notify = new NotifySettings();
		}
		peer->notify->sound = enabled ? "default" : "";
		peer->notify->mute = enabled ? 0 : (unixtime() + muteFor);
	}
	if (!enabled) {
		App::regMuted(peer, muteFor + 1);
	} else {
		App::unregMuted(peer);
	}
	App::history(peer->id)->setMute(!enabled);
	updateNotifySettingTimer.start(NotifySettingSaveTimeout);
}

void MainWidget::incrementSticker(DocumentData *sticker) {
	if (!sticker || !sticker->sticker()) return;

	RecentStickerPack &recent(cGetRecentStickers());
	RecentStickerPack::iterator i = recent.begin(), e = recent.end();
	for (; i != e; ++i) {
		if (i->first == sticker) {
			++i->second;
			if (i->second > 0x8000) {
				for (RecentStickerPack::iterator j = recent.begin(); j != e; ++j) {
					if (j->second > 1) {
						j->second /= 2;
					} else {
						j->second = 1;
					}
				}
			}
			for (; i != recent.begin(); --i) {
				if ((i - 1)->second > i->second) {
					break;
				}
				qSwap(*i, *(i - 1));
			}
			break;
		}
	}
	if (i == e) {
		while (recent.size() >= StickerPanPerRow * StickerPanRowsPerPage) recent.pop_back();
		recent.push_back(qMakePair(sticker, 1));
		for (i = recent.end() - 1; i != recent.begin(); --i) {
			if ((i - 1)->second > i->second) {
				break;
			}
			qSwap(*i, *(i - 1));
		}
	}

	Local::writeUserSettings();

	bool found = false;
	uint64 setId = 0;
	QString setName;
	switch (sticker->sticker()->set.type()) {
	case mtpc_inputStickerSetID: setId = sticker->sticker()->set.c_inputStickerSetID().vid.v; break;
	case mtpc_inputStickerSetShortName: setName = qs(sticker->sticker()->set.c_inputStickerSetShortName().vshort_name).toLower().trimmed(); break;
	}
	StickerSets &sets(cRefStickerSets());
	for (StickerSets::const_iterator i = sets.cbegin(); i != sets.cend(); ++i) {
		if (i->id == CustomStickerSetId || i->id == DefaultStickerSetId || (setId && i->id == setId) || (!setName.isEmpty() && i->shortName.toLower().trimmed() == setName)) {
			for (int32 j = 0, l = i->stickers.size(); j < l; ++j) {
				if (i->stickers.at(j) == sticker) {
					found = true;
					break;
				}
			}
			if (found) break;
		}
	}
	if (!found) {
		StickerSets::iterator it = sets.find(CustomStickerSetId);
		if (it == sets.cend()) {
			it = sets.insert(CustomStickerSetId, StickerSet(CustomStickerSetId, 0, lang(lng_custom_stickers), QString(), 0, 0, 0));
		}
		it->stickers.push_back(sticker);
		++it->count;
		Local::writeStickers();
	}
	history.updateRecentStickers();
}

void MainWidget::activate() {
	if (!profile && !overview) {
		if (_hider) {
			if (_hider->wasOffered()) {
				_hider->setFocus();
			} else {
				dialogs.activate();
			}
        } else if (App::wnd() && !App::wnd()->layerShown()) {
			if (!cSendPaths().isEmpty()) {
				forwardLayer(-1);
			} else if (history.peer()) {
				history.activate();
			} else {
				dialogs.activate();
			}
		}
	}
	App::wnd()->fixOrder();
}

void MainWidget::destroyData() {
	history.destroyData();
	dialogs.destroyData();
}

void MainWidget::updateOnlineDisplayIn(int32 msecs) {
	_onlineUpdater.start(msecs);
}

void MainWidget::addNewContact(int32 uid, bool show) {
	if (dialogs.addNewContact(uid, show)) {
		showPeerHistory(peerFromUser(uid), ShowAtTheEndMsgId);
	}
}

bool MainWidget::isActive() const {
	return !_isIdle && isVisible() && !animating();
}

bool MainWidget::historyIsActive() const {
	return isActive() && !profile && !overview && history.isActive();
}

bool MainWidget::lastWasOnline() const {
	return _lastWasOnline;
}

uint64 MainWidget::lastSetOnline() const {
	return _lastSetOnline;
}

int32 MainWidget::dlgsWidth() const {
	return dialogs.width();
}

MainWidget::~MainWidget() {
	if (App::main() == this) history.showPeerHistory(0, 0);

	delete _background;

	delete _hider;
	MTP::clearGlobalHandlers();
	delete _api;
	if (App::wnd()) App::wnd()->noMain(this);
}

void MainWidget::updateOnline(bool gotOtherOffline) {
	if (this != App::main()) return;
	App::wnd()->checkAutoLock();

	bool isOnline = App::wnd()->isActive();
	int updateIn = cOnlineUpdatePeriod();
	if (isOnline) {
		uint64 idle = psIdleTime();
		if (idle >= uint64(cOfflineIdleTimeout())) {
			isOnline = false;
			if (!_isIdle) {
				_isIdle = true;
				_idleFinishTimer.start(900);
			}
		} else {
			updateIn = qMin(updateIn, int(cOfflineIdleTimeout() - idle));
		}
	}
	uint64 ms = getms(true);
	if (isOnline != _lastWasOnline || (isOnline && _lastSetOnline + cOnlineUpdatePeriod() <= ms) || (isOnline && gotOtherOffline)) {
		if (_onlineRequest) {
			MTP::cancel(_onlineRequest);
			_onlineRequest = 0;
		}

		_lastWasOnline = isOnline;
		_lastSetOnline = ms;
		_onlineRequest = MTP::send(MTPaccount_UpdateStatus(MTP_bool(!isOnline)));

		if (App::self()) App::self()->onlineTill = unixtime() + (isOnline ? (cOnlineUpdatePeriod() / 1000) : -1);

		_lastSetOnline = getms(true);

		updateOnlineDisplay();
	} else if (isOnline) {
		updateIn = qMin(updateIn, int(_lastSetOnline + cOnlineUpdatePeriod() - ms));
	}
	_onlineTimer.start(updateIn);
}

void MainWidget::checkIdleFinish() {
	if (this != App::main()) return;
	if (psIdleTime() < uint64(cOfflineIdleTimeout())) {
		_idleFinishTimer.stop();
		_isIdle = false;
		updateOnline();
		if (App::wnd()) App::wnd()->checkHistoryActivation();
	} else {
		_idleFinishTimer.start(900);
	}
}

void MainWidget::updateReceived(const mtpPrime *from, const mtpPrime *end) {
	if (end <= from || !MTP::authedId()) return;

	App::wnd()->checkAutoLock();

	if (mtpTypeId(*from) == mtpc_new_session_created) {
		MTPNewSession newSession(from, end);
		updSeq = 0;
		MTP_LOG(0, ("getDifference { after new_session_created }%1").arg(cTestMode() ? " TESTMODE" : ""));
		return getDifference();
	} else {
		try {
			MTPUpdates updates(from, end);

			_lastUpdateTime = getms(true);
			noUpdatesTimer.start(NoUpdatesTimeout);

			handleUpdates(updates);
			App::emitPeerUpdated();
		} catch (mtpErrorUnexpected &e) { // just some other type
		}
	}
	update();
}

void MainWidget::handleUpdates(const MTPUpdates &updates, uint64 randomId) {
	switch (updates.type()) {
	case mtpc_updates: {
		const MTPDupdates &d(updates.c_updates());
		if (d.vseq.v) {
			if (d.vseq.v <= updSeq) return;
			if (d.vseq.v > updSeq + 1) {
				_bySeqUpdates.insert(d.vseq.v, updates);
				return _bySeqTimer.start(WaitForSkippedTimeout);
			}
		}

		App::feedUsers(d.vusers, false);
		App::feedChats(d.vchats, false);
		feedUpdates(d.vupdates);

		updSetState(0, d.vdate.v, updQts, d.vseq.v);
	} break;

	case mtpc_updatesCombined: {
		const MTPDupdatesCombined &d(updates.c_updatesCombined());
		if (d.vseq_start.v) {
			if (d.vseq_start.v <= updSeq) return;
			if (d.vseq_start.v > updSeq + 1) {
				_bySeqUpdates.insert(d.vseq_start.v, updates);
				return _bySeqTimer.start(WaitForSkippedTimeout);
			}
		}

		App::feedUsers(d.vusers, false);
		App::feedChats(d.vchats, false);
		feedUpdates(d.vupdates);

		updSetState(0, d.vdate.v, updQts, d.vseq.v);
	} break;

	case mtpc_updateShort: {
		const MTPDupdateShort &d(updates.c_updateShort());

		feedUpdate(d.vupdate);

		updSetState(0, d.vdate.v, updQts, updSeq);
	} break;

	case mtpc_updateShortMessage: {
		const MTPDupdateShortMessage &d(updates.c_updateShortMessage());
		if (!App::userLoaded(d.vuser_id.v) || (d.has_fwd_from_id() && !App::peerLoaded(peerFromMTP(d.vfwd_from_id)))) {
			MTP_LOG(0, ("getDifference { good - getting user for updateShortMessage }%1").arg(cTestMode() ? " TESTMODE" : ""));
			return getDifference();
		}
		if (!ptsUpdated(d.vpts.v, d.vpts_count.v, updates)) {
			return;
		}
		int32 flags = d.vflags.v | MTPDmessage::flag_from_id;
		bool out = (flags & MTPDmessage_flag_out);
		HistoryItem *item = App::histories().addToBack(MTP_message(MTP_int(flags), d.vid, out ? MTP_int(MTP::authedId()) : d.vuser_id, MTP_peerUser(out ? d.vuser_id : MTP_int(MTP::authedId())), d.vfwd_from_id, d.vfwd_date, d.vreply_to_msg_id, d.vdate, d.vmessage, MTP_messageMediaEmpty(), MTPnullMarkup, d.has_entities() ? d.ventities : MTPnullEntities, MTPint()));
		if (item) {
			history.peerMessagesUpdated(item->history()->peer->id);
		}

		updSetState(0, d.vdate.v, updQts, updSeq);
	} break;

	case mtpc_updateShortChatMessage: {
		const MTPDupdateShortChatMessage &d(updates.c_updateShortChatMessage());
		bool noFrom = !App::userLoaded(d.vfrom_id.v);
		if (!App::chatLoaded(d.vchat_id.v) || noFrom || (d.has_fwd_from_id() && !App::peerLoaded(peerFromMTP(d.vfwd_from_id)))) {
			MTP_LOG(0, ("getDifference { good - getting user for updateShortChatMessage }%1").arg(cTestMode() ? " TESTMODE" : ""));
			if (noFrom) App::api()->requestFullPeer(App::chatLoaded(d.vchat_id.v));
			return getDifference();
		}
		if (!ptsUpdated(d.vpts.v, d.vpts_count.v, updates)) {
			return;
		}
		int32 flags = d.vflags.v | MTPDmessage::flag_from_id;
		bool out = (flags & MTPDmessage_flag_out);
		HistoryItem *item = App::histories().addToBack(MTP_message(MTP_int(flags), d.vid, d.vfrom_id, MTP_peerChat(d.vchat_id), d.vfwd_from_id, d.vfwd_date, d.vreply_to_msg_id, d.vdate, d.vmessage, MTP_messageMediaEmpty(), MTPnullMarkup, d.has_entities() ? d.ventities : MTPnullEntities, MTPint()));
		if (item) {
			history.peerMessagesUpdated(item->history()->peer->id);
		}

		updSetState(0, d.vdate.v, updQts, updSeq);
	} break;

	case mtpc_updateShortSentMessage: {
		const MTPDupdateShortSentMessage &d(updates.c_updateShortSentMessage());
		if (randomId) {
			PeerId peerId = 0;
			QString text;
			App::histSentDataByItem(randomId, peerId, text);

			feedUpdate(MTP_updateMessageID(d.vid, MTP_long(randomId))); // ignore real date
			if (peerId) {
				HistoryItem *item = App::histItemById(peerToChannel(peerId), d.vid.v);
				if (!text.isEmpty()) {
					bool hasLinks = d.has_entities() && !d.ventities.c_vector().v.isEmpty();
					if (item && ((hasLinks && !item->hasTextLinks()) || (!hasLinks && item->textHasLinks()))) {
						bool was = item->hasTextLinks();
						item->setText(text, d.has_entities() ? linksFromMTP(d.ventities.c_vector().v) : LinksInText());
						item->initDimensions();
						itemResized(item);
						if (!was && item->hasTextLinks()) {
							item->history()->addToOverview(item, OverviewLinks);
						}
					}
				}
				if (item) {
					item->setMedia(d.has_media() ? (&d.vmedia) : 0);
				}
			}
		}
		
		if (!ptsUpdated(d.vpts.v, d.vpts_count.v, updates)) {
			return;
		}

		updSetState(0, d.vdate.v, updQts, updSeq);
	} break;

	case mtpc_updatesTooLong: {
		MTP_LOG(0, ("getDifference { good - updatesTooLong received }%1").arg(cTestMode() ? " TESTMODE" : ""));
		return getDifference();
	} break;
	}
}

void MainWidget::feedUpdate(const MTPUpdate &update) {
	if (!MTP::authedId()) return;

	switch (update.type()) {
	case mtpc_updateNewMessage: {
		const MTPDupdateNewMessage &d(update.c_updateNewMessage());
		if (!ptsUpdated(d.vpts.v, d.vpts_count.v, update)) {
			return;
		}
		if (d.vmessage.type() == mtpc_message) { // index forwarded messages to links overview
			App::checkEntitiesAndViewsUpdate(d.vmessage.c_message());
		}

		HistoryItem *item = App::histories().addToBack(d.vmessage);
		if (item) {
			history.peerMessagesUpdated(item->history()->peer->id);
		}
	} break;

	case mtpc_updateMessageID: {
		const MTPDupdateMessageID &d(update.c_updateMessageID());
		FullMsgId msg = App::histItemByRandom(d.vrandom_id.v);
		if (msg.msg) {
			HistoryItem *msgRow = App::histItemById(msg);
			if (msgRow) {
				App::historyUnregItem(msgRow);
				History *h = msgRow->history();
				for (int32 i = 0; i < OverviewCount; ++i) {
					History::MediaOverviewIds::iterator j = h->_overviewIds[i].find(msgRow->id);
					if (j != h->_overviewIds[i].cend()) {
						h->_overviewIds[i].erase(j);
						if (h->_overviewIds[i].constFind(d.vid.v) == h->_overviewIds[i].cend()) {
							h->_overviewIds[i].insert(d.vid.v, NullType());
							for (int32 k = 0, l = h->_overview[i].size(); k != l; ++k) {
								if (h->_overview[i].at(k) == msgRow->id) {
									h->_overview[i][k] = d.vid.v;
									break;
								}
							}
						}
					}
				}
				if (App::wnd()) App::wnd()->changingMsgId(msgRow, d.vid.v);
				msgRow->id = d.vid.v;
				if (!App::historyRegItem(msgRow)) {
					msgUpdated(h->peer->id, msgRow);
				} else {
					msgRow->destroy();
					history.peerMessagesUpdated();
				}
			}
			App::historyUnregRandom(d.vrandom_id.v);
		}
		App::historyUnregSentData(d.vrandom_id.v);
	} break;

	case mtpc_updateReadMessagesContents: {
		const MTPDupdateReadMessagesContents &d(update.c_updateReadMessagesContents());
		if (!ptsUpdated(d.vpts.v, d.vpts_count.v, update)) {
			return;
		}
		const QVector<MTPint> &v(d.vmessages.c_vector().v);
		for (int32 i = 0, l = v.size(); i < l; ++i) {
			if (HistoryItem *item = App::histItemById(NoChannel, v.at(i).v)) {
				if (item->isMediaUnread()) {
					item->markMediaRead();
					msgUpdated(item->history()->peer->id, item);
					if (item->out() && item->history()->peer->isUser()) {
						item->history()->peer->asUser()->madeAction();
					}
				}
			}
		}
	} break;

	case mtpc_updateReadHistoryInbox: {
		const MTPDupdateReadHistoryInbox &d(update.c_updateReadHistoryInbox());
		if (!ptsUpdated(d.vpts.v, d.vpts_count.v, update)) {
			return;
		}
		App::feedInboxRead(peerFromMTP(d.vpeer), d.vmax_id.v);
	} break;

	case mtpc_updateReadHistoryOutbox: {
		const MTPDupdateReadHistoryOutbox &d(update.c_updateReadHistoryOutbox());
		if (!ptsUpdated(d.vpts.v, d.vpts_count.v, update)) {
			return;
		}
		PeerId id = peerFromMTP(d.vpeer);
		App::feedOutboxRead(id, d.vmax_id.v);
		if (history.peer() && history.peer()->id == id) history.update();
	} break;

	case mtpc_updateWebPage: {
		const MTPDupdateWebPage &d(update.c_updateWebPage());
		if (!ptsUpdated(d.vpts.v, d.vpts_count.v, update)) {
			return;
		}
		App::feedWebPage(d.vwebpage);
		history.updatePreview();
		webPagesUpdate();
	} break;

	case mtpc_updateDeleteMessages: {
		const MTPDupdateDeleteMessages &d(update.c_updateDeleteMessages());
		if (!ptsUpdated(d.vpts.v, d.vpts_count.v, update)) {
			return;
		}
		App::feedWereDeleted(NoChannel, d.vmessages.c_vector().v);
		history.peerMessagesUpdated();
	} break;

	case mtpc_updateUserTyping: {
		const MTPDupdateUserTyping &d(update.c_updateUserTyping());
		History *history = App::historyLoaded(peerFromUser(d.vuser_id));
		UserData *user = App::userLoaded(d.vuser_id.v);
		if (history && user) {
			App::histories().regSendAction(history, user, d.vaction);
		}
	} break;

	case mtpc_updateChatUserTyping: {
		const MTPDupdateChatUserTyping &d(update.c_updateChatUserTyping());
		History *history = 0;
		if (PeerData *chat = App::peerLoaded(peerFromChat(d.vchat_id.v))) {
			history = App::historyLoaded(chat->id);
		} else if (PeerData *channel = App::peerLoaded(peerFromChannel(d.vchat_id.v))) {
			history = App::historyLoaded(channel->id);
		}
		UserData *user = (d.vuser_id.v == MTP::authedId()) ? 0 : App::userLoaded(d.vuser_id.v);
		if (history && user) {
			App::histories().regSendAction(history, user, d.vaction);
		}
	} break;

	case mtpc_updateChatParticipants: {
		const MTPDupdateChatParticipants &d(update.c_updateChatParticipants());
		App::feedParticipants(d.vparticipants, true, false);
	} break;

	case mtpc_updateChatParticipantAdd: {
		const MTPDupdateChatParticipantAdd &d(update.c_updateChatParticipantAdd());
		App::feedParticipantAdd(d, false);
	} break;

	case mtpc_updateChatParticipantDelete: {
		const MTPDupdateChatParticipantDelete &d(update.c_updateChatParticipantDelete());
		App::feedParticipantDelete(d, false);
	} break;

	case mtpc_updateUserStatus: {
		const MTPDupdateUserStatus &d(update.c_updateUserStatus());
		UserData *user = App::userLoaded(d.vuser_id.v);
		if (user) {
			switch (d.vstatus.type()) {
			case mtpc_userStatusEmpty: user->onlineTill = 0; break;
			case mtpc_userStatusRecently:
				if (user->onlineTill > -10) { // don't modify pseudo-online
					user->onlineTill = -2;
				}
			break;
			case mtpc_userStatusLastWeek: user->onlineTill = -3; break;
			case mtpc_userStatusLastMonth: user->onlineTill = -4; break;
			case mtpc_userStatusOffline: user->onlineTill = d.vstatus.c_userStatusOffline().vwas_online.v; break;
			case mtpc_userStatusOnline: user->onlineTill = d.vstatus.c_userStatusOnline().vexpires.v; break;
			}
			App::markPeerUpdated(user);
		}
		if (d.vuser_id.v == MTP::authedId()) {
			if (d.vstatus.type() == mtpc_userStatusOffline || d.vstatus.type() == mtpc_userStatusEmpty) {
				updateOnline(true);
				if (d.vstatus.type() == mtpc_userStatusOffline) {
					cSetOtherOnline(d.vstatus.c_userStatusOffline().vwas_online.v);
				}
			} else if (d.vstatus.type() == mtpc_userStatusOnline) {
				cSetOtherOnline(d.vstatus.c_userStatusOnline().vexpires.v);
			}
		}
	} break;

	case mtpc_updateUserName: {
		const MTPDupdateUserName &d(update.c_updateUserName());
		UserData *user = App::userLoaded(d.vuser_id.v);
		if (user) {
			if (user->contact <= 0) {
				user->setName(textOneLine(qs(d.vfirst_name)), textOneLine(qs(d.vlast_name)), user->nameOrPhone, textOneLine(qs(d.vusername)));
			} else {
				user->setName(textOneLine(user->firstName), textOneLine(user->lastName), user->nameOrPhone, textOneLine(qs(d.vusername)));
			}
			App::markPeerUpdated(user);
		}
	} break;

	case mtpc_updateUserPhoto: {
		const MTPDupdateUserPhoto &d(update.c_updateUserPhoto());
		UserData *user = App::userLoaded(d.vuser_id.v);
		if (user) {
			user->setPhoto(d.vphoto);
			user->photo->load();
			if (d.vprevious.v) {
				user->photosCount = -1;
				user->photos.clear();
			} else {
				if (user->photoId && user->photoId != UnknownPeerPhotoId) {
					if (user->photosCount > 0) ++user->photosCount;
					user->photos.push_front(App::photo(user->photoId));
				} else {
					user->photosCount = -1;
					user->photos.clear();
				}
			}
			App::markPeerUpdated(user);
			if (App::wnd()) App::wnd()->mediaOverviewUpdated(user, OverviewCount);
		}
	} break;

	case mtpc_updateContactRegistered: {
		const MTPDupdateContactRegistered &d(update.c_updateContactRegistered());
		UserData *user = App::userLoaded(d.vuser_id.v);
		if (user) {
			if (App::history(user->id)->loadedAtBottom()) {
				App::history(user->id)->addToBackService(clientMsgId(), date(d.vdate), lng_action_user_registered(lt_from, user->name), MTPDmessage_flag_unread);
			}
		}
	} break;

	case mtpc_updateContactLink: {
		const MTPDupdateContactLink &d(update.c_updateContactLink());
		App::feedUserLink(d.vuser_id, d.vmy_link, d.vforeign_link, false);
	} break;

	case mtpc_updateNotifySettings: {
		const MTPDupdateNotifySettings &d(update.c_updateNotifySettings());
		applyNotifySetting(d.vpeer, d.vnotify_settings);
	} break;

	case mtpc_updateDcOptions: {
		const MTPDupdateDcOptions &d(update.c_updateDcOptions());
		MTP::updateDcOptions(d.vdc_options.c_vector().v);
	} break;

	case mtpc_updateUserPhone: {
		const MTPDupdateUserPhone &d(update.c_updateUserPhone());
		UserData *user = App::userLoaded(d.vuser_id.v);
		if (user) {
			user->setPhone(qs(d.vphone));
			user->setName(user->firstName, user->lastName, (user->contact || isServiceUser(user->id) || user->input.type() == mtpc_inputPeerSelf || user->phone.isEmpty()) ? QString() : App::formatPhone(user->phone), user->username);
			App::markPeerUpdated(user);
		}
	} break;

	case mtpc_updateNewEncryptedMessage: {
		const MTPDupdateNewEncryptedMessage &d(update.c_updateNewEncryptedMessage());
	} break;

	case mtpc_updateEncryptedChatTyping: {
		const MTPDupdateEncryptedChatTyping &d(update.c_updateEncryptedChatTyping());
	} break;

	case mtpc_updateEncryption: {
		const MTPDupdateEncryption &d(update.c_updateEncryption());
	} break;

	case mtpc_updateEncryptedMessagesRead: {
		const MTPDupdateEncryptedMessagesRead &d(update.c_updateEncryptedMessagesRead());
	} break;

	case mtpc_updateUserBlocked: {
		const MTPDupdateUserBlocked &d(update.c_updateUserBlocked());
		if (UserData *user = App::userLoaded(d.vuser_id.v)) {
			user->blocked = d.vblocked.v ? UserIsBlocked : UserIsNotBlocked;
			App::markPeerUpdated(user);
		}
	} break;

	case mtpc_updateNewAuthorization: {
		const MTPDupdateNewAuthorization &d(update.c_updateNewAuthorization());
		QDateTime datetime = date(d.vdate);

		QString name = App::self()->firstName;
		QString day = langDayOfWeekFull(datetime.date()), date = langDayOfMonth(datetime.date()), time = datetime.time().toString(cTimeFormat());
		QString device = qs(d.vdevice), location = qs(d.vlocation);
		LangString text = lng_new_authorization(lt_name, App::self()->firstName, lt_day, day, lt_date, date, lt_time, time, lt_device, device, lt_location, location);
		App::wnd()->serviceNotification(text);

		emit App::wnd()->newAuthorization();
	} break;

	case mtpc_updateServiceNotification: {
		const MTPDupdateServiceNotification &d(update.c_updateServiceNotification());
		if (d.vpopup.v) {
			App::wnd()->showLayer(new ConfirmBox(qs(d.vmessage), true));
			App::wnd()->serviceNotification(qs(d.vmessage), false, d.vmedia);
		} else {
			App::wnd()->serviceNotification(qs(d.vmessage), true, d.vmedia);
		}
	} break;

	case mtpc_updatePrivacy: {
		const MTPDupdatePrivacy &d(update.c_updatePrivacy());
	} break;

	case mtpc_updateNewChannelMessage: {
		const MTPDupdateNewChannelMessage &d(update.c_updateNewChannelMessage());
		PeerId peer = peerFromMessage(d.vmessage);
		if (ChannelData *channel = App::channelLoaded(peerToChannel(peer))) {
			if (!channel->ptsUpdated(d.vpts.v, d.vpts_count.v, update)) {
				return;
			}
		}
		if (d.vmessage.type() == mtpc_message) { // index forwarded messages to links overview
			App::checkEntitiesAndViewsUpdate(d.vmessage.c_message());
		}

		HistoryItem *item = App::histories().addToBack(d.vmessage);
		if (item) {
			history.peerMessagesUpdated(item->history()->peer->id);
		}
	} break;

	case mtpc_updateReadChannelInbox: {
		const MTPDupdateReadChannelInbox &d(update.c_updateReadChannelInbox());
		App::feedInboxRead(peerFromMTP(d.vpeer), d.vmax_id.v);
	} break;

	case mtpc_updateDeleteChannelMessages: {
		const MTPDupdateDeleteChannelMessages &d(update.c_updateDeleteChannelMessages());
		PeerId peer = peerFromMTP(d.vpeer);
		if (ChannelData *channel = App::channelLoaded(peerToChannel(peer))) {
			if (!channel->ptsUpdated(d.vpts.v, d.vpts_count.v, update)) {
				return;
			}
		}
		App::feedWereDeleted(peerToChannel(peer), d.vmessages.c_vector().v);
		history.peerMessagesUpdated();
	} break;

	case mtpc_updateChannelGroup: {
		const MTPDupdateChannelGroup &d(update.c_updateChannelGroup());
		if (ChannelData *channel = App::channelLoaded(d.vchannel_id.v)) {
			if (d.vgroup.type() == mtpc_messageGroup) {
				const MTPDmessageGroup &data(d.vgroup.c_messageGroup());
				// CHANNELS_FULL
			}
		}
	} break;

	case mtpc_updateChannelTooLong: {
		const MTPDupdateChannelTooLong &d(update.c_updateChannelTooLong());
		if (ChannelData *channel = App::channelLoaded(d.vchannel_id.v)) {
			getChannelDifference(channel);
		}
	} break;

	case mtpc_updateChannelMessageViews: {
		const MTPDupdateChannelMessageViews &d(update.c_updateChannelMessageViews());
		if (HistoryItem *item = App::histItemById(peerToChannel(peerFromMTP(d.vpeer)), d.vid.v)) {
			if (item->from()->id == peerFromMTP(d.vpeer) && item->channelId() != NoChannel) {
				item->setViewsCount(d.vviews.v);
			}
		}
	} break;

	}
}
