// SPDX-FileCopyrightText: Copyright 2026 citron Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <string>
#include <QDialog>

#include "common/common_types.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

// Friends on the Nextendo account: the list, incoming requests, and add by friend code. Every
// call goes out on a worker thread so a slow server cannot freeze the dialog.
class NextendoFriendsDialog : public QDialog {
    Q_OBJECT

public:
    explicit NextendoFriendsDialog(QWidget* parent = nullptr);
    ~NextendoFriendsDialog() override;

private:
    void Refresh();
    void SetBusy(bool busy);
    void OnAdd();
    void OnAccept();
    void OnDecline();
    void OnRemove();

    // Runs `work` off the UI thread, then refreshes; a non-empty return is shown as an error.
    void RunAsync(std::function<std::string()> task);

    // pid of the selected row in either tree, 0 when nothing usable is selected.
    u64 SelectedPid(QTreeWidget* tree) const;

    QLabel* status;
    QTreeWidget* friends_tree;
    QTreeWidget* requests_tree;
    QLineEdit* friend_code_input;
    QPushButton* add_button;
    QPushButton* accept_button;
    QPushButton* decline_button;
    QPushButton* remove_button;
    QPushButton* refresh_button;
};
