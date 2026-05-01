#include "notifier.h"

#include <QProcess>
#include <QDebug>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDebug>
#include <QVariantList>
#include <QVariantMap>

Notifier::Notifier(QObject *parent)
    : QObject{parent}
{}

void Notifier::notify(const QString &title, const QString &artist) {
    QString message;

    if(artist.isEmpty()) {
        message = title;
    } else {
        message = QStringLiteral("%1 - %2").arg(title, artist);
    }

    QString appName = QStringLiteral("Cassette");
    uint replacesId = 0;
    QString appIcon = "media-tape";
    QString summary = tr("Now playing...");
    QStringList actions;
    QVariantMap hints;

    // Establecer la categoría como "audio.music" para que el sistema la trate adecuadamente
    hints[QStringLiteral("category")] = QStringLiteral("audio.music");

    // Tiempo de expiración en milisegundos (0 = usar el predeterminado del sistema)
    int expireTimeout = 5000;

    // Construir el mensaje D-Bus
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.Notifications"),   // servicio
        QStringLiteral("/org/freedesktop/Notifications"),  // ruta del objeto
        QStringLiteral("org.freedesktop.Notifications"),   // interfaz
        QStringLiteral("Notify")                           // método
        );

    // Empaquetar argumentos en el orden correcto
    msg << appName
        << replacesId
        << appIcon
        << summary
        << message
        << actions
        << hints
        << expireTimeout;

    QDBusReply<uint> reply = QDBusConnection::sessionBus().call(msg);
}
