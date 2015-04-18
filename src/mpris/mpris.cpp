
#include <QtDBus/QtDBus>

#include "mpris/mediaplayer2.h"
#include "mpris/mediaplayer2player.h"
#include "mpris/mediaplayer2tracklist.h"
#include "mpris/mediaplayer2playlists.h"

#include "mpris.h"


Mpris::Mpris(QObject *parent) 
        : QObject(parent) {
    QDBusConnection connection = QDBusConnection::sessionBus();
    new MediaPlayer2(this);
    new MediaPlayer2Player(this);
    new MediaPlayer2TrackList(this);
    new MediaPlayer2Playlists(this);
    connection.registerObject("/org/mpris/MediaPlayer2", this);
    connection.registerService("org.mpris.mixxx");
    connection.registerService("org.mpris.MediaPlayer2.mixxx");
}

Mpris::~Mpris() {
    QDBusConnection::sessionBus().unregisterService("org.mpris.mixxx");
    QDBusConnection::sessionBus().unregisterService("org.mpris.MediaPlayer2.mixxx");
}
