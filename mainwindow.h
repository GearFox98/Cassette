#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "playlist.h"
#include "notifier.h"
#include "about.h"

#include <QMainWindow>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QSystemTrayIcon>
#include <QLocalServer>
#include <QLocalSocket>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /**
     * @param no_standalone  If true, the app runs in agent mode:
     *                       hides window on minimize and recieves IPC commands.
     *                       If false, standard standalone mode with no IPC commands provided
     *                       (currently the tray icon is not created – see createTrayIcon).
     */
    explicit MainWindow(QWidget *parent = nullptr, bool agent = false);
    ~MainWindow();

private slots:
    // Playback controls
    void play();
    void stop();
    void next();
    void previous();

    // Volume control
    void set_volume();

    // Repeat toggle
    void repeat();

    // Playlist management
    void playlist_add();
    void playlist_del();

    // Progress slider
    void onDurationChanged(qint64 durationMs);
    void onPositionChanged(qint64 positionMs);
    void onSliderPressed();
    void onSliderReleased();

private:
    Ui::MainWindow *ui;

    // Notifier
    Notifier *notifier;
    bool canNotify = true;
    void toggle_notification();

    // Agent mode
    bool agent;

    // Media
    QMediaPlayer *player;
    QAudioOutput *output;

    Playlist *playlist;

    int m_loopMode = 0;
    enum Mode {
        NORMAL,
        LOOP_LIST,
        LOOP_TRACK
    };

    bool playing = false;
    bool m_userDraggingSlider = false;

    // About - child window
    About *about_dialog = nullptr;

    // Update the track info label with title/artist
    void updateTrackInfo();

    // Persistent playlist storage
    void savePlaylist();
    void loadPlaylist();
    QString m_playlistFilePath;

    // System tray (currently unused in standalone mode)
    //void createTrayIcon();

    QSystemTrayIcon *m_trayIcon = nullptr;

    // IPC server for external control (DesktopAgent)
    QLocalServer *m_server = nullptr;
    QList<QLocalSocket*> m_clients;
    void onNewConnection();
    void processCommand(const QByteArray &message);

    // Settings
    void loadSettings();
    void saveSettings();

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;
};

#endif // MAINWINDOW_H
