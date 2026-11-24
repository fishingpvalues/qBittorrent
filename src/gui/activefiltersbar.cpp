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

#include "activefiltersbar.h"

#include <algorithm>

#include <QHBoxLayout>
#include <QLabel>
#include <QList>
#include <QMenu>
#include <QToolButton>

#include "base/global.h"
#include "base/tagset.h"
#include "base/utils/compare.h"
#include "transferlistmodel.h"
#include "transferlistwidget.h"
#include "uithememanager.h"

namespace
{
    // Chips that don't fit are moved to the overflow menu
    const int MAX_VISIBLE_CHIPS = 6;

    QStringList sortedNames(const QSet<QString> &names)
    {
        QStringList list {names.cbegin(), names.cend()};
        std::ranges::sort(list, Utils::Compare::NaturalLessThan<Qt::CaseInsensitive>());
        return list;
    }
}

ActiveFiltersBar::ActiveFiltersBar(TransferListWidget *transferList, QWidget *parent)
    : QWidget(parent)
    , m_transferList {transferList}
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(4);

    auto *titleLabel = new QLabel(tr("Active filters:"), this);
    layout->addWidget(titleLabel);

    m_chipsLayout = new QHBoxLayout;
    m_chipsLayout->setContentsMargins(0, 0, 0, 0);
    m_chipsLayout->setSpacing(4);
    layout->addLayout(m_chipsLayout);
    layout->addStretch();

    m_counterLabel = new QLabel(this);
    layout->addWidget(m_counterLabel);

    auto *resetButton = new QToolButton(this);
    resetButton->setText(tr("Clear filters"));
    resetButton->setToolTip(tr("Remove all category, tag and tracker filters"));
    resetButton->setAutoRaise(true);
    connect(resetButton, &QToolButton::clicked, this, &ActiveFiltersBar::resetRequested);
    layout->addWidget(resetButton);

    connect(m_transferList, &TransferListWidget::categoryOrTagFilterChanged, this, &ActiveFiltersBar::rebuild);
    connect(m_transferList, &TransferListWidget::trackerFilterChanged, this, &ActiveFiltersBar::rebuild);

    const QAbstractItemModel *visibleModel = m_transferList->model();
    connect(visibleModel, &QAbstractItemModel::rowsInserted, this, &ActiveFiltersBar::updateCounter);
    connect(visibleModel, &QAbstractItemModel::rowsRemoved, this, &ActiveFiltersBar::updateCounter);
    connect(visibleModel, &QAbstractItemModel::modelReset, this, &ActiveFiltersBar::updateCounter);
    connect(visibleModel, &QAbstractItemModel::layoutChanged, this, &ActiveFiltersBar::updateCounter);

    rebuild();
}

void ActiveFiltersBar::rebuild()
{
    while (QLayoutItem *item = m_chipsLayout->takeAt(0))
    {
        // The chip may be the sender of the signal that triggered the rebuild
        if (QWidget *widget = item->widget())
        {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }

    QList<Chip> chips;

    const auto addCategoryChips = [this, &chips](const QSet<QString> &categories, const bool isExcluded)
    {
        for (const QString &category : asConst(sortedNames(categories)))
        {
            const QString name = category.isEmpty() ? tr("Uncategorized") : category;
            chips.append({.text = name
                , .toolTip = (isExcluded
                    ? tr("Hiding torrents in category \"%1\". Click to remove this filter.")
                    : tr("Showing torrents in category \"%1\". Click to remove this filter.")).arg(name)
                , .iconID = (isExcluded ? u"list-remove"_s : u"view-categories"_s)
                , .isExcluded = isExcluded
                , .remove = [this, category] { emit categoryRemovalRequested(category); }});
        }
    };

    const auto addTagChips = [this, &chips](const TagSet &tags, const bool isExcluded)
    {
        for (const Tag &tag : tags)
        {
            const QString name = tag.isEmpty() ? tr("Untagged") : tag.toString();
            chips.append({.text = name
                , .toolTip = (isExcluded
                    ? tr("Hiding torrents with tag \"%1\". Click to remove this filter.")
                    : tr("Showing torrents with tag \"%1\". Click to remove this filter.")).arg(name)
                , .iconID = (isExcluded ? u"list-remove"_s : u"tags"_s)
                , .isExcluded = isExcluded
                , .remove = [this, tag] { emit tagRemovalRequested(tag); }});
        }
    };

    const auto addTrackerChips = [this, &chips](const QSet<QString> &trackerHosts, const bool isExcluded)
    {
        for (const QString &trackerHost : asConst(sortedNames(trackerHosts)))
        {
            const QString name = trackerHost.isEmpty() ? tr("Trackerless") : trackerHost;
            chips.append({.text = name
                , .toolTip = (isExcluded
                    ? tr("Hiding torrents with tracker \"%1\". Click to remove this filter.")
                    : tr("Showing torrents with tracker \"%1\". Click to remove this filter.")).arg(name)
                , .iconID = (isExcluded ? u"list-remove"_s : u"trackers"_s)
                , .isExcluded = isExcluded
                , .remove = [this, trackerHost] { emit trackerRemovalRequested(trackerHost); }});
        }
    };

    addCategoryChips(m_transferList->includedCategories(), false);
    addCategoryChips(m_transferList->excludedCategories(), true);
    addTagChips(m_transferList->includedTags(), false);
    addTagChips(m_transferList->excludedTags(), true);
    addTrackerChips(m_transferList->includedTrackerHosts(), false);
    addTrackerChips(m_transferList->excludedTrackerHosts(), true);

    if (const std::optional<BitTorrent::TorrentAnnounceStatus> announceStatus = m_transferList->announceStatusFilter()
            ; announceStatus && *announceStatus)
    {
        QString name;
        if (announceStatus->testFlag(BitTorrent::TorrentAnnounceStatusFlag::HasWarning))
            name = tr("Warning");
        else if (announceStatus->testFlag(BitTorrent::TorrentAnnounceStatusFlag::HasTrackerError))
            name = tr("Tracker error");
        else
            name = tr("Other error");

        chips.append({.text = name
            , .toolTip = tr("Showing torrents with tracker status \"%1\". Click to remove this filter.").arg(name)
            , .iconID = u"trackers"_s
            , .isExcluded = false
            , .remove = [this] { emit announceStatusRemovalRequested(); }});
    }

    setVisible(!chips.isEmpty());
    if (chips.isEmpty())
        return;

    const qsizetype visibleCount = (chips.size() > (MAX_VISIBLE_CHIPS + 1)) ? MAX_VISIBLE_CHIPS : chips.size();
    for (qsizetype i = 0; i < visibleCount; ++i)
        m_chipsLayout->addWidget(createChipButton(chips[i]));

    if (visibleCount < chips.size())
    {
        auto *overflowButton = new QToolButton(this);
        overflowButton->setText(tr("+%1").arg(chips.size() - visibleCount));
        overflowButton->setToolTip(tr("More active filters"));
        overflowButton->setPopupMode(QToolButton::InstantPopup);

        auto *menu = new QMenu(overflowButton);
        for (qsizetype i = visibleCount; i < chips.size(); ++i)
        {
            const Chip &chip = chips[i];
            QAction *action = menu->addAction(UIThemeManager::instance()->getIcon(chip.iconID), chip.text, this, chip.remove);
            action->setToolTip(chip.toolTip);
            if (chip.isExcluded)
            {
                QFont font = action->font();
                font.setStrikeOut(true);
                action->setFont(font);
            }
        }
        menu->setToolTipsVisible(true);
        overflowButton->setMenu(menu);
        m_chipsLayout->addWidget(overflowButton);
    }

    updateCounter();
}

void ActiveFiltersBar::updateCounter()
{
    if (isHidden())
        return;

    const int visibleTorrents = m_transferList->model()->rowCount();
    const int totalTorrents = m_transferList->getSourceModel()->rowCount();
    m_counterLabel->setText(tr("%1 of %2 torrents").arg(QString::number(visibleTorrents), QString::number(totalTorrents)));
}

QToolButton *ActiveFiltersBar::createChipButton(const Chip &chip)
{
    auto *button = new QToolButton(this);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setIcon(UIThemeManager::instance()->getIcon(chip.iconID));
    button->setText(chip.text);
    button->setToolTip(chip.toolTip);
    if (chip.isExcluded)
    {
        QFont font = button->font();
        font.setStrikeOut(true);
        button->setFont(font);
    }
    connect(button, &QToolButton::clicked, this, chip.remove);
    return button;
}
