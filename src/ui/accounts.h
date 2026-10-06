#pragma once
#include "core/entry.h"
#include "core/otp.h"
#include "identity.h"
#include <QAbstractListModel>
#include <QPersistentModelIndex>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
namespace aeris {
enum class AccountLayout { List, Compact, Cards };
class AccountModel final : public QAbstractListModel {
    Q_OBJECT
  public:
    enum Roles {
        Issuer = Qt::UserRole + 1,
        Account,
        CodeText,
        Remaining,
        Period,
        Search,
        IconColor,
        IconLetters,
        IconMark,
        BrandIdentifier
    };
    explicit AccountModel(QObject *parent = nullptr, OtpEngine::Clock clock = {});
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    void replace(Entries entries);
    EntryPtr entry(int row) const;
    Entries reordered(int row, int destination) const;
    bool moveRows(const QModelIndex &sourceParent, int sourceRow, int count,
                  const QModelIndex &destinationParent, int destinationChild) override;

  private:
    Entries entries_;
    QList<AccountIdentity> identities_;
    OtpEngine engine_;
    bool canMove(int row, int destination) const;
    QVariant staticData(const QModelIndex &index, int role) const;
    QVariant otpData(const Entry &entry, int role) const;
};
class AccountDelegate final : public QStyledItemDelegate {
  public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override;
    static QRect codeRect(const QRect &row);
    void setLayout(AccountLayout layout);
    void setCardWidth(int width);
    AccountLayout layout() const;
    void setFeedback(const QModelIndex &index, qreal progress);
    QModelIndex feedbackIndex() const;

  private:
    AccountLayout layout_ = AccountLayout::List;
    QPersistentModelIndex copied_;
    qreal progress_ = 0;
    int cardWidth_ = 280;
};
} // namespace aeris
