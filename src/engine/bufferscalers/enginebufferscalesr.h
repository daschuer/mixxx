/**
 * FILE: enginebufferscalesr.h
 * -----------------------------
 * Scaler class that uses the libsamplerate API
 */

#pragma once

#include <samplerate.h>

#include <memory>

#include "engine/bufferscalers/enginebufferscale.h"
#include "util/samplebuffer.h"

class ReadAheadManager;

class EngineBufferScaleSR : public EngineBufferScale {
    Q_OBJECT
  public:
    explicit EngineBufferScaleSR(); // input driven
    ~EngineBufferScaleSR() override;

    // Main scaler method
    double scaleBuffer(
            CSAMPLE* pOutputBuffer,
            SINT iOutputBufferSize) override;

    double recScaleBuffer(const CSAMPLE* pInputBuffer,
            CSAMPLE* pOutputBuffer,
            SINT iInputBufferSize,
            double baseRate);

    void clear() override;

  private:
    void onSignalChanged() override;

    mixxx::audio::ChannelCount m_dChannels;
    SRC_STATE* m_pResampler;
};
