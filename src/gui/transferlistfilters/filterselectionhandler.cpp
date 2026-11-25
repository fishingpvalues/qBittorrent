/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Daniel Fischer
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 */

#include "filterselectionhandler.h"

#include <QAbstractItemView>
#include <QMenu>
#include <QMouseEvent>
#include <QStyledItemDelegate>

#include "base/global.h"

namespace
{
    class FilterItemDelegate final : public QStyledItemDelegate
    {
    public:
        FilterItemDelegate(const FilterSelectionHandler *handler, QObject *parent)
            : QStyledItemDelegate(parent)
            , m_handler {handler}
        {
        }

    protected:
        void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override
        {
            QStyledItemDelegate::initStyleOption(option, index);

            if (!m_handler->isExcluded(index))
                return;

            option->font.setStrikeOut(true);
            option->palette.setBrush(QPalette::Text, option->palette.brush(QPalette::Disabled, QPalette::Text));
        }

    private:
        const FilterSelectionHandler *m_handler = nullptr;
    };
}

FilterSelectionHandler::FilterSelectionHandler(QAbstractItemView *view, IndexPredicate isSingleChoiceIndex)
    : QObject(view)
    , m_view {view}
    , m_isSingleChoiceIndex {std::move(isSingleChoiceIndex)}
{
    m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_view->setItemDelegate(new FilterItemDelegate(this, m_view));
    m_view->viewport()->installEventFilter(this);

    connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged, this, &FilterSelectionHandler::onSelectionChanged);
    // Removed rows invalidate the persistent indexes, so the filter has to be revalidated afterwards
    connect(m_view->model(), &QAbstractItemModel::rowsRemoved, this, &FilterSelectionHandler::validateFilter, Qt::QueuedConnection);
    connect(m_view->model(), &QAbstractItemModel::modelReset, this, &FilterSelectionHandler::validateFilter, Qt::QueuedConnection);
}

QModelIndexList FilterSelectionHandler::includedIndexes() const
{
    QModelIndexList indexes;
    for (const QModelIndex &index : asConst(m_view->selectionModel()->selectedRows()))
    {
        if (!isAllIndex(index) && !isSingleChoiceIndex(index))
            indexes.append(index);
    }

    return indexes;
}

QModelIndexList FilterSelectionHandler::excludedIndexes() const
{
    QModelIndexList indexes;
    indexes.reserve(m_excludedIndexes.size());
    for (const QPersistentModelIndex &index : m_excludedIndexes)
    {
        if (index.isValid())
            indexes.append(index);
    }

    return indexes;
}

QModelIndex FilterSelectionHandler::singleChoiceIndex() const
{
    for (const QModelIndex &index : asConst(m_view->selectionModel()->selectedRows()))
    {
        if (isSingleChoiceIndex(index))
            return index;
    }

    return {};
}

bool FilterSelectionHandler::isIncluded(const QModelIndex &index) const
{
    return index.isValid() && !isAllIndex(index) && !isSingleChoiceIndex(index)
        && m_view->selectionModel()->isRowSelected(index.row(), index.parent());
}

bool FilterSelectionHandler::isExcluded(const QModelIndex &index) const
{
    if (!index.isValid())
        return false;

    const QModelIndex rowIndex = index.siblingAtColumn(0);
    return m_excludedIndexes.contains(QPersistentModelIndex(rowIndex));
}

bool FilterSelectionHandler::isFilterActive() const
{
    return !excludedIndexes().isEmpty() || !includedIndexes().isEmpty() || singleChoiceIndex().isValid();
}

void FilterSelectionHandler::setIncluded(const QModelIndex &index, const bool included)
{
    if (!index.isValid() || isAllIndex(index) || isSingleChoiceIndex(index) || (isIncluded(index) == included))
        return;

    // `onSelectionChanged()` takes care of the rest
    m_view->selectionModel()->select(index, ((included ? QItemSelectionModel::Select : QItemSelectionModel::Deselect) | QItemSelectionModel::Rows));
}

void FilterSelectionHandler::setExcluded(const QModelIndex &index, const bool excluded)
{
    if (!index.isValid() || isAllIndex(index) || isSingleChoiceIndex(index) || (isExcluded(index) == excluded))
        return;

    const QModelIndex rowIndex = index.siblingAtColumn(0);

    m_isUpdatingSelection = true;
    if (excluded)
    {
        m_excludedIndexes.append(QPersistentModelIndex(rowIndex));
        m_view->selectionModel()->select(rowIndex, (QItemSelectionModel::Deselect | QItemSelectionModel::Rows));
        // Exclusions can't be combined with a single choice item
        if (const QModelIndex singleChoice = singleChoiceIndex(); singleChoice.isValid())
            m_view->selectionModel()->select(singleChoice, (QItemSelectionModel::Deselect | QItemSelectionModel::Rows));
        selectAllIfNothingIncluded();
    }
    else
    {
        m_excludedIndexes.removeAll(QPersistentModelIndex(rowIndex));
    }
    m_isUpdatingSelection = false;

    m_view->viewport()->update();
    notifyFilterChanged();
}

void FilterSelectionHandler::setFilter(const QModelIndexList &included, const QModelIndexList &excluded)
{
    m_isUpdatingSelection = true;

    QItemSelectionModel *selectionModel = m_view->selectionModel();

    QItemSelection selection;
    for (const QModelIndex &index : included)
    {
        if (index.isValid() && !isAllIndex(index) && !isSingleChoiceIndex(index))
            selection.select(index, index);
    }
    selectionModel->select(selection, (QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows));

    m_excludedIndexes.clear();
    for (const QModelIndex &index : excluded)
    {
        if (index.isValid() && !isAllIndex(index) && !isSingleChoiceIndex(index) && !isIncluded(index))
            m_excludedIndexes.append(QPersistentModelIndex(index.siblingAtColumn(0)));
    }

    selectAllIfNothingIncluded();
    selectionModel->setCurrentIndex(selectionModel->selectedRows().value(0), QItemSelectionModel::NoUpdate);

    m_isUpdatingSelection = false;

    m_view->viewport()->update();
    notifyFilterChanged();
}

void FilterSelectionHandler::resetFilter()
{
    m_isUpdatingSelection = true;
    m_excludedIndexes.clear();
    const QModelIndex allIndex = m_view->model()->index(0, 0);
    m_view->selectionModel()->setCurrentIndex(allIndex, (QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows));
    m_isUpdatingSelection = false;

    m_view->viewport()->update();
    notifyFilterChanged();
}

void FilterSelectionHandler::addFilterActions(QMenu *menu, const QModelIndex &index)
{
    if (index.isValid() && !isAllIndex(index) && !isSingleChoiceIndex(index))
    {
        const QPersistentModelIndex persistentIndex {index};

        QAction *includeAction = menu->addAction(tr("Include"), this, [this, persistentIndex](const bool checked)
        {
            setIncluded(persistentIndex, checked);
        });
        includeAction->setCheckable(true);
        includeAction->setChecked(isIncluded(index));
        includeAction->setToolTip(tr("Show torrents that match this item. Shortcut: Ctrl+click"));

        QAction *excludeAction = menu->addAction(tr("Exclude"), this, [this, persistentIndex](const bool checked)
        {
            setExcluded(persistentIndex, checked);
        });
        excludeAction->setCheckable(true);
        excludeAction->setChecked(isExcluded(index));
        excludeAction->setToolTip(tr("Hide torrents that match this item. Shortcut: Alt+click"));
    }

    if (isFilterActive())
        menu->addAction(tr("Reset filter"), this, &FilterSelectionHandler::resetFilter);

    menu->setToolTipsVisible(true);
    menu->addSeparator();
}

bool FilterSelectionHandler::eventFilter(QObject *watched, QEvent *event)
{
    if ((watched == m_view->viewport()) && (event->type() == QEvent::MouseButtonPress))
        return handleMousePress(static_cast<QMouseEvent *>(event));

    return QObject::eventFilter(watched, event);
}

// Returns `true` if the event was handled and must not reach the view
bool FilterSelectionHandler::handleMousePress(QMouseEvent *event)
{
    const QPoint pos = event->position().toPoint();
    const QModelIndex index = m_view->indexAt(pos);
    const Qt::KeyboardModifiers modifiers = event->modifiers();

    if (index.isValid() && (event->button() == Qt::LeftButton) && modifiers.testFlag(Qt::AltModifier))
    {
        if (isSingleChoiceIndex(index))
        {
            // Single choice items can only be chosen alone
            m_view->selectionModel()->setCurrentIndex(index, (QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows));
        }
        else
        {
            m_view->selectionModel()->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
            setExcluded(index, !isExcluded(index));
        }
        event->accept();
        return true;
    }

    // Opening the context menu changes the filter only if it is a simple one,
    // so that the actions of the menu affect the torrents of the clicked item.
    if (index.isValid() && (event->button() == Qt::RightButton) && (isExcluded(index) || isMultiFilter()))
    {
        m_view->selectionModel()->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
        event->accept();
        return true;
    }

    const bool isPlainClick = index.isValid() && m_view->visualRect(index).contains(pos)
        && (event->button() == Qt::LeftButton)
        && !modifiers.testAnyFlags(Qt::ControlModifier | Qt::ShiftModifier);
    if (!isPlainClick || m_excludedIndexes.isEmpty())
        return false;

    // A plain click replaces the whole filter
    m_excludedIndexes.clear();
    m_view->viewport()->update();

    // The view handles the click after this function returns. If it doesn't change
    // the selection (e.g. the clicked item is selected already), notify about the change anyway.
    const quint64 revision = m_filterRevision;
    QMetaObject::invokeMethod(this, [this, revision]
    {
        if (revision == m_filterRevision)
            notifyFilterChanged();
    }, Qt::QueuedConnection);
    return false;
}

bool FilterSelectionHandler::isAllIndex(const QModelIndex &index) const
{
    return index.isValid() && !index.parent().isValid() && (index.row() == 0);
}

bool FilterSelectionHandler::isSingleChoiceIndex(const QModelIndex &index) const
{
    return index.isValid() && m_isSingleChoiceIndex && m_isSingleChoiceIndex(index);
}

bool FilterSelectionHandler::isMultiFilter() const
{
    return !m_excludedIndexes.isEmpty() || (includedIndexes().size() > 1);
}

void FilterSelectionHandler::onSelectionChanged(const QItemSelection &selected, [[maybe_unused]] const QItemSelection &deselected)
{
    if (m_isUpdatingSelection)
        return;

    m_isUpdatingSelection = true;

    QItemSelectionModel *selectionModel = m_view->selectionModel();
    const QModelIndex allIndex = m_view->model()->index(0, 0);

    QModelIndex newSingleChoiceIndex;
    for (const QModelIndex &index : asConst(selected.indexes()))
    {
        if (isSingleChoiceIndex(index))
            newSingleChoiceIndex = index;
    }

    if (selected.contains(allIndex) || newSingleChoiceIndex.isValid())
    {
        // "All" or a single choice item was chosen explicitly, so it replaces the filter
        m_excludedIndexes.clear();
        selectionModel->select((newSingleChoiceIndex.isValid() ? newSingleChoiceIndex : allIndex)
            , (QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows));
    }
    else
    {
        // "All" and single choice items can't be combined with other items
        for (const QModelIndex &index : asConst(selectionModel->selectedRows()))
        {
            if ((isAllIndex(index) || isSingleChoiceIndex(index)) && (selectionModel->selectedRows().size() > 1))
                selectionModel->select(index, (QItemSelectionModel::Deselect | QItemSelectionModel::Rows));
        }

        // An item cannot be included and excluded at the same time
        for (const QModelIndex &index : asConst(selected.indexes()))
            m_excludedIndexes.removeAll(QPersistentModelIndex(index.siblingAtColumn(0)));

        selectAllIfNothingIncluded();
    }

    m_isUpdatingSelection = false;

    m_view->viewport()->update();
    notifyFilterChanged();
}

void FilterSelectionHandler::validateFilter()
{
    m_excludedIndexes.removeIf([](const QPersistentModelIndex &index) { return !index.isValid(); });

    m_isUpdatingSelection = true;
    selectAllIfNothingIncluded();
    m_isUpdatingSelection = false;

    notifyFilterChanged();
}

void FilterSelectionHandler::selectAllIfNothingIncluded()
{
    if (!m_view->selectionModel()->hasSelection())
        m_view->selectionModel()->select(m_view->model()->index(0, 0), (QItemSelectionModel::Select | QItemSelectionModel::Rows));
}

void FilterSelectionHandler::notifyFilterChanged()
{
    ++m_filterRevision;
    emit filterChanged();
}
