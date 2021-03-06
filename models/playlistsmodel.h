/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
 *
 */
/*
 * Copyright (c) 2008 Sander Knopper (sander AT knopper DOT tk) and
 *                    Roeland Douma (roeland AT rullzer DOT com)
 *
 * This file is part of QtMPC.
 *
 * QtMPC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * QtMPC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QtMPC.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PLAYLISTS_MODEL_H
#define PLAYLISTS_MODEL_H

#include <QIcon>
#include <QAbstractItemModel>
#include <QList>
#include <QMap>
#include "mpd-interface/playlist.h"
#include "mpd-interface/song.h"
#include "actionmodel.h"

class QMenu;
class QAction;

class PlaylistsModel : public ActionModel
{
    Q_OBJECT

public:
    enum Columns
    {
        COL_TITLE,
        COL_ARTIST,
        COL_ALBUM,
        COL_YEAR,
        COL_GENRE,
        COL_LENGTH,
        COL_COMPOSER,
        COL_PERFORMER,

        COL_COUNT
    };

    struct Item
    {
        virtual bool isPlaylist() = 0;
        virtual ~Item() { }
    };

    struct PlaylistItem;
    struct SongItem : public Item, public Song
    {
        SongItem() : parent(0) { }
        SongItem(const Song &s, PlaylistItem *p=0) : Song(s), parent(p) { }
        bool isPlaylist() { return false; }
        PlaylistItem *parent;
    };

    struct PlaylistItem : public Item
    {
        PlaylistItem(quint32 k) : loaded(false), isSmartPlaylist(false), time(0), key(k) { }
        PlaylistItem(const Playlist &pl, quint32 k);
        virtual ~PlaylistItem();
        bool isPlaylist() { return true; }
        SongItem * getSong(const Song &song, int offset);
        void clearSongs();
        quint32 totalTime();
        const QString & visibleName() const { return shortName.isEmpty() ? name : shortName; }
        QString name;
        QString shortName;
        bool loaded;
        bool isSmartPlaylist;
        QList<SongItem *> songs;
        quint32 time;
        quint32 key;
        QDateTime lastModified;
    };

    static PlaylistsModel * self();
    static QString headerText(int col);

    PlaylistsModel(QObject *parent = 0);
    ~PlaylistsModel();
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const { Q_UNUSED(parent) return COL_COUNT; }
    bool canFetchMore(const QModelIndex &index) const;
    void fetchMore(const QModelIndex &index);
    bool hasChildren(const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &index) const;
    QModelIndex index(int row, int col, const QModelIndex &parent) const;
    QVariant data(const QModelIndex &, int) const;
    #ifndef ENABLE_UBUNTU
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    bool setHeaderData(int section, Qt::Orientation orientation, const QVariant &value, int role = Qt::EditRole);
    bool setData(const QModelIndex &index, const QVariant &value, int role);
    #endif
    Qt::ItemFlags flags(const QModelIndex &index) const;
    Qt::DropActions supportedDropActions() const;
    QStringList filenames(const QModelIndexList &indexes, bool filesOnly=false) const;
    QList<Song> songs(const QModelIndexList &indexes) const;
    #ifndef ENABLE_UBUNTU
    QMimeData * mimeData(const QModelIndexList &indexes) const;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int /*col*/, const QModelIndex &parent);
    QStringList mimeTypes() const;
    #endif
    void getPlaylists();
    void clear();
    bool isEnabled() const { return enabled; }
    void setEnabled(bool e);
    bool exists(const QString &n) { return 0!=getPlaylist(n); }
    #ifndef ENABLE_UBUNTU
    QMenu * menu();
    #endif
    static QString strippedText(QString s);
    void setMultiColumn(bool m) { multiCol=m; }

Q_SIGNALS:
    // These are for communicating with MPD object (which is in its own thread, so need to talk via signal/slots)
    void add(const QStringList &files);
    void listPlaylists();
    void playlistInfo(const QString &name) const;
    void addToPlaylist(const QString &name, const QStringList &songs, quint32 pos, quint32 size);
    void moveInPlaylist(const QString &name, const QList<quint32> &idx, quint32 pos, quint32 size);

    void addToNew();
    void addToExisting(const QString &name);
    void updated(const QModelIndex &idx);
    void playlistRemoved(quint32 key);

    // Used in Touch variant only...
    void updated();

private Q_SLOTS:
    void setPlaylists(const QList<Playlist> &playlists);
    void playlistInfoRetrieved(const QString &name, const QList<Song> &songs);
    void removedFromPlaylist(const QString &name, const QList<quint32> &positions);
    void movedInPlaylist(const QString &name, const QList<quint32> &idx, quint32 pos);
    void emitAddToExisting();
    void playlistRenamed(const QString &from, const QString &to);
    void mpdConnectionStateChanged(bool connected);
    void coverLoaded(const Song &song, int s);

private:
    void updateItemMenu(bool craete=false);
    PlaylistItem * getPlaylist(const QString &name);
    void clearPlaylists();
    quint32 allocateKey();

private:
    bool enabled;
    bool multiCol;
    QList<PlaylistItem *> items;
    QSet<quint32> usedKeys;
    #ifndef ENABLE_UBUNTU
    QMenu *itemMenu;
    quint32 dropAdjust;
    QAction *newAction;
    QMap<int, int> alignments;
    #endif
};

#endif
