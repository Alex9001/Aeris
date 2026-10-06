#include "accounts.h"
#include "brands.h"
#include "core/error.h"
namespace aeris {
AccountModel::AccountModel(QObject *p, OtpEngine::Clock c) : QAbstractListModel(p), engine_(std::move(c)) {}
int AccountModel::rowCount(const QModelIndex &p) const {
    return p.isValid() ? 0 : int(entries_.size());
}
EntryPtr AccountModel::entry(int row) const {
    if (row < 0 || row >= entries_.size())
        return {};
    return entries_.at(row);
}
bool AccountModel::canMove(int row, int destination) const {
    return row >= 0 && row < entries_.size() && destination >= 0 && destination <= entries_.size() &&
           destination != row && destination != row + 1;
}
Entries AccountModel::reordered(int row, int destination) const {
    if (!canMove(row, destination))
        return {};
    auto result = entries_;
    result.move(row, destination > row ? destination - 1 : destination);
    return result;
}
bool AccountModel::moveRows(const QModelIndex &sourceParent, int row, int count,
                            const QModelIndex &destinationParent, int destination) {
    if (sourceParent.isValid() || destinationParent.isValid() || count != 1 || !canMove(row, destination))
        return false;
    if (!beginMoveRows({}, row, row, {}, destination))
        return false;
    const int target = destination > row ? destination - 1 : destination;
    entries_.move(row, target);
    identities_.move(row, target);
    endMoveRows();
    return true;
}
QVariant AccountModel::staticData(const QModelIndex &i, int role) const {
    const auto &e = entries_.at(i.row());
    const auto &identity = identities_.at(i.row());
    switch (role) {
    case Issuer:
        return e->issuer;
    case Account:
        return e->account;
    case Period:
        return e->period;
    case IconColor:
        return identity.color;
    case IconLetters:
        return identity.letters;
    case BrandIdentifier:
        return identity.brand;
    case IconMark:
        return identity.mark;
    default:
        return e->issuer + " " + e->account;
    }
}
QVariant AccountModel::otpData(const Entry &e, int role) const {
    try {
        auto code = engine_.current(e);
        if (role == CodeText)
            return code.text;
        if (role == Remaining)
            return code.remaining;
        return e.issuer + ", " + e.account + ", " + code.text + ", " + QString::number(code.remaining) +
               " seconds. Press Enter to copy.";
    } catch (const Error &) {
        return {};
    }
}
QVariant AccountModel::data(const QModelIndex &i, int role) const {
    auto e = entry(i.row());
    if (!i.isValid() || !e)
        return {};
    if (QList<int>{CodeText, Remaining, Qt::AccessibleTextRole}.contains(role))
        return otpData(*e, role);
    if (role == Qt::ToolTipRole)
        return e->issuer + "\n" + e->account + "\nClick to copy the current code; drag to reorder";
    if (QList<int>{Issuer, Account, Period, Search, Qt::DisplayRole, IconColor, IconLetters, IconMark,
                   BrandIdentifier}
            .contains(role))
        return staticData(i, role);
    return {};
}
void AccountModel::replace(Entries entries) {
    beginResetModel();
    entries_ = std::move(entries);
    identities_.clear();
    identities_.reserve(entries_.size());
    for (const auto &e : entries_) {
        auto identity = accountIdentity(e->issuer, e->account);
        identity.brand = brandIdentifier(e->issuer, e->type == TokenType::Steam);
        identities_.append(identity);
    }
    endResetModel();
}
} // namespace aeris
