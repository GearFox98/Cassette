#include <QApplication>
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QLocalSocket>
#include <QDebug>

#include <QTranslator>
#include <QLocale>

#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName("ArchieProject");
    QApplication::setApplicationName("Cassette");

    // Instance control via D‑Bus
    const QString serviceName = QStringLiteral("org.archieproject.Cassette");
    QDBusConnection bus = QDBusConnection::sessionBus();

    if (!bus.registerService(serviceName)) {
        // Already running – bring to front and quit
        QLocalSocket sock;
        sock.connectToServer(QStringLiteral("ArchieProjectCassette"));
        if (sock.waitForConnected(500)) {
            sock.write("show\n");
            sock.flush();
            sock.disconnectFromServer();
        }
        qDebug() << "Cassette is already running. Brought to foreground.";
        return 0;
    }

    // Parse arguments
    QCommandLineParser parser;
    parser.setApplicationDescription("Cassette audio player for DesktopAgent");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption agentMode(
        QStringList() << "agent",
        "Enables agent mode. Not intended for standalone!"
        );
    parser.addOption(agentMode);
    parser.process(app);

    bool agent = parser.isSet(agentMode);   // true = agent mode

    QTranslator translator;
    if (translator.load(QLocale::system(), "cassette", "_", ":/translations")) {
        app.installTranslator(&translator);
    }

    qDebug() << QLocale::system();

    MainWindow w(nullptr, agent);
    w.show();

    return app.exec();
}
