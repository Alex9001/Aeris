#pragma once
#include "accounts.h"
#include <QListView>
#include <QTimer>
#include <QVariantAnimation>
namespace aeris {
class AccountView final : public QListView {
    Q_OBJECT
  public:
    explicit AccountView(QWidget *parent = nullptr);
    void setAccountLayout(AccountLayout layout);
    AccountLayout accountLayout() const;
    void animateCopy(const QModelIndex &index);
    bool copyFeedbackActive() const;
    void clearFeedback();
    void setReorderingEnabled(bool enabled);

  signals:
    void moveRequested(const QModelIndex &source, int destination);

  protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

  private:
    AccountDelegate *delegate_;
    QVariantAnimation animation_;
    QPersistentModelIndex copied_;
    void updateGrid();
    QPersistentModelIndex pressed_;
    QPoint pressPosition_, dragPosition_;
    bool dragging_ = false, suppressRelease_ = false;
    bool reorderingEnabled_ = true;
    int insertion_ = -1;
    QTimer scrollTimer_;
    void cancelDrag();
    int insertionAt(QPoint position) const;
    void updateInsertion();
    void scrollDrag();
};
} // namespace aeris
