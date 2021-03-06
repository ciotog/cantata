/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
 *
 * ----
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "cachesettings.h"
#include "support/localize.h"
#include "context/artistview.h"
#include "context/albumview.h"
#include "context/songview.h"
#include "context/contextwidget.h"
#include "context/wikipediasettings.h"
#include "covers.h"
#include "support/utils.h"
#include "support/messagebox.h"
#include "config.h"
#include "support/thread.h"
#include "settings.h"
#include "widgets/basicitemdelegate.h"
#ifdef ENABLE_STREAMS
#include "models/streamsmodel.h"
#endif
#ifdef ENABLE_ONLINE_SERVICES
#include "online//podcastsearchdialog.h"
#endif
#include "support/squeezedtextlabel.h"
#include "scrobbling/scrobbler.h"
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QGridLayout>
#include <QFileInfo>
#include <QDir>
#include <QList>
#include <QHeaderView>

static const int constMaxRecurseLevel=4;

static void calculate(const QString &d, const QStringList &types, int &items, quint64 &space, int level=0)
{
    if (!d.isEmpty() && level<constMaxRecurseLevel) {
        QDir dir(d);
        if (dir.exists()) {
            QFileInfoList files=dir.entryInfoList(types, QDir::Files|QDir::NoDotAndDotDot);
            foreach (const QFileInfo &file, files) {
                space+=file.size();
            }
            items+=files.count();

            QFileInfoList dirs=dir.entryInfoList(QDir::Dirs|QDir::NoDotAndDotDot);
            foreach (const QFileInfo &subDir, dirs) {
                calculate(subDir.absoluteFilePath(), types, items, space, level+1);
            }
        }
    }
}

static void deleteAll(const QString &d, const QStringList &types, int level=0)
{
    if (!d.isEmpty() && level<constMaxRecurseLevel) {
        QDir dir(d);
        if (dir.exists()) {
            QFileInfoList dirs=dir.entryInfoList(QDir::Dirs|QDir::NoDotAndDotDot);
            foreach (const QFileInfo &subDir, dirs) {
                deleteAll(subDir.absoluteFilePath(), types, level+1);
            }

            QFileInfoList files=dir.entryInfoList(types, QDir::Files|QDir::NoDotAndDotDot);
            foreach (const QFileInfo &file, files) {
                QFile::remove(file.absoluteFilePath());
            }
            if (0!=level) {
                QString dirName=dir.dirName();
                if (!dirName.isEmpty()) {
                    dir.cdUp();
                    dir.rmdir(dirName);
                }
            }
        }
    }
}

CacheItemCounter::CacheItemCounter(const QString &name, const QString &d, const QStringList &t)
    : dir(d)
    , types(t)
{
    thread=new Thread(name+" CacheCleaner");
    moveToThread(thread);
    thread->start();
}

CacheItemCounter::~CacheItemCounter()
{
    if (thread) {
        thread->stop();
    }
}

void CacheItemCounter::getCount()
{
    int items=0;
    quint64 space=0;
    calculate(dir, types, items, space);
    emit count(items, space);
}

void CacheItemCounter::deleteAll()
{
    ::deleteAll(dir, types);
    getCount();
}

CacheItem::CacheItem(const QString &title, const QString &d, const QStringList &t, QTreeWidget *p, Type ty)
    : QTreeWidgetItem(p, QStringList() << title)
    , counter(new CacheItemCounter(title, d, t))
    , empty(true)
    , usedSpace(0)
    , type(ty)
{
    connect(this, SIGNAL(getCount()), counter, SLOT(getCount()), Qt::QueuedConnection);
    connect(this, SIGNAL(deleteAll()), counter, SLOT(deleteAll()), Qt::QueuedConnection);
    connect(counter, SIGNAL(count(int, quint64)), this, SLOT(update(int, quint64)), Qt::QueuedConnection);
    connect(this, SIGNAL(updated()), p, SIGNAL(itemSelectionChanged()));
}

CacheItem::~CacheItem()
{
    delete counter;
}

void CacheItem::update(int itemCount, quint64 space)
{
    setText(1, QString::number(itemCount));
    setText(2, Utils::formatByteSize(space));
    empty=0==itemCount;
    usedSpace=space;
    setStatus();
    emit updated();
}

void CacheItem::setStatus(const QString &str)
{
    QFont f(font(0));

    if (!str.isEmpty()) {
        f.setItalic(true);
        setText(1, str);
        setText(2, str);
    }
    setFont(1, f);
    setFont(2, f);
}

void CacheItem::clean()
{
    setStatus(i18n("Deleting..."));
    emit deleteAll();
    switch (type) {
    case Type_Covers:       Covers::self()->clearNameCache(); break;
    case Type_ScaledCovers: Covers::self()->clearScaleCache(); break;
    default: break;
    }
}

void CacheItem::calculate()
{
    setStatus(i18n("Calculating..."));
    emit getCount();
}

static inline void setResizeMode(QHeaderView *hdr, int idx, QHeaderView::ResizeMode mode)
{
    #if QT_VERSION < 0x050000
    hdr->setResizeMode(idx, mode);
    #else
    hdr->setSectionResizeMode(idx, mode);
    #endif
}

CacheTree::CacheTree(QWidget *parent)
    : QTreeWidget(parent)
    , calculated(false)
{
    setHeaderLabels(QStringList() << i18n("Name") << i18n("Item Count") << i18n("Space Used"));
    setAllColumnsShowFocus(true);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setRootIsDecorated(false);
    setSortingEnabled(false);
    //setSortingEnabled(true);
    //sortByColumn(0, Qt::AscendingOrder);
    setResizeMode(header(), 0, QHeaderView::Stretch);
    setResizeMode(header(), 1, QHeaderView::Stretch);
    setResizeMode(header(), 2, QHeaderView::Stretch);
    header()->setStretchLastSection(true);
    setAlternatingRowColors(false);
    setItemDelegate(new BasicItemDelegate(this));
}

CacheTree::~CacheTree()
{
}

void CacheTree::showEvent(QShowEvent *e)
{
    if (!calculated) {
        for (int i=0; i<topLevelItemCount(); ++i) {
            static_cast<CacheItem *>(topLevelItem(i))->calculate();
        }
        calculated=true;
    }
    QTreeWidget::showEvent(e);
}

class SpaceLabel : public SqueezedTextLabel
{
public:
    SpaceLabel(QWidget *p)
        : SqueezedTextLabel(p)
    {
        QFont f=font();
        f.setItalic(true);
        setFont(f);
        update(i18n("Calculating..."));
    }

    void update(int space) { update(Utils::formatByteSize(space)); }
    void update(const QString &text) { setText(i18n("Total space used: %1", text)); }
};

CacheSettings::CacheSettings(QWidget *parent)
    : QWidget(parent)
{
    int spacing=Utils::layoutSpacing(this);
    QGridLayout *layout=new QGridLayout(this);
    layout->setMargin(0);
    int row=0;
    int col=0;
    QLabel *label=new QLabel(i18n("Cantata caches various pieces of informat (covers, lyrics, etc). Below is a summary of Cantata's "
                                  "current cache usage."), this);
    label->setWordWrap(true);
    layout->addWidget(label, row++, col, 1, 2);
    layout->addItem(new QSpacerItem(spacing, spacing, QSizePolicy::Fixed, QSizePolicy::Fixed), row++, 0);

    tree=new CacheTree(this);
    layout->addWidget(tree, row++, col, 1, 2);

    new CacheItem(i18n("Covers"), Utils::cacheDir(Covers::constCoverDir, false), QStringList() << "*.jpg" << "*.png", tree,
                  CacheItem::Type_Covers);
    new CacheItem(i18n("Scaled Covers"), Utils::cacheDir(Covers::constScaledCoverDir, false), QStringList() << "*.jpg" << "*.png", tree,
                  CacheItem::Type_ScaledCovers);
    new CacheItem(i18n("Backdrops"), Utils::cacheDir(ContextWidget::constCacheDir, false), QStringList() << "*.jpg" << "*.png", tree);
    new CacheItem(i18n("Lyrics"), Utils::cacheDir(SongView::constLyricsDir, false), QStringList() << "*"+SongView::constExtension, tree);
    new CacheItem(i18n("Artist Information"), Utils::cacheDir(ArtistView::constCacheDir, false), QStringList() << "*"+ArtistView::constInfoExt
                  << "*"+ArtistView::constSimilarInfoExt << "*.json.gz" << "*.jpg" << "*.png", tree);
    new CacheItem(i18n("Album Information"), Utils::cacheDir(AlbumView::constCacheDir, false), QStringList() << "*"+AlbumView::constInfoExt << "*.jpg" << "*.png", tree);
    new CacheItem(i18n("Track Information"), Utils::cacheDir(SongView::constCacheDir, false), QStringList() << "*"+AlbumView::constInfoExt, tree);
    #ifdef ENABLE_STREAMS
    new CacheItem(i18n("Stream Listings"), Utils::cacheDir(StreamsModel::constSubDir, false), QStringList() << "*"+StreamsModel::constCacheExt, tree);
    #endif
    #ifdef ENABLE_ONLINE_SERVICES
    new CacheItem(i18n("Podcast Directories"), Utils::cacheDir(PodcastSearchDialog::constCacheDir, false), QStringList() << "*"+PodcastSearchDialog::constExt, tree);
    #endif
    new CacheItem(i18n("Wikipedia Languages"), Utils::cacheDir(WikipediaSettings::constSubDir, false), QStringList() << "*.xml.gz", tree);
    new CacheItem(i18n("Scrobble Tracks"), Utils::cacheDir(Scrobbler::constCacheDir, false), QStringList() << "*.xml.gz", tree);

    for (int i=0; i<tree->topLevelItemCount(); ++i) {
        connect(static_cast<CacheItem *>(tree->topLevelItem(i)), SIGNAL(updated()), this, SLOT(updateSpace()));
    }

    spaceLabel=new SpaceLabel(this);
    button=new QPushButton(i18n("Delete All"), this);
    layout->addWidget(spaceLabel, row, 0, 1, 1);
    layout->addWidget(button, row++, 1, 1, 1);
    button->setEnabled(false);

    connect(tree, SIGNAL(itemSelectionChanged()), this, SLOT(controlButton()));
    connect(button, SIGNAL(clicked()), this, SLOT(deleteAll()));
}

CacheSettings::~CacheSettings()
{
}

void CacheSettings::controlButton()
{
    button->setEnabled(false);
    for (int i=0; i<tree->topLevelItemCount(); ++i) {
        CacheItem *item=static_cast<CacheItem *>(tree->topLevelItem(i));

        if (item->isSelected() && !item->isEmpty()) {
            button->setEnabled(true);
            break;
        }
    }
}

void CacheSettings::deleteAll()
{
    QList<CacheItem *> toDelete;
    for (int i=0; i<tree->topLevelItemCount(); ++i) {
        CacheItem *item=static_cast<CacheItem *>(tree->topLevelItem(i));

        if (item->isSelected() && !item->isEmpty()) {
            toDelete.append(item);
        }
    }

    if (!toDelete.isEmpty()) {
        if (1==toDelete.count()) {
            if (MessageBox::Yes==MessageBox::warningYesNo(this, i18n("Delete all '%1' items?", toDelete.at(0)->name()),
                                                          i18n("Delete Cache Items"), StdGuiItem::del(), StdGuiItem::cancel())) {
                toDelete.first()->clean();
            }
        } else if (toDelete.count()>1) {
            static const QChar constBullet(0x2022);

            QString items;
            foreach (CacheItem *i, toDelete) {
                items+=constBullet+QLatin1Char(' ')+i->name()+QLatin1Char('\n');
            }

            if (MessageBox::No==MessageBox::warningYesNo(this, i18n("Delete items from all selected categories?")+QLatin1String("\n\n")+items,
                                                         i18n("Delete Cache Items"), StdGuiItem::del(), StdGuiItem::cancel())) {
                return;
            }

            foreach (CacheItem *i, toDelete) {
                i->clean();
            }
        }
    }
}

void CacheSettings::updateSpace()
{
    quint64 space=0;
    for (int i=0; i<tree->topLevelItemCount(); ++i) {
        space+=static_cast<CacheItem *>(tree->topLevelItem(i))->spaceUsed();
    }
    spaceLabel->update(space);
}
