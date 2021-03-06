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
#ifndef DIRVIEWMODEL_H
#define DIRVIEWMODEL_H

#include <QList>
#include <QModelIndex>
#include "dirviewitemroot.h"
#include "actionmodel.h"

class QXmlStreamWriter;

class DirViewModel : public ActionModel
{
    Q_OBJECT

public:
    static DirViewModel * self();

    DirViewModel(QObject *parent = 0);
    ~DirViewModel();
    QModelIndex index(int, int, const QModelIndex & = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &) const;
    QVariant data(const QModelIndex &, int) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
    QStringList filenames(const QModelIndexList &indexes, bool allowPlaylists) const;
    #ifndef ENABLE_UBUNTU
    QMimeData *mimeData(const QModelIndexList &indexes) const;
    #endif
    void load();
    void clear();
    void addFileToList(const QString &file, const QString &mopidyPath);
    void removeFileFromList(const QString &file);
    bool isEnabled() const { return enabled; }
    void setEnabled(bool e);

public Q_SLOTS:
    void updateDirView(DirViewItemRoot *newroot, time_t dbUpdate=0);
    void mpdStatsUpdated();

Q_SIGNALS:
    void updated();
    void loadFolers();

private:
    void toXML(const DirViewItem *item, QXmlStreamWriter &writer);
    time_t fromXML(QIODevice *dev, time_t dt, DirViewItemRoot *root);
    void addFileToList(const QStringList &parts, const QModelIndex &parent, DirViewItemDir *dir, const QString &mopidyPath);
    void removeFileFromList(const QStringList &parts, const QModelIndex &parent, DirViewItemDir *dir);
    void getFiles(DirViewItem *item, QStringList &filenames, bool allowPlaylists) const;

private:
    DirViewItemRoot *rootItem;
    time_t databaseTime;
    bool databaseTimeUnreliable;
    bool enabled;
};

#endif
