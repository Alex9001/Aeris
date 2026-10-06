#include "window.h"
#include <QLabel>
namespace aeris {
void Window::reorder(const QModelIndex &source, int destination) {
    if (busy_ || !ready_ || !source.isValid() || source.model() != filter_)
        return;
    if (destination < 0 || destination > filter_->rowCount())
        return;
    const int row = filter_->mapToSource(source).row();
    // Search results insert relative to a visible neighbour; hidden accounts
    // retain their order. Dropping after the last result means after that account.
    const bool atEnd = destination == filter_->rowCount();
    const auto neighbour = filter_->index(atEnd ? destination - 1 : destination, 0);
    const int target = filter_->mapToSource(neighbour).row() + int(atEnd);
    auto entries = model_->reordered(row, target);
    if (entries.isEmpty())
        return;
    auto factory = factory_;
    retryAction_ = [this] { load(); };
    savingOrder_ = true;
    start(
        [factory, entries] {
            TaskResult result;
            result.vault = factory()->replace(entries);
            return result;
        },
        [this, row, target](TaskResult result) { finishReorder(std::move(result), row, target); },
        "Saving account order…");
}
void Window::finishReorder(TaskResult result, int row, int target) {
    list_->setFocus(Qt::OtherFocusReason);
    if (!result.error.isEmpty()) {
        showError("Could not save account order. " + result.error);
        return;
    }
    model_->moveRow({}, row, {}, target);
    const int moved = target > row ? target - 1 : target;
    list_->setCurrentIndex(filter_->mapFromSource(model_->index(moved, 0)));
    list_->scrollTo(list_->currentIndex());
    status_->setText("Order saved · Drag to reorder.");
    if (!result.vault.warning.isEmpty())
        showError(result.vault.warning);
    emit orderSaved();
}
} // namespace aeris
