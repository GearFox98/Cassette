#include "playlist.h"
#include <QFileInfo>
#include <taglib/tag.h>
#include <taglib/fileref.h>

Playlist::Playlist(QObject *parent)
    : QObject{parent}
{}

// Función para leer metadatos y rellenar title, artist y displayName
void Playlist::fetchMetadata(MediaItem &item)
{
    // Por defecto, nombre sin extensión
    QString filePath = item.url.toLocalFile();
    QString baseName = QFileInfo(filePath).completeBaseName();

    TagLib::FileRef f(filePath.toStdString().c_str());
    if (!f.isNull() && f.tag()) {
        TagLib::Tag *tag = f.tag();
        QString title = QString::fromStdString(tag->title().to8Bit(true));
        QString artist = QString::fromStdString(tag->artist().to8Bit(true));

        if (!title.isEmpty()) {
            item.title = title;
        } else {
            item.title = baseName;   // fallback: nombre de archivo sin extensión
        }
        if (!artist.isEmpty()) {
            item.artist = artist;
        } else {
            item.artist.clear();     // si no hay artista, no lo mostramos
        }

        // Construir displayName: "Título - Artista" o solo título/archivo
        if (!item.artist.isEmpty()) {
            item.displayName = item.title + " – " + item.artist;
        } else {
            item.displayName = item.title;   // que ya es baseName si no había título
        }
    } else {
        // No se pudo leer el archivo → usamos baseName sin tags
        item.title = baseName;
        item.artist.clear();
        item.displayName = baseName;
    }
}

void Playlist::addMedia(const QUrl &url)
{
    MediaItem item{url, QString(), QString(), QString()};
    fetchMetadata(item);   // rellena title, artist, displayName
    m_items.append(item);

    // Ajustar índice si era -1 y ahora hay algo
    if (m_currentIndex < 0 && !m_items.isEmpty()) {
        m_currentIndex = 0;
    }
    emit mediaAdded(m_items.size() - 1);
}

void Playlist::addMedia(const QList<QUrl> &urls)
{
    for (const QUrl &url : urls) {
        addMedia(url);
    }
}

void Playlist::removeMedia(int index)
{
    if (index < 0 || index >= m_items.size()) return;
    m_items.removeAt(index);
    fixCurrentIndex();
    emit mediaRemoved(index);
}

QUrl Playlist::currentMedia() const
{
    if (m_currentIndex < 0 || m_currentIndex >= m_items.size())
        return QUrl();
    return m_items.at(m_currentIndex).url;
}

QString Playlist::currentDisplayName() const
{
    if (m_currentIndex < 0 || m_currentIndex >= m_items.size())
        return QString();
    return m_items.at(m_currentIndex).displayName;
}

int Playlist::currentIndex() const
{
    return m_currentIndex;
}

void Playlist::setCurrentIndex(int index)
{
    if (index >= 0 && index < m_items.size() && index != m_currentIndex) {
        m_currentIndex = index;
        emit currentIndexChanged(m_currentIndex);
    }
}

QUrl Playlist::next()
{
    if (m_items.isEmpty()) return QUrl();
    m_currentIndex = (m_currentIndex + 1) % m_items.size();
    emit currentIndexChanged(m_currentIndex);
    return currentMedia();
}

QUrl Playlist::previous()
{
    if (m_items.isEmpty()) return QUrl();
    m_currentIndex = (m_currentIndex - 1 + m_items.size()) % m_items.size();
    emit currentIndexChanged(m_currentIndex);
    return currentMedia();
}

int Playlist::size() const
{
    return m_items.size();
}

bool Playlist::isEmpty() const
{
    return m_items.isEmpty();
}

void Playlist::clear()
{
    m_items.clear();
    m_currentIndex = -1;
    emit cleared();
}

// Iterators
Playlist::iterator Playlist::begin() { return m_items.begin(); }
Playlist::iterator Playlist::end()   { return m_items.end(); }
Playlist::const_iterator Playlist::begin() const { return m_items.begin(); }
Playlist::const_iterator Playlist::end()   const { return m_items.end(); }

void Playlist::fixCurrentIndex()
{
    if (m_items.isEmpty()) {
        m_currentIndex = -1;
    }
    else if (m_currentIndex >= m_items.size()) {
        m_currentIndex = m_items.size() - 1;
    }
    else if (m_currentIndex < 0) {
        m_currentIndex = 0;
    }
}

QString Playlist::currentTitle() const
{
    if (m_currentIndex >= 0 && m_currentIndex < m_items.size())
        return m_items.at(m_currentIndex).title;
    return QString();
}

QString Playlist::currentArtist() const
{
    if (m_currentIndex >= 0 && m_currentIndex < m_items.size())
        return m_items.at(m_currentIndex).artist;
    return QString();
}

void Playlist::addMediaWithMetadata(const QUrl &url, const QString &displayName,
                                    const QString &title, const QString &artist)
{
    MediaItem item{url, displayName, title, artist};
    m_items.append(item);
    if (m_currentIndex < 0 && !m_items.isEmpty())
        m_currentIndex = 0;
    emit mediaAdded(m_items.size() - 1);
}
