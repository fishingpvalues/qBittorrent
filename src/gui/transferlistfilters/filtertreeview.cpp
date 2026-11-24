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

#include "filtertreeview.h"

#include <QMenu>
#include <QMouseEvent>
#include <QStyledItemDelegate>

#include "base/global.h"

namespace
{
    class FilterItemDelegate final : public QStyledItemDelegate
    {
    public:
        explicit FilterItemDelegate(FilterTreeView *view)
            : QStyledItemDelegate(view)
            , m_view {view}
        {
        }

    protected:
        void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override
        {
            QStyledItemDelegate::initStyleOption(option, index);

            if (!m_view->isExcluded(index))
                return;

            option->font.setStrikeOut(true);
            option->palette.setBrush(QPalette::Text, option->palette.brush(QPalette::Disabled, QPalette::Text));
        }

    private:
        const FilterTreeView *m_view = nullptr;
    };
}

FilterTreeView::FilterTreeView(QWidget *parent)
    : QTreeView(parent)
{
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setItemDelegate(new FilterItemDelegate(this));
}

void FilterTreeView::initFilter()
{
    connect(selectionModel(), &QItemSelectionModel::selectionChanged, this, &FilterTreeView::onSelectionChanged);
    // Removed rows invalidate the persistent indexes, so the filter has to be revalidated afterwards
    connect(model(), &QAbstractItemModel::rowsRemoved, this, &FilterTreeView::validateFilter, Qt::QueuedConnection);
    connect(model(), &QAbstractItemModel::modelReset, this, &FilterTreeView::validateFilter, Qt::QueuedConnection);
}

QModelIndexList FilterTreeView::includedIndexes() const
{
    QModelIndexList indexes;
    for (const QModelIndex &index : asConst(selectionModel()->selectedRows()))
    {
        if (!isAllIndex(index))
            indexes.append(index);
    }

    return indexes;
}

QModelIndexList FilterTreeView::excludedIndexes() const
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

bool FilterTreeView::isIncluded(const QModelIndex &index) const
{
    return index.isValid() && !isAllIndex(index) && selectionModel()->isRowSelected(index.row(), index.parent());
}

bool FilterTreeView::isExcluded(const QModelIndex &index) const
{
    if (!index.isValid())
        return false;

    const QModelIndex rowIndex = index.siblingAtColumn(0);
    return m_excludedIndexes.contains(QPersistentModelIndex(rowIndex));
}

bool FilterTreeView::isFilterActive() const
{
    return !excludedIndexes().isEmpty() || !includedIndexes().isEmpty();
}

void FilterTreeView::setIncluded(const QModelIndex &index, const bool included)
{
    if (!index.isValid() || isAllIndex(index) || (isIncluded(index) == included))
        return;

    // `onSelectionChanged()` takes care of the rest
    selectionModel()->select(index, ((included ? QItemSelectionModel::Select : QItemSelectionModel::Deselect) | QItemSelectionModel::Rows));
}

void FilterTreeView::setExcluded(const QModelIndex &index, const bool excluded)
{
    if (!index.isValid() || isAllIndex(index) || (isExcluded(index) == excluded))
        return;

    const QModelIndex rowIndex = index.siblingAtColumn(0);

    m_isUpdatingSelection = true;
    if (excluded)
    {
        m_excludedIndexes.append(QPersistentModelIndex(rowIndex));
        selectionModel()->select(rowIndex, (QItemSelectionModel::Deselect | QItemSelectionModel::Rows));
        selectAllIfNothingIncluded();
    }
    else
    {
        m_excludedIndexes.removeAll(QPersistentModelIndex(rowIndex));
    }
    m_isUpdatingSelection = false;

    viewport()->update();
    notifyFilterChanged();
}

void FilterTreeView::resetFilter()
{
    m_isUpdatingSelection = true;
    m_excludedIndexes.clear();
    const QModelIndex allIndex = model()->index(0, 0);
    selectionModel()->setCurrentIndex(allIndex, (QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows));
    m_isUpdatingSelection = false;

    viewport()->update();
    notifyFilterChanged();
}

void FilterTreeView::addFilterActions(QMenu *menu, const QModelIndex &index)
{
    if (index.isValid() && !isAllIndex(index))
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
        menu->addAction(tr("Reset filter"), this, &FilterTreeView::resetFilter);

    menu->setToolTipsVisible(true);
    menu->addSeparator();
}

void FilterTreeView::mousePressEvent(QMouseEvent *event)
{
    const QPoint pos = event->position().toPoint();
    const QModelIndex index = indexAt(pos);
    const Qt::KeyboardModifiers modifiers = event->modifiers();

    if (index.isValid() && (event->button() == Qt::LeftButton) && modifiers.testFlag(Qt::AltModifier))
    {
        selectionModel()->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
        setExcluded(index, !isExcluded(index));
        event->accept();
        return;
    }

    // Opening the context menu changes the filter only if it is a simple one,
    // so that the actions of the menu affect the torrents of the clicked item.
    if (index.isValid() && (event->button() == Qt::RightButton) && (isExcluded(index) || isMultiFilter()))
    {
        selectionModel()->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
        event->accept();
        return;
    }

    const bool isPlainClick = index.isValid() && visualRect(index).contains(pos)
        && (event->button() == Qt::LeftButton)
        && !modifiers.testAnyFlags(Qt::ControlModifier | Qt::ShiftModifier);
    if (!isPlainClick || m_excludedIndexes.isEmpty())
    {
        QTreeView::mousePressEvent(event);
        return;
    }

    // A plain click replaces the whole filter
    m_excludedIndexes.clear();
    viewport()->update();

    const quint64 revision = m_filterRevision;
    QTreeView::mousePressEvent(event);
    if (revision == m_filterRevision)
        notifyFilterChanged();
}

bool FilterTreeView::isAllIndex(const QModelIndex &index) const
{
    return index.isValid() && !index.parent().isValid() && (index.row() == 0);
}

bool FilterTreeView::isMultiFilter() const
{
    return !m_excludedIndexes.isEmpty() || (includedIndexes().size() > 1);
}

void FilterTreeView::onSelectionChanged(const QItemSelection &selected, [[maybe_unused]] const QItemSelection &deselected)
{
    if (m_isUpdatingSelection)
        return;

    m_isUpdatingSelection = true;

    const QModelIndex allIndex = model()->index(0, 0);
    if (selected.contains(allIndex))
    {
        // "All" item was chosen explicitly, so the filter is reset
        m_excludedIndexes.clear();
        selectionModel()->select(allIndex, (QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows));
    }
    else
    {
        if (selectionModel()->isRowSelected(0, {}) && (selectionModel()->selectedRows().size() > 1))
            selectionModel()->select(allIndex, (QItemSelectionModel::Deselect | QItemSelectionModel::Rows));

        // An item cannot be included and excluded at the same time
        for (const QModelIndex &index : asConst(selected.indexes()))
            m_excludedIndexes.removeAll(QPersistentModelIndex(index.siblingAtColumn(0)));

        selectAllIfNothingIncluded();
    }

    m_isUpdatingSelection = false;

    viewport()->update();
    notifyFilterChanged();
}

void FilterTreeView::validateFilter()
{
    m_excludedIndexes.removeIf([](const QPersistentModelIndex &index) { return !index.isValid(); });

    m_isUpdatingSelection = true;
    selectAllIfNothingIncluded();
    m_isUpdatingSelection = false;

    notifyFilterChanged();
}

void FilterTreeView::selectAllIfNothingIncluded()
{
    if (!selectionModel()->hasSelection())
        selectionModel()->select(model()->index(0, 0), (QItemSelectionModel::Select | QItemSelectionModel::Rows));
}

void FilterTreeView::notifyFilterChanged()
{
    ++m_filterRevision;
    emit filterChanged();
}
