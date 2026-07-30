// SPDX-FileCopyrightText: Copyright 2026 citron Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <thread>
#include <utility>

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "common/logging.h"
#include "common/nextendo_account.h"
#include "common/nextendo_friends.h"
#include "citron/nextendo_friends_dialog.h"

#ifdef ENABLE_WEB_SERVICE
#include "web_service/nextendo_api.h"
#endif

namespace {

constexpr int PidRole = Qt::UserRole + 1;

QString PresenceText(s32 status) {
    switch (status) {
    case 1:
        return QObject::tr("Online");
    case 2:
        return QObject::tr("In a game");
    default:
        return QObject::tr("Offline");
    }
}

QTreeWidget* MakeTree(const QStringList& headers) {
    auto* tree = new QTreeWidget;
    tree->setColumnCount(headers.size());
    tree->setHeaderLabels(headers);
    tree->setRootIsDecorated(false);
    tree->setSelectionBehavior(QAbstractItemView::SelectRows);
    tree->setSelectionMode(QAbstractItemView::SingleSelection);
    tree->header()->setStretchLastSection(true);
    return tree;
}

} // Anonymous namespace

NextendoFriendsDialog::NextendoFriendsDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Nextendo Friends"));
    resize(560, 480);

    status = new QLabel;
    status->setWordWrap(true);

    friends_tree = MakeTree({tr("Name"), tr("Friend Code"), tr("Status")});
    requests_tree = MakeTree({tr("Name"), tr("Friend Code")});

    friend_code_input = new QLineEdit;
    friend_code_input->setPlaceholderText(tr("SW-0000-0000-0000"));
    add_button = new QPushButton(tr("Add"));
    refresh_button = new QPushButton(tr("Refresh"));
    accept_button = new QPushButton(tr("Accept"));
    decline_button = new QPushButton(tr("Decline"));
    remove_button = new QPushButton(tr("Remove"));

    auto* add_row = new QHBoxLayout;
    add_row->addWidget(friend_code_input);
    add_row->addWidget(add_button);

    auto* request_row = new QHBoxLayout;
    request_row->addWidget(accept_button);
    request_row->addWidget(decline_button);
    request_row->addStretch();

    auto* friend_row = new QHBoxLayout;
    friend_row->addWidget(remove_button);
    friend_row->addStretch();
    friend_row->addWidget(refresh_button);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Your friend code: %1")
                                     .arg(QString::fromStdString(
                                         Common::NextendoAccount::GetFriendCode()))));
    layout->addLayout(add_row);
    layout->addWidget(new QLabel(tr("Friends")));
    layout->addWidget(friends_tree, 3);
    layout->addLayout(friend_row);
    layout->addWidget(new QLabel(tr("Friend requests")));
    layout->addWidget(requests_tree, 2);
    layout->addLayout(request_row);
    layout->addWidget(status);
    layout->addWidget(buttons);

    connect(add_button, &QPushButton::clicked, this, &NextendoFriendsDialog::OnAdd);
    connect(refresh_button, &QPushButton::clicked, this, &NextendoFriendsDialog::Refresh);
    connect(accept_button, &QPushButton::clicked, this, &NextendoFriendsDialog::OnAccept);
    connect(decline_button, &QPushButton::clicked, this, &NextendoFriendsDialog::OnDecline);
    connect(remove_button, &QPushButton::clicked, this, &NextendoFriendsDialog::OnRemove);
    connect(friend_code_input, &QLineEdit::returnPressed, this, &NextendoFriendsDialog::OnAdd);

    Refresh();
}

NextendoFriendsDialog::~NextendoFriendsDialog() = default;

void NextendoFriendsDialog::SetBusy(bool busy) {
    add_button->setEnabled(!busy);
    refresh_button->setEnabled(!busy);
    accept_button->setEnabled(!busy);
    decline_button->setEnabled(!busy);
    remove_button->setEnabled(!busy);
}

u64 NextendoFriendsDialog::SelectedPid(QTreeWidget* tree) const {
    const auto* item = tree->currentItem();
    return item ? item->data(0, PidRole).toULongLong() : 0;
}

void NextendoFriendsDialog::Refresh() {
#ifdef ENABLE_WEB_SERVICE
    SetBusy(true);
    status->setText(tr("Loading..."));

    std::thread{[this] {
        auto fetched = WebService::NextendoApi::GetFriends();

        QMetaObject::invokeMethod(
            this,
            [this, list = std::move(fetched)] {
                SetBusy(false);
                friends_tree->clear();
                requests_tree->clear();

                if (!list.ok) {
                    status->setText(QString::fromStdString(list.error));
                    return;
                }

                std::vector<Common::NextendoFriends::Entry> cache;
                cache.reserve(list.friends.size());
                for (const auto& entry : list.friends) {
                    cache.push_back({entry.pid, entry.name, entry.presence_status, entry.app_field});
                }
                Common::NextendoFriends::Set(std::move(cache));

                for (const auto& entry : list.friends) {
                    auto* item = new QTreeWidgetItem(
                        {QString::fromStdString(entry.name),
                         QString::fromStdString(entry.friend_code),
                         PresenceText(entry.presence_status)});
                    item->setData(0, PidRole, QVariant::fromValue<qulonglong>(entry.pid));
                    friends_tree->addTopLevelItem(item);
                }
                for (const auto& entry : list.requests) {
                    auto* item =
                        new QTreeWidgetItem({QString::fromStdString(entry.name),
                                             QString::fromStdString(entry.friend_code)});
                    item->setData(0, PidRole, QVariant::fromValue<qulonglong>(entry.pid));
                    requests_tree->addTopLevelItem(item);
                }

                status->setText(tr("%1 friend(s), %2 request(s).")
                                    .arg(list.friends.size())
                                    .arg(list.requests.size()));
            },
            Qt::QueuedConnection);
    }}.detach();
#else
    status->setText(tr("This build has no web services support."));
    SetBusy(false);
#endif
}

void NextendoFriendsDialog::RunAsync(std::function<std::string()> task) {
#ifdef ENABLE_WEB_SERVICE
    SetBusy(true);
    status->setText(tr("Working..."));

    std::thread{[this, work = std::move(task)] {
        const std::string result = work();

        QMetaObject::invokeMethod(
            this,
            [this, error = result] {
                if (error.empty()) {
                    Refresh();
                } else {
                    SetBusy(false);
                    status->setText(QString::fromStdString(error));
                }
            },
            Qt::QueuedConnection);
    }}.detach();
#endif
}

void NextendoFriendsDialog::OnAdd() {
#ifdef ENABLE_WEB_SERVICE
    const std::string code = friend_code_input->text().trimmed().toStdString();
    if (code.empty()) {
        status->setText(tr("Enter a friend code first."));
        return;
    }
    friend_code_input->clear();
    RunAsync([code] { return WebService::NextendoApi::AddFriendByCode(code); });
#endif
}

void NextendoFriendsDialog::OnAccept() {
#ifdef ENABLE_WEB_SERVICE
    const u64 pid = SelectedPid(requests_tree);
    if (pid == 0) {
        status->setText(tr("Select a request first."));
        return;
    }
    RunAsync([pid] { return WebService::NextendoApi::AcceptFriend(pid); });
#endif
}

void NextendoFriendsDialog::OnDecline() {
#ifdef ENABLE_WEB_SERVICE
    const u64 pid = SelectedPid(requests_tree);
    if (pid == 0) {
        status->setText(tr("Select a request first."));
        return;
    }
    RunAsync([pid] { return WebService::NextendoApi::DeclineFriend(pid); });
#endif
}

void NextendoFriendsDialog::OnRemove() {
#ifdef ENABLE_WEB_SERVICE
    const u64 pid = SelectedPid(friends_tree);
    if (pid == 0) {
        status->setText(tr("Select a friend first."));
        return;
    }
    RunAsync([pid] { return WebService::NextendoApi::RemoveFriend(pid); });
#endif
}
