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

#include "musiclibraryitemroot.h"
#include "musiclibraryitemartist.h"
#include "musiclibraryitemalbum.h"
#include "musiclibraryitempodcast.h"
#include "musiclibraryitemsong.h"
#include "onlineservicesmodel.h"
#include "online/onlineservice.h"
#include "widgets/icons.h"
#include "qtiocompressor/qtiocompressor.h"
#include "support/utils.h"
#include "gui/covers.h"
#include "online/rssparser.h"
#include "online/podcastservice.h"
#include <QPixmap>
#include <QFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QCryptographicHash>
#include <QNetworkReply>

static QLatin1String constTopTag("podcast");
static QLatin1String constImageAttribute("img");
static QLatin1String constRssAttribute("rss");
static QLatin1String constEpisodeTag("episode");
static QLatin1String constNameAttribute("name");
static QLatin1String constDateAttribute("date");
static QLatin1String constUrlAttribute("url");
static QLatin1String constTimeAttribute("time");
static QLatin1String constPlayedAttribute("played");
static QLatin1String constLocalAttribute("local");
static QLatin1String constTrue("true");

const QString MusicLibraryItemPodcast::constExt=QLatin1String(".xml.gz");
const QString MusicLibraryItemPodcast::constDir=QLatin1String("podcasts");

static QString generateFileName(const QUrl &url, bool creatingNew)
{
    QString hash=QCryptographicHash::hash(url.toString().toUtf8(), QCryptographicHash::Md5).toHex();
    QString dir=Utils::dataDir(MusicLibraryItemPodcast::constDir, true);
    QString fileName=dir+hash+MusicLibraryItemPodcast::constExt;

    if (creatingNew) {
        int i=0;
        while (QFile::exists(fileName) && i<100) {
            fileName=dir+hash+QChar('_')+QString::number(i)+MusicLibraryItemPodcast::constExt;
            i++;
        }
    }

    return fileName;
}

MusicLibraryItemPodcast::MusicLibraryItemPodcast(const QString &fileName, MusicLibraryItemContainer *parent)
    : MusicLibraryItemContainer(QString(), parent)
    , m_fileName(fileName)
    , m_unplayedEpisodeCount(0)
{
    if (!m_fileName.isEmpty()) {
        m_imageFile=m_fileName;
        m_imageFile=m_imageFile.replace(constExt, ".jpg");
    }
}

bool MusicLibraryItemPodcast::load()
{
    if (m_fileName.isEmpty()) {
        return false;
    }

    QFile file(m_fileName);
    QtIOCompressor compressor(&file);
    compressor.setStreamFormat(QtIOCompressor::GzipFormat);
    if (!compressor.open(QIODevice::ReadOnly)) {
        return false;
    }

    QXmlStreamReader reader(&compressor);
    m_unplayedEpisodeCount=0;
    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.error() && reader.isStartElement()) {
            QString element = reader.name().toString();
            QXmlStreamAttributes attributes=reader.attributes();

            if (constTopTag == element) {
                m_imageUrl=attributes.value(constImageAttribute).toString();
                m_rssUrl=attributes.value(constRssAttribute).toString();
                QString name=attributes.value(constNameAttribute).toString();
                if (m_rssUrl.isEmpty() || name.isEmpty()) {
                    return false;
                }
                m_itemData=name;
            } else if (constEpisodeTag == element) {
                QString name=attributes.value(constNameAttribute).toString();
                QString url=attributes.value(constUrlAttribute).toString();
                if (!name.isEmpty() && !url.isEmpty()) {
                    Song s;
                    s.title=name;
                    s.file=url;
                    s.artist=m_itemData;
                    s.setPlayed(constTrue==attributes.value(constPlayedAttribute).toString());
                    s.setPodcastImage(m_imageFile);
                    s.setPodcastPublishedDate(attributes.value(constDateAttribute).toString());
                    QString time=attributes.value(constTimeAttribute).toString();
                    s.time=time.isEmpty() ? 0 : time.toUInt();
                    QString localFile=attributes.value(constLocalAttribute).toString();
                    if (QFile::exists(localFile)) {
                        s.setPodcastLocalPath(localFile);
                    }
                    MusicLibraryItemPodcastEpisode *song=new MusicLibraryItemPodcastEpisode(s, this);
                    m_childItems.append(song);
                    if (!s.hasBeenPlayed()) {
                        m_unplayedEpisodeCount++;
                    }
                }
            }
        }
    }

    return true;
}

static const QString constRssTag=QLatin1String("rss");

MusicLibraryItemPodcast::RssStatus MusicLibraryItemPodcast::loadRss(QNetworkReply *dev)
{
    m_rssUrl=dev->url();
    if (m_fileName.isEmpty()) {
        m_fileName=m_imageFile=generateFileName(m_rssUrl, true);
        m_imageFile=m_imageFile.replace(constExt, ".jpg");
    }

    RssParser::Channel ch=RssParser::parse(dev);

    if (!ch.isValid()) {
        return FailedToParse;
    }

    if (ch.video) {
        return VideoPodcast;
    }

    m_imageUrl=ch.image;
    m_itemData=ch.name;

    m_unplayedEpisodeCount=ch.episodes.count();
    foreach (const RssParser::Episode &ep, ch.episodes) {
        Song s;
        s.title=ep.name;
        s.file=ep.url.toString(); // ????
        s.artist=m_itemData;
        s.time=ep.duration;
        s.setPlayed(false);
        s.setPodcastImage(m_imageFile);
        s.setPodcastPublishedDate(ep.publicationDate.toString(Qt::ISODate));
        MusicLibraryItemSong *song=new MusicLibraryItemPodcastEpisode(s, this);
        m_childItems.append(song);
    }

    return Loaded;
}

bool MusicLibraryItemPodcast::save()
{
    if (m_fileName.isEmpty()) {
        return false;
    }

    QFile file(m_fileName);
    QtIOCompressor compressor(&file);
    compressor.setStreamFormat(QtIOCompressor::GzipFormat);
    if (!compressor.open(QIODevice::WriteOnly)) {
        return false;
    }

    QXmlStreamWriter writer(&compressor);
    writer.writeStartElement(constTopTag);
    writer.writeAttribute(constImageAttribute, m_imageUrl.toString()); // ??
    writer.writeAttribute(constRssAttribute, m_rssUrl.toString()); // ??
    writer.writeAttribute(constNameAttribute, m_itemData);
    foreach (MusicLibraryItem *i, m_childItems) {
        MusicLibraryItemPodcastEpisode *episode=static_cast<MusicLibraryItemPodcastEpisode *>(i);
        const Song &s=episode->song();
        writer.writeStartElement(constEpisodeTag);
        writer.writeAttribute(constNameAttribute, s.title);
        writer.writeAttribute(constUrlAttribute, s.file);
        if (s.time) {
            writer.writeAttribute(constTimeAttribute, QString::number(s.time));
        }
        if (s.hasBeenPlayed()) {
            writer.writeAttribute(constPlayedAttribute, constTrue);
        }
        if (!s.podcastPublishedDate().isEmpty()) {
            writer.writeAttribute(constDateAttribute, s.podcastPublishedDate());
        }
        if (!s.podcastLocalPath().isEmpty()) {
            writer.writeAttribute(constLocalAttribute, s.podcastLocalPath());
        }
        writer.writeEndElement();
    }
    writer.writeEndElement();
    compressor.close();
    return true;
}

Song MusicLibraryItemPodcast::coverSong() const
{
    Song song;
    if (childCount()) {
        song.artist=PodcastService::constName;
        song.album=data();
        song.title=data();
        song.type=Song::OnlineSvrTrack;
        song.setIsFromOnlineService(PodcastService::constName);
        song.setExtraField(Song::OnlineImageUrl, m_imageUrl.toString());
        song.setExtraField(Song::OnlineImageCacheName, m_imageFile);
        song.file=m_rssUrl.toString();
    }
    return song;
}

void MusicLibraryItemPodcast::remove(int row)
{
    delete m_childItems.takeAt(row);
    resetRows();
}

void MusicLibraryItemPodcast::remove(MusicLibraryItemSong *i)
{
    int idx=m_childItems.indexOf(i);
    if (-1!=idx) {
        remove(idx);
    }
}

void MusicLibraryItemPodcast::removeFiles()
{
    if (!m_fileName.isEmpty() && QFile::exists(m_fileName)) {
        QFile::remove(m_fileName);
    }
    if (!m_imageFile.isEmpty() && QFile::exists(m_imageFile)) {
        QFile::remove(m_imageFile);
    }
}

void MusicLibraryItemPodcast::setUnplayedCount()
{
    m_unplayedEpisodeCount=childCount();
    foreach (MusicLibraryItem *i, m_childItems) {
        if (static_cast<MusicLibraryItemSong *>(i)->song().hasBeenPlayed()) {
            m_unplayedEpisodeCount--;
        }
    }
}

void MusicLibraryItemPodcast::setPlayed(MusicLibraryItemSong *song, bool played)
{
    if (played!=song->song().hasBeenPlayed()) {
        song->setPlayed(played);
        if (played) {
            m_unplayedEpisodeCount--;
        } else {
            m_unplayedEpisodeCount++;
        }
    }
}

void MusicLibraryItemPodcast::addAll(const QList<MusicLibraryItemPodcastEpisode *> &others)
{
    foreach (MusicLibraryItemPodcastEpisode *i, others) {
        static_cast<MusicLibraryItemSong *>(i)->setPodcastImage(m_imageFile);
        i->setParent(this);
    }
}

MusicLibraryItemPodcastEpisode * MusicLibraryItemPodcast::getEpisode(const QString &file) const
{
    foreach (MusicLibraryItem *i, m_childItems) {
        if (static_cast<MusicLibraryItemSong *>(i)->file()==file) {
            return static_cast<MusicLibraryItemPodcastEpisode *>(i);
        }
    }
    return 0;
}

const QString & MusicLibraryItemPodcastEpisode::published()
{
    if (publishedDate.isEmpty()) {
        QDateTime dt=QDateTime::fromString(song().podcastPublishedDate(), Qt::ISODate);
        publishedDate=dt.toString(Qt::LocalDate);
    }
    return publishedDate;
}
