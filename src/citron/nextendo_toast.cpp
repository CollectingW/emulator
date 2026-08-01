// SPDX-FileCopyrightText: Copyright 2026 citron Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "citron/nextendo_toast.h"

#include <algorithm>

#include <QApplication>
#include <QFont>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QScreen>

#include "citron/nextendo_avatar_cache.h"
#include "citron/uisettings.h"

namespace {
// Sizes at scale=1.0 (see NextendoToast::ComputeScale) -- a 1920-wide screen or narrower. This is
// the visible card; the widget itself is larger by 2*kShadowMargin so the painted soft shadow has
// room to bleed into without being clipped.
constexpr int kWidth = 264;
constexpr int kHeight = 64;
constexpr int kMargin = 16;      // gap from the screen edge
constexpr int kTopExtra = 10;    // extra drop below kMargin, clear of the toolbar row
constexpr int kShadowMargin = 14;
constexpr int kAvatarSize = 40;
constexpr int kAutoHideMs = 6000;
constexpr int kFadeMs = 250;
} // Anonymous namespace

NextendoToast::NextendoToast(QWidget* main_window_)
    : QWidget(main_window_), main_window(main_window_),
      fade(new QPropertyAnimation(this, "windowOpacity", this)) {
    // Qt::Tool maps to xdg_toplevel on Wayland, which the client cannot position at all -- the
    // compositor centers it and ignores move(). Qt::ToolTip maps to xdg_popup instead, which
    // Wayland compositors are required to place at the client's requested position.
    setWindowFlags(Qt::FramelessWindowHint | Qt::ToolTip | Qt::WindowStaysOnTopHint |
                  Qt::WindowDoesNotAcceptFocus | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    resize(kWidth, kHeight);
    setWindowOpacity(0.0);
    hide();

    auto_hide_timer.setSingleShot(true);
    connect(&auto_hide_timer, &QTimer::timeout, this, &NextendoToast::HideAnimated);
}

NextendoToast::~NextendoToast() = default;

float NextendoToast::ComputeScale() const {
    QScreen* screen = main_window ? main_window->screen() : nullptr;
    if (!screen) {
        return 1.0f;
    }
    // 1.0 up to a 1920-wide screen, growing to 2.0 by 3840-wide (4K); clamped past either end.
    constexpr float kReferenceWidth = 1920.0f;
    constexpr float kMaxScale = 2.0f;
    return std::clamp(screen->geometry().width() / kReferenceWidth, 1.0f, kMaxScale);
}

void NextendoToast::Show(const QString& headline, const QString& detail,
                         const QString& avatar_base64) {
    // main_window's geometry (and Reposition() below) is unreliable while minimized/unfocused.
    if (!main_window || main_window->isMinimized() || !main_window->isActiveWindow()) {
        return;
    }

    scale = ComputeScale();
    const int shadow_margin = static_cast<int>(kShadowMargin * scale);
    resize(static_cast<int>(kWidth * scale) + 2 * shadow_margin,
          static_cast<int>(kHeight * scale) + 2 * shadow_margin);

    line1 = headline;
    line2 = detail;
    avatar = Nextendo::AvatarCache::Get(headline.toStdString(), avatar_base64.toStdString(),
                                        static_cast<int>(kAvatarSize * scale));

    Reposition();
    show();
    raise();
    // Some WMs ignore a WA_ShowWithoutActivating window's requested position at map time and
    // auto-place it instead, but honor move() once it's already shown -- reassert it a tick later.
    QTimer::singleShot(0, this, &NextendoToast::Reposition);

    fade->stop();
    fade->setDuration(kFadeMs);
    fade->setStartValue(windowOpacity());
    fade->setEndValue(1.0);
    fade->start();

    auto_hide_timer.start(kAutoHideMs);
    update();
}

void NextendoToast::Reposition() {
    if (!main_window) {
        return;
    }
    const int edge_margin = static_cast<int>(kMargin * scale);
    const int top_extra = static_cast<int>(kTopExtra * scale);
    const int shadow_margin = static_cast<int>(kShadowMargin * scale);
    const int card_w = static_cast<int>(kWidth * scale);
    const QPoint win_pos = main_window->mapToGlobal(QPoint(0, 0));
    const int card_right_x = win_pos.x() + main_window->width() - edge_margin;
    const int card_top_y = win_pos.y() + edge_margin + top_extra;
    move(card_right_x - card_w - shadow_margin, card_top_y - shadow_margin);
}

void NextendoToast::HideAnimated() {
    fade->stop();
    fade->setDuration(kFadeMs);
    fade->setStartValue(windowOpacity());
    fade->setEndValue(0.0);
    fade->start();
    // fade->finished() is shared with the fade-IN animation (Show()), so a persistent connection
    // there would also fire -- and hide() -- right as a toast finishes appearing. A plain timer
    // tied to this specific fade-out avoids that.
    QTimer::singleShot(kFadeMs, this, &QWidget::hide);
}

void NextendoToast::mousePressEvent(QMouseEvent* event) {
    QWidget::mousePressEvent(event);
    auto_hide_timer.stop();
    HideAnimated();
}

void NextendoToast::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing |
                           QPainter::SmoothPixmapTransform);

    const auto S = [this](int base) { return static_cast<int>(base * scale); };
    const int radius = S(14);

    const QRect card = rect().adjusted(S(kShadowMargin), S(kShadowMargin), -S(kShadowMargin),
                                       -S(kShadowMargin));

    // Soft shadow: a poor-man's blur via layered, low-alpha rounded rects growing outward.
    painter.setPen(Qt::NoPen);
    for (int i = S(8); i >= S(1); i -= std::max(S(1), 1)) {
        QPainterPath layer;
        layer.addRoundedRect(card.adjusted(-i, -i + S(2), i, i + S(2)), radius + i, radius + i);
        painter.fillPath(layer, QColor(0, 0, 0, 6));
    }

    QPainterPath bg;
    bg.addRoundedRect(card, radius, radius);
    const QColor bg_color =
        UISettings::IsDarkTheme() ? QColor(32, 32, 37, 245) : QColor(250, 250, 252, 245);
    painter.fillPath(bg, bg_color);
    painter.setPen(QPen(UISettings::IsDarkTheme() ? QColor(255, 255, 255, 20) : QColor(0, 0, 0, 18), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(bg);

    const QColor kAccent(100, 149, 237);
    const int avatar_size = S(kAvatarSize);
    const QRect avatar_rect(card.left() + S(14), card.top() + (card.height() - avatar_size) / 2,
                            avatar_size, avatar_size);

    painter.setPen(QPen(kAccent, S(2)));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(avatar_rect.adjusted(-S(2), -S(2), S(2), S(2)));

    painter.save();
    QPainterPath clip;
    clip.addEllipse(avatar_rect);
    painter.setClipPath(clip);
    if (!avatar.isNull()) {
        painter.drawPixmap(avatar_rect, avatar);
    } else {
        painter.fillRect(avatar_rect, kAccent);
        QFont f = QApplication::font();
        f.setBold(true);
        f.setPointSizeF(std::max((f.pointSizeF() + 2) * scale, S(12) * 1.0));
        painter.setFont(f);
        painter.setPen(Qt::white);
        painter.drawText(avatar_rect, Qt::AlignCenter,
                         line1.isEmpty() ? QStringLiteral("?") : line1.left(1).toUpper());
    }
    painter.restore();

    const int text_left = avatar_rect.right() + S(18);
    const int text_width = card.right() - S(14) - text_left;

    QFont name_font = QApplication::font();
    name_font.setBold(true);
    name_font.setPointSizeF(name_font.pointSizeF() * scale);
    QFont sub_font = QApplication::font();
    sub_font.setPointSizeF(std::max((sub_font.pointSizeF() - 1) * scale, 7.0 * scale));

    // One block (name + gap + subtitle), centered as a unit rather than stretched to fill the
    // card -- splitting the card into two rigid half-height boxes left short text looking pulled
    // apart, with a lot of dead space around it.
    const QFontMetrics name_fm(name_font);
    const QFontMetrics sub_fm(sub_font);
    const int line_gap = S(3);
    const int block_h = name_fm.height() + line_gap + sub_fm.height();
    const int block_top = card.top() + (card.height() - block_h) / 2;

    painter.setFont(name_font);
    painter.setPen(UISettings::IsDarkTheme() ? QColor(235, 235, 238) : QColor(20, 20, 26));
    painter.drawText(QRect(text_left, block_top, text_width, name_fm.height()),
                     Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                     name_fm.elidedText(line1, Qt::ElideRight, text_width));

    painter.setFont(sub_font);
    painter.setPen(UISettings::IsDarkTheme() ? QColor(175, 175, 182) : QColor(95, 95, 105));
    painter.drawText(QRect(text_left, block_top + name_fm.height() + line_gap, text_width,
                           sub_fm.height()),
                     Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                     sub_fm.elidedText(line2, Qt::ElideRight, text_width));
}
