#include "accounts.h"
#include "appearance.h"
#include "brands.h"
#include <QApplication>
#include <QFontDatabase>
#include <QPainter>
#include <QPainterPath>
namespace aeris {
struct RowGeometry {
    QRect panel, avatar, title, account, code, countdown;
};
static RowGeometry geometry(const QRect &row, AccountLayout layout, int lineHeight) {
    const bool cards = layout == AccountLayout::Cards;
    const bool compact = layout == AccountLayout::Compact;
    const auto r = cards ? row.adjusted(4, 4, -4, -4) : row.adjusted(0, 2, 0, -2);
    const int size = compact ? 28 : 36;
    QRect avatar(r.left() + 12, cards ? r.top() + 14 : r.center().y() - size / 2, size, size);
    QRect code =
        cards ? QRect(r.left() + 14, r.bottom() - 61, r.width() - 64, 42) : AccountDelegate::codeRect(r);
    const int textLeft = avatar.right() + 12;
    const int textWidth = cards ? r.right() - textLeft - 14 : code.left() - textLeft - 10;
    const int top = cards ? r.top() + 14 : r.center().y() - lineHeight;
    QRect title(textLeft, top, qMax(1, textWidth), lineHeight);
    QRect account(textLeft, top + lineHeight + 1, qMax(1, textWidth), lineHeight);
    const QPoint center(r.right() - 26, cards ? code.center().y() : r.center().y());
    return {r, avatar, title, account, code, QRect(center - QPoint(14, 14), QSize(28, 28))};
}
QRect AccountDelegate::codeRect(const QRect &r) {
    return {r.right() - 226, r.center().y() - 22, 176, 44};
}
QSize AccountDelegate::sizeHint(const QStyleOptionViewItem &o, const QModelIndex &) const {
    if (layout_ == AccountLayout::Cards)
        return {cardWidth_, qMax(144, o.fontMetrics.height() * 2 + 104)};
    const int height = layout_ == AccountLayout::Compact ? 54 : 74;
    return {420, qMax(height, o.fontMetrics.height() * 2 + 18)};
}
void AccountDelegate::setLayout(AccountLayout layout) {
    layout_ = layout;
}
void AccountDelegate::setCardWidth(int width) {
    cardWidth_ = width;
}
AccountLayout AccountDelegate::layout() const {
    return layout_;
}
void AccountDelegate::setFeedback(const QModelIndex &index, qreal progress) {
    copied_ = index;
    progress_ = progress;
}
QModelIndex AccountDelegate::feedbackIndex() const {
    return copied_;
}
static QColor panel(QPainter *p, const QStyleOptionViewItem &o, const QRect &r, bool card) {
    const bool selected = o.state.testFlag(QStyle::State_Selected);
    const bool hover = o.state.testFlag(QStyle::State_MouseOver);
    auto background = o.palette.color(card ? QPalette::Base : QPalette::Window);
    if (selected || hover)
        background = blend(background, o.palette.color(QPalette::Highlight), selected ? .14 : .07);
    p->setPen(Qt::NoPen);
    p->setBrush(background);
    p->drawRoundedRect(r, 8, 8);
    if (o.state.testFlag(QStyle::State_HasFocus)) {
        p->setPen(QPen(o.palette.color(QPalette::Highlight), 1));
        p->setBrush(Qt::NoBrush);
        p->drawRoundedRect(r.adjusted(1, 1, -1, -1), 7, 7);
    }
    return background;
}

static bool brandAvatar(QPainter *p, const QRect &r, const QModelIndex &i) {
    const auto logo = brandPixmap(i.data(AccountModel::BrandIdentifier).toString(), r.width() - 8,
                                  p->device()->devicePixelRatioF());
    if (logo.isNull())
        return false;
    p->setPen(Qt::NoPen);
    p->setBrush(QColor("#e8e8e8"));
    p->drawRoundedRect(r, r.width() * .28, r.width() * .28);
    const QSizeF size = logo.deviceIndependentSize();
    p->drawPixmap(QPointF(r.center()) + QPointF(.5, .5) - QPointF(size.width() / 2, size.height() / 2), logo);
    p->setPen(QPen(QColor("#e8e8e8"), 1));
    p->setBrush(i.data(AccountModel::IconColor).value<QColor>());
    p->drawEllipse(QPointF(r.right() - 2, r.bottom() - 2), 3.5, 3.5);
    return true;
}
static void avatar(QPainter *p, const QRect &r, const QModelIndex &i, QFont font) {
    if (brandAvatar(p, r, i))
        return;
    p->setPen(Qt::NoPen);
    p->setBrush(i.data(AccountModel::IconColor).value<QColor>());
    p->drawRoundedRect(r, r.width() * .28, r.width() * .28);
    p->setPen(Qt::white);
    font.setPixelSize(int(r.height() * .36));
    font.setWeight(QFont::DemiBold);
    p->setFont(font);
    p->drawText(r.adjusted(2, -1, -2, -1), Qt::AlignCenter, i.data(AccountModel::IconLetters).toString());
    const int mark = i.data(AccountModel::IconMark).toInt();
    p->setPen(QPen(QColor(255, 255, 255, 170), 1.6, Qt::SolidLine, Qt::RoundCap));
    p->drawArc(r.adjusted(3, 3, -3, -3), (mark % 4) * 90 * 16, (20 + (mark % 3) * 12) * 16);
}
static void labels(QPainter *p, const QStyleOptionViewItem &o, const QModelIndex &i, const RowGeometry &g) {
    auto font = o.font;
    font.setWeight(QFont::DemiBold);
    p->setFont(font);
    p->setPen(o.palette.color(QPalette::Text));
    auto issuer = i.data(AccountModel::Issuer).toString();
    if (issuer.isEmpty())
        issuer = i.data(AccountModel::Account).toString();
    p->drawText(g.title, Qt::AlignVCenter,
                p->fontMetrics().elidedText(issuer, Qt::ElideRight, g.title.width()));
    font.setWeight(QFont::Normal);
    font.setPointSizeF(qMax(8.0, font.pointSizeF() - 1));
    p->setFont(font);
    p->setPen(o.palette.color(QPalette::PlaceholderText));
    p->drawText(g.account, Qt::AlignVCenter,
                p->fontMetrics().elidedText(i.data(AccountModel::Account).toString(), Qt::ElideRight,
                                            g.account.width()));
}
static QString spacedCode(QString code) {
    if (code.size() % 2 == 0)
        code.insert(code.size() / 2, ' ');
    return code;
}
static void copyGlyph(QPainter *p, QPointF center, bool copied, QColor color, qreal progress) {
    p->save();
    p->translate(center);
    p->setBrush(Qt::NoBrush);
    p->setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (copied) {
        const auto scale = .75 + .25 * qMin(1.0, progress * 6);
        p->scale(scale, scale);
        QPainterPath check;
        check.moveTo(-6, 0);
        check.lineTo(-2, 4);
        check.lineTo(7, -5);
        p->drawPath(check);
    } else {
        p->drawRoundedRect(QRectF(-4, -3, 9, 11), 1.5, 1.5);
        p->drawLine(-7, 4, -7, -6);
        p->drawLine(-7, -6, 2, -6);
    }
    p->restore();
}
static void code(QPainter *p, const QStyleOptionViewItem &o, const QModelIndex &i, const QRect &area,
                 bool copied, qreal progress) {
    const auto success =
        o.palette.color(QPalette::Base).lightness() < 128 ? QColor("#8ce5bc") : QColor("#187049");
    auto color = copied ? success : o.palette.color(QPalette::Text);
    if (copied) {
        auto tint = success;
        tint.setAlphaF(.12 * (1 - progress));
        p->setPen(Qt::NoPen);
        p->setBrush(tint);
        p->drawRoundedRect(area.adjusted(-4, 0, 3, 0), 7, 7);
    }
    const auto text =
        copied ? QStringLiteral("Copied") : spacedCode(i.data(AccountModel::CodeText).toString());
    auto font = copied ? o.font : QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPointSizeF(o.font.pointSizeF() + (copied ? 1 : 5));
    font.setWeight(QFont::DemiBold);
    const auto textArea = area.adjusted(0, 0, -26, 0);
    while (QFontMetrics(font).horizontalAdvance(text) > textArea.width() && font.pointSizeF() > 9)
        font.setPointSizeF(font.pointSizeF() - 1);
    p->setFont(font);
    p->setPen(color);
    p->drawText(textArea, Qt::AlignVCenter | Qt::AlignHCenter, text);
    copyGlyph(p, QPointF(area.right() - 10, area.center().y()), copied,
              copied ? success : o.palette.color(QPalette::PlaceholderText), progress);
}
static void countdown(QPainter *p, const QStyleOptionViewItem &o, const QModelIndex &i, QRect rect) {
    const int remaining = i.data(AccountModel::Remaining).toInt();
    const int period = i.data(AccountModel::Period).toInt();
    p->setBrush(Qt::NoBrush);
    p->setPen(QPen(o.palette.color(QPalette::Mid), 1.5));
    p->drawEllipse(rect);
    p->setPen(QPen(o.palette.color(QPalette::Highlight), 2, Qt::SolidLine, Qt::RoundCap));
    if (period > 0)
        p->drawArc(rect, 90 * 16, -int(5760.0 * remaining / period));
    auto font = o.font;
    font.setPointSizeF(qMax(8.0, font.pointSizeF() - 2));
    p->setFont(font);
    p->setPen(o.palette.color(QPalette::PlaceholderText));
    p->drawText(rect, Qt::AlignCenter, QString::number(remaining));
}
void AccountDelegate::paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &i) const {
    // A transparent stylesheet background can resolve the view's Base to
    // transparent black. Painted account surfaces use the complete theme palette.
    auto themed = option;
    themed.palette = QApplication::palette();
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    const auto g = geometry(themed.rect, layout_, themed.fontMetrics.height());
    const auto background = panel(p, themed, g.panel, layout_ == AccountLayout::Cards);
    themed.palette.setColor(QPalette::Base, background);
    themed.palette.setColor(QPalette::Text, readableText(themed.palette.color(QPalette::Text), background));
    themed.palette.setColor(QPalette::PlaceholderText,
                            readableText(themed.palette.color(QPalette::PlaceholderText), background));
    avatar(p, g.avatar, i, themed.font);
    labels(p, themed, i, g);
    code(p, themed, i, g.code, copied_.isValid() && copied_ == i, progress_);
    countdown(p, themed, i, g.countdown);
    p->restore();
}
} // namespace aeris
