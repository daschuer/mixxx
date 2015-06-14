#include <QApplication>
#include <QDateTime>

#include <math.h>

#include "playerinfo.h"
#include "track/beats.h"
#include "mediaplayer2tracklist.h"

MediaPlayer2TrackList::MediaPlayer2TrackList(QObject* parent)
    : QDBusAbstractAdaptor(parent) {
}

MediaPlayer2TrackList::~MediaPlayer2TrackList() {
}

TrackIds MediaPlayer2TrackList::tracks() const {
    TrackIds tracks;
    return tracks;
}

bool MediaPlayer2TrackList::canEditTracks() const {
    return false;
}

TrackMetadata MediaPlayer2TrackList::GetTracksMetadata(
        const TrackIds& tracks) const {
    Q_UNUSED(tracks);

    TrackMetadata metadata;

    TrackPointer pTrack;
    pTrack = PlayerInfo::instance().getCurrentPlayingTrack();
    if (!pTrack.isNull()) {

        QVariantMap trackInfo;

        // String: The album name.
        trackInfo["xesam:album"] = pTrack->getAlbum();

        //List of Strings: The album artist(s)
        trackInfo["xesam:albumArtist"] = pTrack->getAlbumArtist();

        // List of Strings: The track artist(s).
        trackInfo["xesam:artist"] = pTrack->getArtist();

        // String: The track lyrics.
        //trackInfo["xesam:asText"] = pTrack->

        //Integer: The speed of the music, in beats per minute.
        double dBpm = pTrack->getBpm();
        BeatsPointer pBeats = pTrack->getBeats();
        if (pBeats) {
            dBpm = pBeats->getBpm();
        }
        trackInfo["xesam:audioBPM"] = (int)round(dBpm);

        // "xesam:autoRating" from the standard
        // Float: An automatically-generated rating, based on things such as
        // how often it has been played. This should be in the range 0.0 to 1.0.
        // trackInfo["xesam:autoRating"] = pTrack->

        trackInfo["xesam:comment"] = pTrack->getComment();
        trackInfo["xesam:composer"] = pTrack->getComposer();

        // Date/Time: When the track was created.
        // Usually only the year component will be useful.
        // Should be sent as strings in ISO 8601 extended format
        QDateTime dateTime;
        dateTime = dateTime.addYears(pTrack->getYear().toInt());
        trackInfo["xesam:contentCreated"] = dateTime.toString(Qt::ISODate);

        // Integer: The disc number on the album that this track is from.
        //trackInfo["xesam:discNumber"] = pTrack->

        //Date/Time: When the track was first played.
        //trackInfo["xesam:firstUsed"] = pTrack->

        trackInfo["xesam:genre"] = pTrack->getGenre();

        //Date/Time: When the track was last played.
        //trackInfo["xesam:lastUsed"] = pTrack->

        // List of Strings: The lyricist(s) of the track.
        //trackInfo["xesam:lyricist"] = pTrack->

        trackInfo["xesam:title"] = pTrack->getTitle();

        // Integer: The track number on the album disc.
        trackInfo["xesam:trackNumber"] = pTrack->getTrackNumber().toInt();

        // The location of the media file.
        trackInfo["xesam:url"] = pTrack->getURL();

        // Integer: The number of times the track has been played.
        trackInfo["xesam:useCount"] = pTrack->getTimesPlayed();

        // Float: A user-specified rating.
        // This should be in the range 0.0 to 1.0.
        // TODO curent rating have an integer type
        trackInfo["xesam:userRating"] = pTrack->getRating();

        metadata << trackInfo;

    }

    return metadata;
}

void MediaPlayer2TrackList::AddTrack(const QString& uri,
                                     const QDBusObjectPath& afterTrack,
                                     bool setAsCurrent) {
    Q_UNUSED(uri);
    Q_UNUSED(afterTrack);
    Q_UNUSED(setAsCurrent);
}

void MediaPlayer2TrackList::RemoveTrack(const QDBusObjectPath& trackId) {
    Q_UNUSED(trackId);
}

void MediaPlayer2TrackList::GoTo(const QDBusObjectPath& trackId) {
    Q_UNUSED(trackId);
}




