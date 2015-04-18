#include <QApplication>

#include "mediaplayer2.h"

MediaPlayer2::MediaPlayer2(QObject *parent) 
    : QDBusAbstractAdaptor(parent) {
}

MediaPlayer2::~MediaPlayer2() {
}

bool MediaPlayer2::canQuit() const {
    return true;
}

bool MediaPlayer2::fullscreen() const {
    return true;
}

void MediaPlayer2::setFullscreen(bool fullscreen) {
    Q_UNUSED(fullscreen);
}

bool MediaPlayer2::canSetFullscreen() const {
    return true;
}

bool MediaPlayer2::canRaise() const {
    return false;
}

bool MediaPlayer2::hasTrackList() const {
    return false;
}

QString MediaPlayer2::identity() const {
    return "Mixxx";
}

QString MediaPlayer2::desktopEntry() const {
    return "mixxx";
}

QStringList MediaPlayer2::supportedUriSchemes() const {
    QStringList protocols;
    return protocols;
}

QStringList MediaPlayer2::supportedMimeTypes() const {
    QStringList mimeTypes;
    return mimeTypes;
}

void MediaPlayer2::Raise() {
}

void MediaPlayer2::Quit() {
    QApplication::quit();
}


