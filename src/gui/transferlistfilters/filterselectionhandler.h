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

#pragma once

#include <functional>

#include <QList>
#include <QObject>
#include <QPersistentModelIndex>

class QAbstractItemView;
class QItemSelection;
class QMenu;
class QMouseEvent;

// Lets a sidebar filter view include and exclude several items at once.
// * Click: filter by the clicked item only
// * Ctrl+click (Cmd+click on macOS): include/stop including the item
// * Alt+click (Option+click on macOS): exclude/stop excluding the item
// The first top level row ("All") is selected when no item is included.
// Single choice items (e.g. tracker statuses) can't be combined with other items.
class FilterSelectionHandler final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(FilterSelectionHandler)

public:
    using IndexPredicate = std::function<bool (const QModelIndex &index)>;

    // Must be created after the model of `view` is set
    explicit FilterSelectionHandler(QAbstractItemView *view, IndexPredicate isSingleChoiceIndex = {});

    QModelIndexList includedIndexes() const;
    QModelIndexList excludedIndexes() const;
    // Returns the selected single choice item, if any
    QModelIndex singleChoiceIndex() const;
    bool isIncluded(const QModelIndex &index) const;
    bool isExcluded(const QModelIndex &index) const;
    bool isFilterActive() const;

    void setIncluded(const QModelIndex &index, bool included);
    void setExcluded(const QModelIndex &index, bool excluded);
    void resetFilter();

    void addFilterActions(QMenu *menu, const QModelIndex &index);

signals:
    void filterChanged();

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    bool handleMousePress(QMouseEvent *event);

    bool isAllIndex(const QModelIndex &index) const;
    bool isSingleChoiceIndex(const QModelIndex &index) const;
    bool isMultiFilter() const;
    void onSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
    void validateFilter();
    void selectAllIfNothingIncluded();
    void notifyFilterChanged();

    QAbstractItemView *m_view = nullptr;
    IndexPredicate m_isSingleChoiceIndex;
    QList<QPersistentModelIndex> m_excludedIndexes;
    bool m_isUpdatingSelection = false;
    quint64 m_filterRevision = 0;
};
