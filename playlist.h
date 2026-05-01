#ifndef PLAYLIST_H
#define PLAYLIST_H

#include <QObject>
#include <QList>
#include <QUrl>
#include <QString>

class Playlist : public QObject
{
    Q_OBJECT

public:
    struct MediaItem {
        QUrl url;
        QString displayName;   // "Título - Artista" o nombre de archivo
        QString title;
        QString artist;
    };

    using iterator = QList<MediaItem>::iterator;
    using const_iterator = QList<MediaItem>::const_iterator;

    explicit Playlist(QObject *parent = nullptr);

    void addMedia(const QUrl &url);
    void addMedia(const QList<QUrl> &urls);
    void removeMedia(int index);

    // Metadatos del elemento actual
    QUrl currentMedia() const;
    QString currentDisplayName() const;
    QString currentTitle() const;
    QString currentArtist() const;
    int currentIndex() const;
    void setCurrentIndex(int index);

    QUrl next();
    QUrl previous();

    int size() const;
    bool isEmpty() const;
    void clear();

    // Iteradores
    iterator begin();
    iterator end();
    const_iterator begin() const;
    const_iterator end() const;

    void addMediaWithMetadata(const QUrl &url, const QString &displayName,
                              const QString &title, const QString &artist);

signals:
    void mediaAdded(int index);
    void mediaRemoved(int index);
    void currentIndexChanged(int index);
    void cleared();

private:
    QList<MediaItem> m_items;
    int m_currentIndex = -1;

    void fixCurrentIndex();
    void fetchMetadata(MediaItem &item);
};

#endif // PLAYLIST_H
