// SPDX-FileCopyrightText: Copyright 2026 citron Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cmath>
#include <thread>
#include <unordered_map>
#include <utility>

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QCursor>
#include <QEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QListView>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include "common/nextendo_account.h"
#include "citron/nextendo_account_dialog.h"
#include "citron/nextendo_account_page_p.h"
#include "citron/nextendo_avatar_cache.h"
#include "citron/nextendo_controller.h"
#include "citron/nextendo_friend_delegate.h"
#include "citron/nextendo_history_delegate.h"
#include "citron/uisettings.h"

#ifdef ENABLE_WEB_SERVICE
#include "web_service/nextendo_api.h"
#endif

namespace {

constexpr int kHeaderAvatarSize = 72;

QColor CardBg() {
    return UISettings::IsDarkTheme() ? QColor(30, 30, 34) : QColor(244, 244, 248);
}

QColor DimColor() {
    return UISettings::IsDarkTheme() ? QColor(150, 150, 158) : QColor(110, 110, 120);
}

QColor AccentColor() {
    const QString hex = QString::fromStdString(UISettings::values.accent_color.GetValue());
    if (QColor(hex).isValid()) {
        return QColor(hex);
    }
    const QColor pa = QApplication::palette().color(QPalette::Highlight);
    return (pa.isValid() && pa != Qt::black) ? pa : QColor(100, 149, 237);
}

QPixmap RoundedPixmap(const QPixmap& source, int size) {
    QPixmap out(size, size);
    out.fill(Qt::transparent);
    QPainter painter(&out);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath clip;
    clip.addEllipse(0, 0, size, size);
    painter.setClipPath(clip);
    if (source.isNull()) {
        painter.fillRect(0, 0, size, size, QColor(100, 149, 237));
    } else {
        painter.drawPixmap(0, 0, size, size, source);
    }
    return out;
}

class HeaderCard : public QWidget {
public:
    explicit HeaderCard(QWidget* parent) : QWidget(parent) {
        auto* timer = new QTimer(this);
        timer->setInterval(33);
        connect(timer, &QTimer::timeout, this, [this] {
            phase += 0.035;
            update();
        });
        timer->start();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const QRect card = rect().adjusted(1, 1, -1, -1);
        QPainterPath clip_path;
        clip_path.addRoundedRect(card, 12, 12);
        painter.setClipPath(clip_path);
        painter.fillPath(clip_path, CardBg());

        const QColor accent = AccentColor();
        const qreal breathe = 0.5 + 0.5 * std::sin(phase);

        const int glow_w = std::min(100, card.width() / 3);
        QColor edge = accent;
        edge.setAlphaF(0.22 * breathe);
        QColor edge_fade = accent;
        edge_fade.setAlphaF(0.0);

        QLinearGradient left_glow(card.left(), 0, card.left() + glow_w, 0);
        left_glow.setColorAt(0.0, edge);
        left_glow.setColorAt(1.0, edge_fade);
        painter.fillRect(QRect(card.left(), card.top(), glow_w, card.height()), left_glow);

        QLinearGradient right_glow(card.right() - glow_w, 0, card.right(), 0);
        right_glow.setColorAt(0.0, edge_fade);
        right_glow.setColorAt(1.0, edge);
        painter.fillRect(QRect(card.right() - glow_w, card.top(), glow_w, card.height()), right_glow);

        for (int w = 0; w < 2; ++w) {
            const qreal amp = 4.0 + w * 2.5;
            const qreal y_base = card.top() + card.height() * (0.6 + w * 0.18);
            const qreal speed = phase * (1.0 + w * 0.35);

            QPainterPath wave;
            wave.moveTo(card.left(), y_base);
            for (int x = card.left(); x <= card.right(); x += 6) {
                wave.lineTo(x, y_base + amp * std::sin(x * 0.025 + speed));
            }
            QColor wave_color = accent;
            wave_color.setAlphaF(0.06 - w * 0.02);
            painter.strokePath(wave, QPen(wave_color, 1.4));
        }

        painter.setClipping(false);
        painter.setPen(QPen(QColor(255, 255, 255, 20), 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(clip_path);
    }

private:
    qreal phase = 0.0;
};

QListView* MakeCardList(QWidget* parent) {
    auto* view = new QListView(parent);
    view->setSelectionMode(QAbstractItemView::NoSelection);
    view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    view->setFrameShape(QFrame::NoFrame);
    view->setMouseTracking(true);
    view->viewport()->setAttribute(Qt::WA_Hover);
    return view;
}

QLabel* MakeEmptyLabel(const QString& text) {
    auto* label = new QLabel(text);
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    QPalette pal = label->palette();
    pal.setColor(QPalette::WindowText, DimColor());
    label->setPalette(pal);
    return label;
}

QString TabWidgetStyle() {
    const QColor bg = CardBg();
    const QString bg_hex = bg.name();
    return QStringLiteral("QTabWidget::pane { border: none; background: %1; border-radius: 8px; }"
                          "QTabBar::tab { padding: 6px 14px; margin-right: 2px; "
                          "border-top-left-radius: 8px; border-top-right-radius: 8px; }"
                          "QTabBar::tab:selected { background: %1; }")
        .arg(bg_hex);
}

} // Anonymous namespace

NextendoAccountDialog::NextendoAccountDialog(NextendoController* controller_, QWidget* parent)
    : QDialog(parent), controller(controller_) {
    setWindowTitle(tr("Nextendo Account"));
    resize(560, 660);

    auto* header_card = new HeaderCard(this);
    header_avatar = new QLabel;
    header_avatar->setFixedSize(kHeaderAvatarSize, kHeaderAvatarSize);
    header_avatar->setPixmap(RoundedPixmap({}, kHeaderAvatarSize));

    header_name = new QLabel;
    QFont name_font = header_name->font();
    name_font.setPointSize(name_font.pointSize() + 4);
    name_font.setBold(true);
    header_name->setFont(name_font);

    header_code = new QLabel;
    header_code->setCursor(Qt::PointingHandCursor);
    header_code->setToolTip(tr("Click to copy"));
    header_code->installEventFilter(this);
    QFont code_font(QStringLiteral("monospace"));
    header_code->setFont(code_font);
    header_code->setStyleSheet(QStringLiteral("QLabel { padding: 2px 8px; border-radius: 8px; "
                                              "background: rgba(255,255,255,20); }"));

    auto* header_text = new QVBoxLayout;
    header_text->setSpacing(6);
    header_text->addStretch();
    header_text->addWidget(header_name);
    header_text->addWidget(header_code, 0, Qt::AlignLeft);
    header_text->addStretch();

    auto* header_content = new QHBoxLayout;
    header_content->setSpacing(16);
    header_content->addWidget(header_avatar);
    header_content->addLayout(header_text);

    auto* header_row = new QHBoxLayout(header_card);
    header_row->setContentsMargins(16, 14, 16, 14);
    header_row->addStretch();
    header_row->addLayout(header_content);
    header_row->addStretch();

    friend_code_input = new QLineEdit;
    friend_code_input->setPlaceholderText(tr("SW-0000-0000-0000"));
    add_button = new QPushButton(tr("Add Friend"));
    auto* add_row = new QHBoxLayout;
    add_row->addWidget(friend_code_input);
    add_row->addWidget(add_button);

    friend_search = new QLineEdit;
    friend_search->setPlaceholderText(tr("Search friends..."));
    friend_search->setClearButtonEnabled(true);

    friends_view = MakeCardList(this);
    friends_model = new QStandardItemModel(this);
    friends_view->setModel(friends_model);
    friend_delegate = new NextendoFriendDelegate(friends_view, this);
    friends_view->setItemDelegate(friend_delegate);
    connect(friends_view, &QListView::clicked, this, &NextendoAccountDialog::OnFriendsViewClicked);
    connect(friend_search, &QLineEdit::textChanged, this, &NextendoAccountDialog::ApplyFriendFilter);

    auto* friends_page = new QWidget;
    auto* friends_page_layout = new QVBoxLayout(friends_page);
    friends_page_layout->setContentsMargins(0, 0, 0, 0);
    friends_page_layout->setSpacing(8);
    friends_page_layout->addWidget(friend_search);
    friends_page_layout->addWidget(friends_view, 1);

    friends_stack = new QStackedWidget;
    friends_stack->addWidget(friends_page);
    friends_stack->addWidget(MakeEmptyLabel(tr("No friends yet — add one by friend code above.")));

    requests_view = MakeCardList(this);
    requests_model = new QStandardItemModel(this);
    requests_view->setModel(requests_model);
    request_delegate = new NextendoFriendDelegate(requests_view, this);
    requests_view->setItemDelegate(request_delegate);
    connect(requests_view, &QListView::clicked, this, &NextendoAccountDialog::OnFriendsViewClicked);
    requests_stack = new QStackedWidget;
    requests_stack->addWidget(requests_view);
    requests_stack->addWidget(MakeEmptyLabel(tr("No incoming friend requests.")));

    history_view = MakeCardList(this);
    history_model = new QStandardItemModel(this);
    history_view->setModel(history_model);
    history_view->setItemDelegate(new NextendoHistoryDelegate(this));
    history_stack = new QStackedWidget;
    history_stack->addWidget(history_view);
    history_stack->addWidget(MakeEmptyLabel(tr("No games played yet.")));

    auto* tabs = new QTabWidget;
    tabs->setStyleSheet(TabWidgetStyle());
    tabs->addTab(friends_stack, tr("Friends"));
    tabs->addTab(requests_stack, tr("Requests"));
    tabs->addTab(history_stack, tr("Recently Played"));

    status = new QLabel;
    status->setWordWrap(true);

    auto* notifications_toggle = new QCheckBox(tr("Notifications"));
    notifications_toggle->setChecked(UISettings::values.nextendo_notifications_enabled.GetValue());
    connect(notifications_toggle, &QCheckBox::toggled, this,
            [](bool checked) { UISettings::values.nextendo_notifications_enabled.SetValue(checked); });

    auto* notification_corner = new QComboBox;
    notification_corner->addItem(tr("Top Right"));
    notification_corner->addItem(tr("Top Left"));
    notification_corner->addItem(tr("Bottom Right"));
    notification_corner->addItem(tr("Bottom Left"));
    notification_corner->setCurrentIndex(
        std::clamp(UISettings::values.nextendo_notification_corner.GetValue(), 0, 3));
    connect(notification_corner, &QComboBox::currentIndexChanged, this,
            [](int index) { UISettings::values.nextendo_notification_corner.SetValue(index); });

    auto* status_row = new QHBoxLayout;
    status_row->addWidget(status, 1);
    status_row->addWidget(notifications_toggle);
    status_row->addWidget(notification_corner);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(12);
    layout->addWidget(header_card);
    layout->addLayout(add_row);
    layout->addWidget(tabs, 1);
    layout->addLayout(status_row);

    connect(add_button, &QPushButton::clicked, this, &NextendoAccountDialog::OnAdd);
    connect(friend_code_input, &QLineEdit::returnPressed, this, &NextendoAccountDialog::OnAdd);

    header_name->setText(QString::fromStdString(Common::NextendoAccount::GetUsername()));
    header_code->setText(QString::fromStdString(Common::NextendoAccount::GetFriendCode()));

    RefreshFriends();
    RefreshHistory();

    refresh_timer.setInterval(15000);
    connect(&refresh_timer, &QTimer::timeout, this, &NextendoAccountDialog::RefreshFriends);
    refresh_timer.start();

#ifdef ENABLE_WEB_SERVICE
    std::thread{[this] {
        auto profile = WebService::NextendoApi::GetProfile();
        if (!profile.ok || profile.image_base64.empty()) {
            return;
        }
        QMetaObject::invokeMethod(
            this,
            [this, image = profile.image_base64] {
                const QPixmap avatar = Nextendo::AvatarCache::Get("self", image, kHeaderAvatarSize);
                if (!avatar.isNull()) {
                    header_avatar->setPixmap(RoundedPixmap(avatar, kHeaderAvatarSize));
                }
            },
            Qt::QueuedConnection);
    }}.detach();
#endif
}

NextendoAccountDialog::~NextendoAccountDialog() = default;

bool NextendoAccountDialog::eventFilter(QObject* watched, QEvent* event) {
    if (watched == header_code && event->type() == QEvent::MouseButtonRelease) {
        QApplication::clipboard()->setText(header_code->text());
        status->setText(tr("Friend code copied."));
        return true;
    }
    return QDialog::eventFilter(watched, event);
}

void NextendoAccountDialog::SetBusy(bool busy) {
    add_button->setEnabled(!busy);
}

u64 NextendoAccountDialog::SelectedPid(const QModelIndex& index) const {
    return index.isValid() ? index.data(NextendoFriendItem::PidRole).toULongLong() : 0;
}

void NextendoAccountDialog::ApplyFriendFilter(const QString& text) {
    for (int row = 0; row < friends_model->rowCount(); ++row) {
        const QString name =
            friends_model->index(row, 0).data(NextendoFriendItem::NameRole).toString();
        friends_view->setRowHidden(row, !text.isEmpty() && !name.contains(text, Qt::CaseInsensitive));
    }
}

void NextendoAccountDialog::OnFriendsViewClicked(const QModelIndex& index) {
    if (!index.isValid()) {
        return;
    }
    auto* view = qobject_cast<QListView*>(sender());
    auto* delegate = view == requests_view ? request_delegate : friend_delegate;
    const bool is_request = index.data(NextendoFriendItem::IsRequestRole).toBool();

    const QRect cell_rect = view->visualRect(index);
    const QPoint pos = view->viewport()->mapFromGlobal(QCursor::pos());
    const auto hit = delegate->HitTestActions(cell_rect, pos, is_request);
    if (hit == NextendoFriendDelegate::ActionHit::None) {
        return;
    }

    const u64 pid = SelectedPid(index);
    if (pid == 0) {
        return;
    }

#ifdef ENABLE_WEB_SERVICE
    if (is_request) {
        if (hit == NextendoFriendDelegate::ActionHit::Primary) {
            RunAsync([pid] { return WebService::NextendoApi::AcceptFriend(pid); });
        } else {
            RunAsync([pid] { return WebService::NextendoApi::DeclineFriend(pid); });
        }
    } else {
        const QString name = index.data(NextendoFriendItem::NameRole).toString();
        const auto confirm = QMessageBox::question(
            this, tr("Remove Friend"), tr("Remove %1 from your friends list?").arg(name),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (confirm != QMessageBox::Yes) {
            return;
        }
        RunAsync([pid] { return WebService::NextendoApi::RemoveFriend(pid); });
    }
#endif
}

void NextendoAccountDialog::RunAsync(std::function<std::string()> task,
                                     std::function<void()> on_success) {
#ifdef ENABLE_WEB_SERVICE
    SetBusy(true);
    status->setText(tr("Working..."));

    std::thread{[this, work = std::move(task), success_cb = std::move(on_success)] {
        const std::string result = work();

        QMetaObject::invokeMethod(
            this,
            [this, error = result, success_cb] {
                SetBusy(false);
                if (error.empty()) {
                    RefreshFriends();
                    if (controller) {
                        controller->RefreshFriendCache();
                    }
                    if (success_cb) {
                        success_cb();
                    }
                } else {
                    status->setText(QString::fromStdString(error));
                }
            },
            Qt::QueuedConnection);
    }}.detach();
#endif
}

void NextendoAccountDialog::OnAdd() {
#ifdef ENABLE_WEB_SERVICE
    const std::string code = friend_code_input->text().trimmed().toStdString();
    if (code.empty()) {
        status->setText(tr("Enter a friend code first."));
        return;
    }
    friend_code_input->clear();
    RunAsync([code] { return WebService::NextendoApi::AddFriendByCode(code); }, [this, code] {
        if (controller) {
            controller->NotifyFriendRequestSent(QString::fromStdString(code));
        }
    });
#endif
}

void NextendoAccountDialog::RefreshFriends() {
#ifdef ENABLE_WEB_SERVICE
    SetBusy(true);
    status->setText(tr("Loading..."));

    std::thread{[this] {
        auto fetched = WebService::NextendoApi::GetFriends();

        QMetaObject::invokeMethod(
            this,
            [this, list = std::move(fetched)]() mutable {
                SetBusy(false);
                friends_model->clear();
                requests_model->clear();

                if (!list.ok) {
                    status->setText(QString::fromStdString(list.error));
                    return;
                }

                const std::string local_app_id = controller ? controller->GetLocalAppId() : std::string{};
                std::unordered_map<std::string, int> group_size;
                for (const auto& entry : list.friends) {
                    if (entry.presence_status != 0 && !entry.app_id.empty()) {
                        ++group_size[entry.app_id];
                    }
                }
                // Rank: 0 = playing what I'm playing, 1..N = other games (bigger group first),
                // N+1 = online with no game, N+2 = offline. Name breaks ties within a rank.
                const auto rank = [&](const WebService::NextendoApi::Friend& f) -> int {
                    if (f.presence_status == 0) {
                        return static_cast<int>(group_size.size()) + 2;
                    }
                    if (f.app_id.empty()) {
                        return static_cast<int>(group_size.size()) + 1;
                    }
                    if (!local_app_id.empty() && f.app_id == local_app_id) {
                        return 0;
                    }
                    return 1; // refined below by group size, same tier is fine for a stable sort
                };
                std::stable_sort(list.friends.begin(), list.friends.end(),
                                 [&](const WebService::NextendoApi::Friend& a, const WebService::NextendoApi::Friend& b) {
                                     const int ra = rank(a);
                                     const int rb = rank(b);
                                     if (ra != rb) {
                                         return ra < rb;
                                     }
                                     if (ra == 1 && a.app_id != b.app_id) {
                                         return group_size[a.app_id] > group_size[b.app_id];
                                     }
                                     return a.name < b.name;
                                 });

                for (const auto& entry : list.friends) {
                    const QString game =
                        controller ? controller->ResolveGameName(entry.app_id) : QString{};
                    friends_model->appendRow(new NextendoFriendItem(
                        entry.pid, QString::fromStdString(entry.name),
                        QString::fromStdString(entry.friend_code), entry.presence_status, game,
                        QString::fromStdString(entry.image_base64), false));
                }
                for (const auto& entry : list.requests) {
                    requests_model->appendRow(new NextendoFriendItem(
                        entry.pid, QString::fromStdString(entry.name),
                        QString::fromStdString(entry.friend_code), entry.presence_status,
                        QString{}, QString::fromStdString(entry.image_base64), true));
                }

                status->setText(tr("%1 friend(s), %2 request(s).")
                                    .arg(list.friends.size())
                                    .arg(list.requests.size()));

                friends_stack->setCurrentIndex(friends_model->rowCount() > 0 ? 0 : 1);
                requests_stack->setCurrentIndex(requests_model->rowCount() > 0 ? 0 : 1);
                ApplyFriendFilter(friend_search->text());
            },
            Qt::QueuedConnection);
    }}.detach();
#else
    status->setText(tr("This build has no web services support."));
    SetBusy(false);
#endif
}

void NextendoAccountDialog::RefreshHistory() {
#ifdef ENABLE_WEB_SERVICE
    std::thread{[this] {
        auto fetched = WebService::NextendoApi::GetHistory();
        if (!fetched.ok) {
            return;
        }

        QMetaObject::invokeMethod(
            this,
            [this, list = std::move(fetched)] {
                history_model->clear();
                for (const auto& entry : list.entries) {
                    // The server only knows what the client last uploaded, which can be a raw
                    // NCA filename or a missing icon; the locally installed game (it must be
                    // installed, we have it in our own history) has the real name/icon, same
                    // source the game list itself renders from.
                    const QString local_name =
                        controller ? controller->ResolveGameName(entry.title_id) : QString{};
                    const QString local_icon =
                        controller ? controller->ResolveGameIcon(entry.title_id) : QString{};
                    const QString name = (!local_name.isEmpty() && local_name != tr("a game"))
                                             ? local_name
                                             : QString::fromStdString(entry.name);
                    const QString icon =
                        !local_icon.isEmpty() ? local_icon : QString::fromStdString(entry.icon_base64);
                    history_model->appendRow(new NextendoHistoryItem(
                        QString::fromStdString(entry.title_id), name, icon, entry.seconds,
                        QString::fromStdString(entry.last_played)));
                }
                history_stack->setCurrentIndex(history_model->rowCount() > 0 ? 0 : 1);
            },
            Qt::QueuedConnection);
    }}.detach();
#endif
}
