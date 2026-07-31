// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-FileCopyrightText: Copyright 2025 Citron Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <thread>
#include <QDesktopServices>
#include <QMessageBox>
#include <QUrl>
#include <QtConcurrent/QtConcurrent>
#include "common/nextendo_account.h"
#include "common/nextendo_friends.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/hle/service/acc/profile_manager.h"
#include "core/internal_network/network_interface.h"
#include "ui_configure_network.h"
#include "citron/configuration/configure_network.h"
#include "citron/nextendo_friends_dialog.h"

#ifdef ENABLE_WEB_SERVICE
#include "web_service/nextendo_api.h"
#endif

ConfigureNetwork::ConfigureNetwork(const Core::System& system_, QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::ConfigureNetwork>()), system{system_} {
    ui->setupUi(this);

    ui->network_interface->addItem(tr("None"));
    for (const auto& iface : Network::GetAvailableNetworkInterfaces()) {
        ui->network_interface->addItem(QString::fromStdString(iface.name));
    }

    this->SetConfiguration();

    // Store the initial URL
    original_lobby_api_url = Settings::values.lobby_api_url.GetValue();

    connect(ui->restore_default_lobby_api, &QPushButton::clicked, this, &ConfigureNetwork::OnRestoreDefaultLobbyApi);
    connect(ui->nextendo_login_button, &QPushButton::clicked, this,
            &ConfigureNetwork::OnNextendoSignIn);
    connect(ui->nextendo_logout_button, &QPushButton::clicked, this,
            &ConfigureNetwork::OnNextendoSignOut);
    connect(ui->nextendo_friends_button, &QPushButton::clicked, this,
            [this] { NextendoFriendsDialog(this).exec(); });

#ifndef ENABLE_WEB_SERVICE
    ui->nextendo_login_button->setEnabled(false);
    ui->nextendo_logout_button->setEnabled(false);
    ui->nextendo_friends_button->setEnabled(false);
#endif

    UpdateNextendoAccountStatus();
}

ConfigureNetwork::~ConfigureNetwork() = default;

void ConfigureNetwork::ApplyConfiguration() {
    // Apply all settings from the UI to the settings system
    Settings::values.airplane_mode = ui->airplane_mode->isChecked();
    Settings::values.network_interface = ui->network_interface->currentText().toStdString();
    Settings::values.lobby_api_url = ui->lobby_api_url->text().toStdString();

    Settings::values.enable_nextendo = ui->enable_nextendo->isChecked();
}

void ConfigureNetwork::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }
    QWidget::changeEvent(event);
}

void ConfigureNetwork::RetranslateUI() {
    ui->retranslateUi(this);
}

void ConfigureNetwork::SetConfiguration() {
    const bool runtime_lock = !system.IsPoweredOn();

    ui->airplane_mode->setChecked(Settings::values.airplane_mode.GetValue());
    ui->airplane_mode->setEnabled(runtime_lock);

    const std::string& network_interface = Settings::values.network_interface.GetValue();
    ui->network_interface->setCurrentText(QString::fromStdString(network_interface));

    ui->lobby_api_url->setText(QString::fromStdString(Settings::values.lobby_api_url.GetValue()));

    ui->enable_nextendo->setChecked(Settings::values.enable_nextendo.GetValue());

    const bool networking_enabled = runtime_lock && !ui->airplane_mode->isChecked();
    ui->network_interface->setEnabled(networking_enabled);
    ui->lobby_api_url->setEnabled(networking_enabled);
    ui->restore_default_lobby_api->setEnabled(networking_enabled);

    ui->nextendo_group->setEnabled(networking_enabled);

    connect(ui->airplane_mode, &QCheckBox::toggled, this, [this, runtime_lock](bool checked) {
        const bool enabled = !checked && runtime_lock;
        ui->network_interface->setEnabled(enabled);
        ui->lobby_api_url->setEnabled(enabled);
        ui->restore_default_lobby_api->setEnabled(enabled);
        ui->nextendo_group->setEnabled(enabled);
    });
}

void ConfigureNetwork::OnRestoreDefaultLobbyApi() {
    ui->lobby_api_url->setText(QString::fromStdString(Settings::values.lobby_api_url.GetDefault()));
}

void ConfigureNetwork::UpdateNextendoAccountStatus() {
    const bool linked = Common::NextendoAccount::IsLinked();

    if (linked) {
        const QString name = QString::fromStdString(Common::NextendoAccount::GetUsername());
        const QString code = QString::fromStdString(Common::NextendoAccount::GetFriendCode());
        // Deliberately no Network ID: a bare PID is accepted as an identity by the online
        // service, so showing it hands anyone who sees the screen the account.
        ui->nextendo_status->setText(tr("Signed in as %1 (%2).")
                                         .arg(name.isEmpty() ? tr("(unnamed)") : name)
                                         .arg(code.isEmpty() ? tr("no friend code") : code));
    } else {
        ui->nextendo_status->setText(
            tr("Not signed in. Signing in opens nextendo.network in your browser; citron never sees "
               "your password."));
    }

    ui->nextendo_logout_button->setEnabled(linked);
    ui->nextendo_friends_button->setEnabled(linked);
}

void ConfigureNetwork::OnNextendoSignIn() {
#ifdef ENABLE_WEB_SERVICE
    ui->nextendo_login_button->setEnabled(false);
    ui->nextendo_status->setText(tr("Finish signing in in your browser, then come back here."));

    std::thread{[this] {
        // The browser opens on the UI thread; the flow itself waits on a loopback callback.
        const auto open_url = [this](const std::string& url) {
            QMetaObject::invokeMethod(
                this,
                [url] { QDesktopServices::openUrl(QUrl(QString::fromStdString(url))); },
                Qt::QueuedConnection);
        };

        auto login_result = WebService::NextendoApi::SignInWithBrowser(open_url);

        QMetaObject::invokeMethod(
            this,
            [this, result = std::move(login_result)] {
                ui->nextendo_login_button->setEnabled(true);

                if (!result.ok) {
                    ui->nextendo_status->setText(QString::fromStdString(result.error));
                    return;
                }

                Common::NextendoAccount::Save(result.pid, result.username, result.friend_code,
                                              result.token);
                ApplyNextendoProfileName(result.username);
                UpdateNextendoAccountStatus();
                Common::NextendoFriends::SetLocalStatus(
                    Common::NextendoFriends::PresenceOnline);
                RefreshNextendoFriendCache();
            },
            Qt::QueuedConnection);
    }}.detach();
#endif
}

void ConfigureNetwork::OnNextendoSignOut() {
    Common::NextendoAccount::Clear();
    UpdateNextendoAccountStatus();
}

// Games read the player name from the Switch profile, not from the account, so a local profile
// still called "Player" shows up as "Player" online. Rename the active profile to match.
void ConfigureNetwork::ApplyNextendoProfileName(const std::string& name) {
    if (name.empty()) {
        return;
    }

    Service::Account::ProfileManager profile_manager;
    const auto uuid = profile_manager.GetLastOpenedUser();
    if (uuid.IsInvalid()) {
        return;
    }

    Service::Account::ProfileBase profile{};
    if (!profile_manager.GetProfileBase(uuid, profile)) {
        return;
    }

    const std::string trimmed = name.substr(0, profile.username.size() - 1);
    std::fill(profile.username.begin(), profile.username.end(), '\0');
    std::copy(trimmed.begin(), trimmed.end(), profile.username.begin());

    profile_manager.SetProfileBase(uuid, profile);
    profile_manager.WriteUserSaveFile();
    LOG_INFO(Frontend, "[Nextendo] Renamed the active profile to the account nickname");
}

// Primes the friends snapshot the friend service reads, so a game launched without ever opening
// the friends window still sees the account's friends.
void ConfigureNetwork::RefreshNextendoFriendCache() {
#ifdef ENABLE_WEB_SERVICE
    std::thread{[] {
        auto list = WebService::NextendoApi::GetFriends();
        if (!list.ok) {
            return;
        }
        std::vector<Common::NextendoFriends::Entry> cache;
        cache.reserve(list.friends.size());
        for (const auto& entry : list.friends) {
            cache.push_back({entry.pid, entry.name, entry.presence_status, entry.app_field});
        }
        Common::NextendoFriends::Set(std::move(cache));
    }}.detach();
#endif
}
