#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QIcon>
#include <QString>
#include <QSettings>
#include <QStandardPaths>
#include <QFileDialog>
#include <QDebug>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCloseEvent>
#include <QDir>
#include <QMenu>

#include <QPushButton>
#include <QMessageBox>

// ──────────────────────────────────────────────────────
// Constructor
// ──────────────────────────────────────────────────────

MainWindow::MainWindow(QWidget *parent, bool agent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , playing(false)
{
    ui->setupUi(this);

    // --- Set notifier ---
    notifier = new Notifier(this);
    ui->btn_notify->setIcon(QIcon::fromTheme(canNotify ? "user-available" : "user-offline"));

    // --- Set agent ---
    this->agent = agent;

    // --- Multimedia objects ---
    player = new QMediaPlayer(this);
    output = new QAudioOutput(this);
    player->setAudioOutput(output);
    ui->s_progress->setEnabled(false);

    // --- Load settings ---
    loadSettings();

    // --- Playlist ---
    playlist = new Playlist(this);

    // --- Playlist ↔ UI connections ---
    connect(playlist, &Playlist::mediaAdded, this, [this](int index) {
        QString display = playlist->begin()[index].displayName;
        QListWidgetItem *item = new QListWidgetItem(display);
        item->setToolTip(playlist->begin()[index].url.toLocalFile());
        ui->lt_playlist->addItem(item);
    });

    connect(playlist, &Playlist::mediaRemoved, this, [this](int index) {
        delete ui->lt_playlist->takeItem(index);
    });

    connect(playlist, &Playlist::currentIndexChanged, this, [this](int index) {
        if (index >= 0)
            ui->lt_playlist->setCurrentRow(index);
        else
            ui->lt_playlist->clearSelection();
    });

    // --- Player → auto-advance on track end ---
    connect(player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::EndOfMedia) {
            next();

            // Notify if visible and enabled
            if (canNotify && this->isMinimized()) {
                notifier->notify(playlist->currentDisplayName(),
                                 playlist->currentArtist());
            }
        }
    });

    // --- Button connections ---
    connect(ui->btn_play, &QPushButton::clicked, this, &MainWindow::play);
    connect(ui->btn_stop, &QPushButton::clicked, this, &MainWindow::stop);
    connect(ui->btn_next, &QPushButton::clicked, this, &MainWindow::next);
    connect(ui->btn_prev, &QPushButton::clicked, this, &MainWindow::previous);
    connect(ui->btn_repeat_mode, &QPushButton::clicked, this, &MainWindow::repeat);
    connect(ui->btn_playlist_add, &QPushButton::clicked, this, &MainWindow::playlist_add);
    connect(ui->btn_playlist_del, &QPushButton::clicked, this, &MainWindow::playlist_del);
    connect(ui->s_volume, &QSlider::valueChanged, this, &MainWindow::set_volume);
    connect(ui->btn_notify, &QPushButton::clicked, this, &MainWindow::toggle_notification);

    // --- Show about
    connect(ui->btn_about, &QPushButton::clicked, this, [this]() {
        if (!about_dialog) {
            about_dialog = new About(this);
            about_dialog->setWindowFlags(Qt::Dialog);
            about_dialog->setAttribute(Qt::WA_DeleteOnClose, true);
        }

        connect(about_dialog, &QObject::destroyed, this, [this]() {
            about_dialog = nullptr;
        });

        about_dialog->show();
        about_dialog->raise();
    });

    // Mute toggle button
    connect(ui->btn_volume, &QPushButton::toggled, this, [this](bool /*checked*/){
        if (ui->btn_volume->isChecked()) {
            ui->btn_volume->setIcon(QIcon::fromTheme("audio-volume-muted"));
            ui->s_volume->setEnabled(false);
            output->setVolume(0.0);
        } else {
            ui->s_volume->setEnabled(true);
            this->set_volume();
        }
    });

    // --- Double‑click on playlist → play selected track ---
    connect(ui->lt_playlist, &QListWidget::doubleClicked, this, [this]() {
        int row = ui->lt_playlist->currentRow();
        if (row >= 0 && row < playlist->size()) {
            playlist->setCurrentIndex(row);
            player->setSource(playlist->currentMedia());
            player->play();
            playing = true;
            ui->s_progress->setEnabled(true);
            updateTrackInfo();
            ui->btn_play->setIcon(QIcon::fromTheme("media-playback-pause"));
        }
    });

    // Progress slider connections
    connect(player, &QMediaPlayer::durationChanged, this, &MainWindow::onDurationChanged);
    connect(player, &QMediaPlayer::positionChanged, this, &MainWindow::onPositionChanged);
    connect(ui->s_progress, &QSlider::sliderPressed, this, &MainWindow::onSliderPressed);
    connect(ui->s_progress, &QSlider::sliderReleased, this, &MainWindow::onSliderReleased);

    // --- Volume initialization ---
    this->set_volume();

    // --- Dark theme ---
    QFile file(":/styles/dark.qss");
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        setStyleSheet(file.readAll());
        file.close();
    }

    // --- Persistent playlist path ---
    // Persistent playlist stored alongside app data (respects XDG)
    QString playlistDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(playlistDir);
    m_playlistFilePath = playlistDir + "/playlist.json";
    loadPlaylist();

    // --- IPC server ---
    const QString socketName = "ArchieProjectCassette";
    m_server = new QLocalServer(this);
    connect(m_server, &QLocalServer::newConnection, this, &MainWindow::onNewConnection);
    QLocalServer::removeServer(socketName);
    if (!m_server->listen(socketName)) {
        qWarning() << "IPC: could not listen on" << socketName << m_server->errorString();
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

// ──────────────────────────────────────────────────────
// Track info label
// ──────────────────────────────────────────────────────

void MainWindow::updateTrackInfo()
{
    if (playlist->isEmpty() || playlist->currentIndex() < 0) {
        ui->lb_info->clear();
        return;
    }
    QString title = playlist->currentTitle();
    QString artist = playlist->currentArtist();
    if (!artist.isEmpty())
        ui->lb_info->setText(title + " – " + artist);
    else
        ui->lb_info->setText(title);
}

// ──────────────────────────────────────────────────────
// Playback controls
// ──────────────────────────────────────────────────────

void MainWindow::play()
{
    if (playlist->isEmpty()) return;

    // If nothing is selected, default to the first track
    if (playlist->currentIndex() < 0)
        playlist->setCurrentIndex(0);

    if (playing) {
        player->pause();
        playing = false;
        ui->btn_play->setIcon(QIcon::fromTheme("media-playback-start"));
    } else {
        // Re‑load source if the player was stopped
        if (player->playbackState() == QMediaPlayer::StoppedState) {
            player->setSource(playlist->currentMedia());
        }
        player->play();
        playing = true;
        ui->s_progress->setEnabled(true);
        updateTrackInfo();
        ui->btn_play->setIcon(QIcon::fromTheme("media-playback-pause"));
    }
}

void MainWindow::stop()
{
    player->stop();
    playing = false;
    ui->s_progress->setEnabled(false);
    ui->lb_info->setText("-");
    ui->btn_play->setIcon(QIcon::fromTheme("media-playback-start"));
}

void MainWindow::next()
{
    QUrl url = playlist->next();
    if (!url.isEmpty()) {
        player->setSource(url);
        player->play();
        playing = true;
        updateTrackInfo();
        ui->btn_play->setIcon(QIcon::fromTheme("media-playback-pause"));
    }
}

void MainWindow::previous()
{
    QUrl url = playlist->previous();
    if (!url.isEmpty()) {
        player->setSource(url);
        player->play();
        playing = true;
        updateTrackInfo();
        ui->btn_play->setIcon(QIcon::fromTheme("media-playback-pause"));
    }
}

// ──────────────────────────────────────────────────────
// Volume
// ──────────────────────────────────────────────────────

void MainWindow::set_volume()
{
    int vol = ui->s_volume->value();
    output->setVolume(vol / 100.0);

    if (vol == 0) {
        ui->btn_volume->setIcon(QIcon::fromTheme("audio-volume-muted"));
    } else if (vol > 0 && vol <= 30) {
        ui->btn_volume->setIcon(QIcon::fromTheme("audio-volume-low"));
    } else if (vol > 40 && vol < 70) {
        ui->btn_volume->setIcon(QIcon::fromTheme("audio-volume-medium"));
    } else if (vol > 70) {
        ui->btn_volume->setIcon(QIcon::fromTheme("audio-volume-high"));
    }
}

// ──────────────────────────────────────────────────────
// Repeat / loop
// ──────────────────────────────────────────────────────

void MainWindow::repeat()
{
    if (ui->btn_repeat_mode->isChecked()) {
        player->setLoops(QMediaPlayer::Infinite);
    } else {
        player->setLoops(1);
    }
}

// ──────────────────────────────────────────────────────
// Playlist management
// ──────────────────────────────────────────────────────

void MainWindow::playlist_add()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this,
        tr("Open Files"),
        QDir::homePath(),
        "Audio (*.mp3 *.ogg *.wav *.flac *.m4a *.aac)"
        );

    for (const QString &track : files) {
        playlist->addMedia(QUrl::fromLocalFile(track));
    }
}

void MainWindow::playlist_del()
{
    int row = ui->lt_playlist->currentRow();
    if (row >= 0) {
        if (row == playlist->currentIndex() && playing) {
            stop(); // stop playback if the currently playing track is removed
        }
        playlist->removeMedia(row);
    }
}

// ──────────────────────────────────────────────────────
// Progress slider
// ──────────────────────────────────────────────────────

void MainWindow::onDurationChanged(qint64 durationMs)
{
    ui->s_progress->setRange(0, static_cast<int>(durationMs));
}

void MainWindow::onPositionChanged(qint64 positionMs)
{
    if (!m_userDraggingSlider) {
        ui->s_progress->setValue(static_cast<int>(positionMs));
    }
}

void MainWindow::onSliderPressed()
{
    m_userDraggingSlider = true;
}

void MainWindow::onSliderReleased()
{
    m_userDraggingSlider = false;
    player->setPosition(ui->s_progress->value());
}

// ──────────────────────────────────────────────────────
// Persistent playlist (JSON)
// ──────────────────────────────────────────────────────

void MainWindow::savePlaylist()
{
    QJsonArray array;
    for (const auto &item : *playlist) {
        QJsonObject obj;
        obj["url"] = item.url.toString(QUrl::None);
        obj["displayName"] = item.displayName;
        obj["title"] = item.title;
        obj["artist"] = item.artist;
        array.append(obj);
    }

    QFile file(m_playlistFilePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(QJsonDocument(array).toJson());
        file.close();
    } else {
        qWarning() << "Could not save playlist to" << m_playlistFilePath;
    }
}

void MainWindow::loadPlaylist()
{
    QFile file(m_playlistFilePath);
    if (!file.exists())
        return;

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Could not read saved playlist.";
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isArray()) {
        qWarning() << "Invalid format in" << m_playlistFilePath;
        return;
    }

    const QJsonArray array = doc.array();
    playlist->clear();
    ui->lt_playlist->clear();

    for (const QJsonValue &val : array) {
        QJsonObject obj = val.toObject();
        QUrl url(obj["url"].toString());
        QString displayName = obj["displayName"].toString();
        QString title = obj["title"].toString();
        QString artist = obj["artist"].toString();

        playlist->addMediaWithMetadata(url, displayName, title, artist);
    }
}

// ──────────────────────────────────────────────────────
// IPC: control from DesktopAgent
// ──────────────────────────────────────────────────────

void MainWindow::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QLocalSocket *client = m_server->nextPendingConnection();
        m_clients.append(client);

        connect(client, &QLocalSocket::readyRead, this, [this, client]() {
            while (client->canReadLine()) {
                QByteArray message = client->readLine().trimmed();
                if (!message.isEmpty())
                    processCommand(message);
            }
        });

        connect(client, &QLocalSocket::disconnected, this, [this, client]() {
            m_clients.removeAll(client);
            client->deleteLater();
        });
    }
}

void MainWindow::processCommand(const QByteArray &message)
{
    qDebug() << "IPC:" << message;

    if (message == "show") {
        this->showNormal();
        this->activateWindow();
        this->raise();
    }
    else if (message == "playpause") {
        play();
    }
    else if (message == "next") {
        next();
    }
    else if (message == "previous") {
        previous();
    }
    else if (message == "close") {
        // Shut down IPC and quit
        if (m_server) {
            m_server->close();
        }
        for (QLocalSocket *client : m_clients) {
            client->disconnectFromServer();
        }

        savePlaylist();
        QApplication::quit();
    }
}

// ──────────────────────────────────────────────────────
// Notifier toggle
// ──────────────────────────────────────────────────────
void MainWindow::toggle_notification() {
    canNotify = !canNotify;
    ui->btn_notify->setIcon(QIcon::fromTheme(canNotify ? "user-available" : "user-offline"));
}

// ──────────────────────────────────────────────────────
// Settings
// ──────────────────────────────────────────────────────

void MainWindow::loadSettings()
{
    QSettings settings;
    canNotify = settings.value("notifications", true).toBool();
    ui->s_volume->setValue(settings.value("volume", 50).toInt());
}

void MainWindow::saveSettings()
{
    QSettings settings;
    settings.setValue("notifications", canNotify);
    settings.setValue("volume", ui->s_volume->value());
}

// ──────────────────────────────────────────────────────
// Window events
// ──────────────────────────────────────────────────────

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange) {
        if (isMinimized()) {
            // Original condition (commented) only hid the window when the tray icon existed.
            // Now we always hide when minimized, regardless of tray icon presence.
            // if (m_trayIcon) {
            if(agent){
                this->hide();
                event->ignore();
                return;
            }
            // }
        }
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    savePlaylist();
    saveSettings();

    // Close IPC server and disconnect clients
    if (m_server) {
        m_server->close();
    }
    for (QLocalSocket *client : m_clients) {
        client->disconnectFromServer();
    }

    event->accept();
    QApplication::quit();
}
