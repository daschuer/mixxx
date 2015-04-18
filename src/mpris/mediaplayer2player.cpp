#include <QApplication>

#include "mediaplayer2player.h"

MediaPlayer2Player::MediaPlayer2Player(QObject *parent)
    : QDBusAbstractAdaptor(parent) {
}

MediaPlayer2Player::~MediaPlayer2Player() {
}

QString MediaPlayer2Player::playbackStatus() const {
    QString playbackStatus;
    return playbackStatus;
}

QString MediaPlayer2Player::loopStatus() const {
    QString loopStatus;
    return loopStatus;
}

void MediaPlayer2Player::setLoopStatus(const QString& value) {
    Q_UNUSED(value);
}

double MediaPlayer2Player::rate() const {
    double rate = 0.0;
    return rate;
}

void MediaPlayer2Player::setRate(double value) {
    Q_UNUSED(value);
}

bool MediaPlayer2Player::shuffle() const {
    bool shuffle = false;
    return shuffle;
}

void MediaPlayer2Player::setShuffle(bool value) {
    Q_UNUSED(value);
}

QVariantMap MediaPlayer2Player::metadata() const {
    QVariantMap metadata;
    return metadata;
}

double MediaPlayer2Player::volume() const {
    double volume = 0.0;
    return volume;
}

void MediaPlayer2Player::setVolume(double value) {
    Q_UNUSED(value);
}

qlonglong MediaPlayer2Player::position() const {
    qlonglong position = 0;
    return position;
}

double MediaPlayer2Player::minimumRate() const {
    return 0.0;
}

double MediaPlayer2Player::maximumRate() const {
    return 0.0;
}

bool MediaPlayer2Player::canGoNext() const {
    return false;
}

bool MediaPlayer2Player::canGoPrevious() const {
    return false;
}

bool MediaPlayer2Player::canPlay() const {
    return false;
}

bool MediaPlayer2Player::canPause() const {
    return false;
}

bool MediaPlayer2Player::canSeek() const {
    return false;
}

bool MediaPlayer2Player::canControl() const {
    return false;
}

void MediaPlayer2Player::Next() {
}

void MediaPlayer2Player::Previous() {
}

void MediaPlayer2Player::Pause() {
}

void MediaPlayer2Player::PlayPause() {
}

void MediaPlayer2Player::Stop() {
}

void MediaPlayer2Player::Play() {
}

void MediaPlayer2Player::Seek(qlonglong offset) {
    Q_UNUSED(offset);
}

void MediaPlayer2Player::SetPosition(const QDBusObjectPath& trackId,
                                     qlonglong position) {
    Q_UNUSED(trackId);
    Q_UNUSED(position);
}

void MediaPlayer2Player::OpenUri(const QString& uri) {
    Q_UNUSED(uri);
}
