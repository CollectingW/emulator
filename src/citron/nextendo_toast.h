// SPDX-FileCopyrightText: Copyright 2026 citron Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QPixmap>
#include <QString>
#include <QTimer>
#include <QWidget>

class QPropertyAnimation;
class QPaintEvent;
class QMouseEvent;

// Passive "<friend> is now playing <game>" popup, top-right of the main window. No invite
// action, dismisses on timeout or click. Only shown while main_window is active and not
// minimized -- see NextendoToast::Show.
class NextendoToast : public QWidget {
    Q_OBJECT

public:
    explicit NextendoToast(QWidget* main_window);
    ~NextendoToast() override;

    void Show(const QString& headline, const QString& detail, const QString& avatar_base64);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;

private:
    void Reposition();
    void HideAnimated();
    float ComputeScale() const;

    QWidget* main_window;
    QTimer auto_hide_timer;
    QPropertyAnimation* fade;
    QPixmap avatar;
    QString line1;
    QString line2;
    float scale = 1.0f; // recomputed per Show() from the current screen's resolution
};
