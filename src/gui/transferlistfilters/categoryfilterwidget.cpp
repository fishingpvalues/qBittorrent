/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2016-2025  Vladimir Golovnev <glassez@yandex.ru>
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

#include "categoryfilterwidget.h"

#include <QMenu>

#include "base/bittorrent/session.h"
#include "base/global.h"
#include "gui/torrentcategorydialog.h"
#include "gui/uithememanager.h"
#include "gui/utils.h"
#include "categoryfiltermodel.h"
#include "categoryfilterproxymodel.h"

namespace
{
    QString getCategoryFilter(const CategoryFilterProxyModel *const model, const QModelIndex &index)
    {
        QString categoryFilter; // Defaults to All
        if (index.isValid())
        {
            if (!index.parent().isValid() && (index.row() == 1))
                categoryFilter = u""_s; // Uncategorized
            else if (index.parent().isValid() || (index.row() > 1))
                categoryFilter = model->categoryName(index);
        }

        return categoryFilter;
    }
}

CategoryFilterWidget::CategoryFilterWidget(QWidget *parent)
    : FilterTreeView(parent)
{
    auto *proxyModel = new CategoryFilterProxyModel(this);
    proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    proxyModel->setSourceModel(new CategoryFilterModel(this));
    setModel(proxyModel);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setUniformRowHeights(true);
    setHeaderHidden(true);
    setIconSize(Utils::Gui::smallIconSize());
#ifdef Q_OS_MACOS
    setAttribute(Qt::WA_MacShowFocusRect, false);
#endif
    setContextMenuPolicy(Qt::CustomContextMenu);
    sortByColumn(0, Qt::AscendingOrder);
    setCurrentIndex(model()->index(0, 0));

    connect(this, &QTreeView::collapsed, this, &CategoryFilterWidget::callUpdateGeometry);
    connect(this, &QTreeView::expanded, this, &CategoryFilterWidget::callUpdateGeometry);
    connect(this, &QWidget::customContextMenuRequested, this, &CategoryFilterWidget::showMenu);
    initFilter();
    connect(this, &FilterTreeView::filterChanged, this, [this]
    {
        emit categoryFilterChanged(includedCategories(), excludedCategories());
    });
    connect(model(), &QAbstractItemModel::rowsRemoved, this, [this]
    {
        adjustIndentation();
        updateGeometry();
    });
    connect(model(), &QAbstractItemModel::modelReset, this, [this]
    {
        adjustIndentation();
        updateGeometry();
    });

    adjustIndentation();
}

QString CategoryFilterWidget::currentCategory() const
{
    return getCategoryFilter(static_cast<CategoryFilterProxyModel *>(model()), currentIndex());
}

QSet<QString> CategoryFilterWidget::includedCategories() const
{
    const auto *proxyModel = static_cast<CategoryFilterProxyModel *>(model());

    QSet<QString> categories;
    for (const QModelIndex &index : asConst(includedIndexes()))
        categories.insert(getCategoryFilter(proxyModel, index));

    return categories;
}

QSet<QString> CategoryFilterWidget::excludedCategories() const
{
    const auto *proxyModel = static_cast<CategoryFilterProxyModel *>(model());

    QSet<QString> categories;
    for (const QModelIndex &index : asConst(excludedIndexes()))
        categories.insert(getCategoryFilter(proxyModel, index));

    return categories;
}

void CategoryFilterWidget::removeFromFilter(const QString &categoryName)
{
    const auto *proxyModel = static_cast<CategoryFilterProxyModel *>(model());
    const QModelIndex index = categoryName.isEmpty()
        ? model()->index(1, 0)  // Uncategorized
        : proxyModel->index(categoryName);

    setIncluded(index, false);
    setExcluded(index, false);
}

void CategoryFilterWidget::showMenu()
{
    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    addFilterActions(menu, currentIndex());

    menu->addAction(UIThemeManager::instance()->getIcon(u"list-add"_s), tr("Add category...")
        , this, &CategoryFilterWidget::addCategory);

    if (currentIndex().isValid() && !CategoryFilterModel::isSpecialItem(currentIndex()))
    {
        menu->addAction(UIThemeManager::instance()->getIcon(u"list-add"_s), tr("Add subcategory...")
            , this, &CategoryFilterWidget::addSubcategory);
        menu->addAction(UIThemeManager::instance()->getIcon(u"edit-rename"_s, u"document-edit"_s), tr("Edit category...")
            , this, &CategoryFilterWidget::editCategory);
        menu->addAction(UIThemeManager::instance()->getIcon(u"edit-clear"_s, u"list-remove"_s), tr("Remove category")
            , this, &CategoryFilterWidget::removeCategory);
    }

    menu->addAction(UIThemeManager::instance()->getIcon(u"edit-clear"_s, u"list-remove"_s), tr("Remove unused categories")
        , this, &CategoryFilterWidget::removeUnusedCategories);
    menu->addSeparator();
    menu->addAction(UIThemeManager::instance()->getIcon(u"torrent-start"_s, u"media-playback-start"_s), tr("Start torrents")
        , this, &CategoryFilterWidget::actionStartTorrentsTriggered);
    menu->addAction(UIThemeManager::instance()->getIcon(u"torrent-start-forced"_s, u"media-playback-start"_s), tr("Force start torrents")
        , this, &CategoryFilterWidget::actionForceStartTorrentsTriggered);
    menu->addAction(UIThemeManager::instance()->getIcon(u"torrent-stop"_s, u"media-playback-pause"_s), tr("Stop torrents")
        , this, &CategoryFilterWidget::actionStopTorrentsTriggered);
    menu->addAction(UIThemeManager::instance()->getIcon(u"list-remove"_s), tr("Remove torrents")
        , this, &CategoryFilterWidget::actionDeleteTorrentsTriggered);

    menu->popup(QCursor::pos());
}

void CategoryFilterWidget::callUpdateGeometry()
{
    updateGeometry();
}

QSize CategoryFilterWidget::sizeHint() const
{
    // The sizeHint must depend on viewportSizeHint,
    // otherwise widget will not correctly adjust the
    // size when subcategories are used.
    const QSize viewportSize {viewportSizeHint()};
    return
    {
        viewportSize.width(),
        viewportSize.height() + static_cast<int>(0.5 * sizeHintForRow(0))
    };
}

QSize CategoryFilterWidget::minimumSizeHint() const
{
    QSize size = sizeHint();
    size.setWidth(6);
    return size;
}

void CategoryFilterWidget::rowsInserted(const QModelIndex &parent, const int start, const int end)
{
    FilterTreeView::rowsInserted(parent, start, end);

    if (parent.isValid())
        adjustIndentation();

    // Expand all parents if the parent(s) of the node are not expanded.
    QModelIndex p = parent;
    while (p.isValid())
    {
        if (!isExpanded(p))
            expand(p);
        p = model()->parent(p);
    }

    updateGeometry();
}

bool CategoryFilterWidget::hasAnySubcategory() const
{
    const int rowCount = model()->rowCount();
    for (int row = 0; row < rowCount; ++row)
    {
        if (model()->hasChildren(model()->index(row, 0)))
            return true;
    }

    return false;
}

void CategoryFilterWidget::adjustIndentation()
{
    if (hasAnySubcategory())
        resetIndentation();
    else
        setIndentation(0);
}

void CategoryFilterWidget::addCategory()
{
    TorrentCategoryDialog::createCategory(this);
}

void CategoryFilterWidget::addSubcategory()
{
    TorrentCategoryDialog::createCategory(this, currentCategory());
}

void CategoryFilterWidget::editCategory()
{
    TorrentCategoryDialog::editCategory(this, currentCategory());
}

void CategoryFilterWidget::removeCategory()
{
    const QModelIndex index = currentIndex();
    if (index.isValid() && !CategoryFilterModel::isSpecialItem(index))
    {
        BitTorrent::Session::instance()->removeCategory(
                    static_cast<CategoryFilterProxyModel *>(model())->categoryName(index));
        updateGeometry();
    }
}

void CategoryFilterWidget::removeUnusedCategories()
{
    auto *session = BitTorrent::Session::instance();
    for (const QString &category : asConst(session->categories()))
    {
        if (model()->data(static_cast<CategoryFilterProxyModel *>(model())->index(category), Qt::UserRole) == 0)
            session->removeCategory(category);
    }
    updateGeometry();
}
