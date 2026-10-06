#include "account_view.h"
#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
namespace aeris {
AccountView::AccountView(QWidget *parent) : QListView(parent), delegate_(new AccountDelegate(this)) {
    setObjectName("accounts");
    setItemDelegate(delegate_);
    setFrameShape(QFrame::NoFrame);
    setUniformItemSizes(true);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setMouseTracking(true);
    scrollTimer_.setInterval(40);
    connect(&scrollTimer_, &QTimer::timeout, this, &AccountView::scrollDrag);
    animation_.setDuration(1100);
    animation_.setStartValue(0.0);
    animation_.setEndValue(1.0);
    connect(&animation_, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        delegate_->setFeedback(copied_, v.toReal());
        viewport()->update(visualRect(copied_));
    });
    connect(&animation_, &QVariantAnimation::finished, this, &AccountView::clearFeedback);
}
void AccountView::clearFeedback() {
    animation_.stop();
    auto rect = visualRect(copied_);
    copied_ = QModelIndex{};
    delegate_->setFeedback({}, 0);
    viewport()->update(rect);
}
void AccountView::animateCopy(const QModelIndex &index) {
    clearFeedback();
    copied_ = index;
    delegate_->setFeedback(index, 0);
    animation_.start();
}
bool AccountView::copyFeedbackActive() const {
    return copied_.isValid() && animation_.state() == QAbstractAnimation::Running;
}
AccountLayout AccountView::accountLayout() const {
    return delegate_->layout();
}
void AccountView::setAccountLayout(AccountLayout layout) {
    cancelDrag();
    clearFeedback();
    delegate_->setLayout(layout);
    const bool cards = layout == AccountLayout::Cards;
    setViewMode(cards ? QListView::IconMode : QListView::ListMode);
    setFlow(cards ? QListView::LeftToRight : QListView::TopToBottom);
    setMovement(QListView::Static);
    setWrapping(cards);
    setResizeMode(QListView::Adjust);
    setSpacing(0);
    updateGrid();
    doItemsLayout();
}
void AccountView::updateGrid() {
    if (accountLayout() != AccountLayout::Cards) {
        setGridSize({});
        return;
    }
    const int width = qMax(1, viewport()->width());
    const int columns = qMax(1, width / 270);
    // Leave a rounding pixel: QListView wraps an item that exactly meets the
    // viewport's right edge, otherwise a two-column grid can collapse to one.
    const int cellWidth = qMax(1, (width - 2) / columns);
    delegate_->setCardWidth(cellWidth);
    QStyleOptionViewItem option;
    option.initFrom(this);
    const int height = delegate_->sizeHint(option, {}).height();
    setGridSize({cellWidth, height});
}
void AccountView::resizeEvent(QResizeEvent *event) {
    QListView::resizeEvent(event);
    updateGrid();
}
void AccountView::setReorderingEnabled(bool enabled) {
    reorderingEnabled_ = enabled;
    if (!enabled)
        cancelDrag();
}
void AccountView::mousePressEvent(QMouseEvent *event) {
    cancelDrag();
    suppressRelease_ = false;
    if (reorderingEnabled_ && event->button() == Qt::LeftButton) {
        pressed_ = indexAt(event->position().toPoint());
        pressPosition_ = event->position().toPoint();
    }
    QListView::mousePressEvent(event);
}
void AccountView::mouseMoveEvent(QMouseEvent *event) {
    dragPosition_ = event->position().toPoint();
    if (pressed_.isValid() && event->buttons().testFlag(Qt::LeftButton)) {
        if (!dragging_ &&
            (dragPosition_ - pressPosition_).manhattanLength() >= QApplication::startDragDistance()) {
            dragging_ = true;
            suppressRelease_ = true;
            clearFeedback();
            scrollTimer_.start();
        }
    }
    if (dragging_) {
        viewport()->setCursor(Qt::ClosedHandCursor);
        updateInsertion();
        event->accept();
        return;
    }
    viewport()->setCursor(indexAt(dragPosition_).isValid() ? Qt::PointingHandCursor : Qt::ArrowCursor);
    QListView::mouseMoveEvent(event);
}
void AccountView::mouseReleaseEvent(QMouseEvent *event) {
    const auto source = QModelIndex(pressed_);
    const bool moving = dragging_ && event->button() == Qt::LeftButton &&
                        viewport()->rect().contains(event->position().toPoint());
    const int destination = insertionAt(event->position().toPoint());
    const bool suppressed = suppressRelease_;
    cancelDrag();
    if (moving && source.isValid() && destination != source.row() && destination != source.row() + 1)
        emit moveRequested(source, destination);
    if (suppressed) {
        setState(QAbstractItemView::NoState);
        event->accept();
        return;
    }
    QListView::mouseReleaseEvent(event);
}
void AccountView::cancelDrag() {
    scrollTimer_.stop();
    dragging_ = false;
    pressed_ = QModelIndex{};
    insertion_ = -1;
    viewport()->unsetCursor();
    viewport()->update();
}
void AccountView::keyPressEvent(QKeyEvent *event) {
    if (dragging_ && event->key() == Qt::Key_Escape) {
        cancelDrag();
        event->accept();
        return;
    }
    QListView::keyPressEvent(event);
}
void AccountView::focusOutEvent(QFocusEvent *event) {
    cancelDrag();
    QListView::focusOutEvent(event);
}
int AccountView::insertionAt(QPoint position) const {
    if (!model() || model()->rowCount() == 0)
        return -1;
    const auto target = indexAt(position);
    if (!target.isValid())
        return position.y() < visualRect(model()->index(0, 0)).top() ? 0 : model()->rowCount();
    const auto rect = visualRect(target);
    const bool after = accountLayout() == AccountLayout::Cards ? position.x() >= rect.center().x()
                                                               : position.y() >= rect.center().y();
    return target.row() + int(after);
}
void AccountView::updateInsertion() {
    if (!pressed_.isValid()) {
        cancelDrag();
        return;
    }
    insertion_ = insertionAt(dragPosition_);
    viewport()->update();
}
void AccountView::scrollDrag() {
    if (!dragging_ || !isEnabled()) {
        cancelDrag();
        return;
    }
    const int edge = 28;
    int step = 0;
    if (dragPosition_.y() < edge)
        step = -16;
    else if (dragPosition_.y() >= viewport()->height() - edge)
        step = 16;
    verticalScrollBar()->setValue(verticalScrollBar()->value() + step);
    updateInsertion();
}
void AccountView::paintEvent(QPaintEvent *event) {
    QListView::paintEvent(event);
    if (!dragging_ || insertion_ < 0 || !model())
        return;
    const bool atEnd = insertion_ == model()->rowCount();
    const auto target = model()->index(atEnd ? insertion_ - 1 : insertion_, 0);
    const auto rect = visualRect(target).adjusted(1, 1, -1, -1);
    QPainter painter(viewport());
    painter.setPen(QPen(palette().color(QPalette::Highlight), 3));
    if (accountLayout() == AccountLayout::Cards) {
        const int x = atEnd ? rect.right() : rect.left();
        painter.drawLine(x, rect.top() + 5, x, rect.bottom() - 5);
    } else {
        const int y = atEnd ? rect.bottom() : rect.top();
        painter.drawLine(rect.left() + 5, y, rect.right() - 5, y);
    }
}
} // namespace aeris
