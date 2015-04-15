
#include <QtDBus/QtDBus>
#include "mpris.h"

Mpris::Mpris(QObject *parent) 
        : QObject(parent) {
    QDBusConnection connection = QDBusConnection::sessionBus();
    connection.registerObject("/org/mpris/MediaPlayer2", this);
    connection.registerService("org.mpris.mixxx");
    connection.registerService("org.mpris.MediaPlayer2.mixxx");
}

Mpris::~Mpris() {
    QDBusConnection::sessionBus().unregisterService("org.mpris.mixxx");
    QDBusConnection::sessionBus().unregisterService("org.mpris.MediaPlayer2.mixxx");
}
