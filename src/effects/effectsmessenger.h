#pragma once

#include "engine/effects/message.h"

/// EffectsMessenger sends EffectsRequests from the main thread and receives
/// EffectsResponses from the audio thread. This allows memory allocation and
/// deallocation on the heap, which is slow, to be done in the main thread to
/// avoid blocking the audio thread and causing audible glitches.
/// Refer to
/// http://www.rossbencina.com/code/real-time-audio-programming-101-time-waits-for-nothing
/// for background information.
class EffectsMessenger {
  public:
    EffectsMessenger(EffectsRequestPipe* pRequestPipe, EffectsResponsePipe* m_pResponsePipe);
    ~EffectsMessenger();
    /// Write an EffectsRequest to the EngineEffectsManager. EffectsMessenger takes
    /// ownership of request and deletes it once a response is received.
    bool writeRequest(EffectsRequest* request);

    void startShutdownProcess();
    void processEffectsResponses();

  private:
    void collectGarbage(const EffectsRequest* pRequest);

    QString debugString() const {
        return "EffectsMessenger";
    }

    bool m_bShuttingDown;

    QScopedPointer<EffectsRequestPipe> m_pRequestPipe;
    QScopedPointer<EffectsResponsePipe> m_pResponsePipe;
    qint64 m_nextRequestId;
    QHash<qint64, EffectsRequest*> m_activeRequests;
};
