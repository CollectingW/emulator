// SPDX-FileCopyrightText: Copyright 2026 citron Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <string>
#include <QDialog>

#include "common/common_types.h"

class QLabel;
class QLineEdit;
class QListView;
class QPushButton;
class QStackedWidget;
class QStandardItemModel;
class QModelIndex;
class NextendoController;
class NextendoFriendDelegate;

// Reachable from the NexTendo toolbar menu's "Open Account Page" entry.
class NextendoAccountDialog : public QDialog {
    Q_OBJECT

public:
    explicit NextendoAccountDialog(NextendoController* controller, QWidget* parent = nullptr);
    ~NextendoAccountDialog() override;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void RefreshFriends();
    void RefreshHistory();
    void SetBusy(bool busy);
    void OnAdd();

    void RunAsync(std::function<std::string()> task, std::function<void()> on_success = nullptr);

    void OnFriendsViewClicked(const QModelIndex& index);
    u64 SelectedPid(const QModelIndex& index) const;

    NextendoController* controller;

    QLabel* header_avatar;
    QLabel* header_name;
    QLabel* header_code;
    QLabel* status;

    QListView* friends_view;
    QStandardItemModel* friends_model;
    QStackedWidget* friends_stack;
    QListView* requests_view;
    QStandardItemModel* requests_model;
    QStackedWidget* requests_stack;
    QListView* history_view;
    QStandardItemModel* history_model;
    QStackedWidget* history_stack;
    NextendoFriendDelegate* friend_delegate;
    NextendoFriendDelegate* request_delegate;

    QLineEdit* friend_code_input;
    QPushButton* add_button;
};
