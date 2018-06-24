#ifndef WAVEFORMMARKRANGE_H
#define WAVEFORMMARKRANGE_H

#include <QImage>

#include "skin/skincontext.h"
#include "util/memory.h"

class ControlProxy;
class QDomNode;
class WaveformSignalColors;

class WaveformMarkRange {
  public:
    WaveformMarkRange() {
    }
    WaveformMarkRange(const WaveformMarkRange&) = delete;
    WaveformMarkRange(WaveformMarkRange&&) = default;

    // If a mark range is active it has valid start/end points so it should be
    // drawn on waveforms.
    bool active();
    // If a mark range is enabled that means it should be painted with its
    // active color instead of its disabled color.
    bool enabled();
    // Returns start value or -1 if the start control doesn't exist.
    double start();
    // Returns end value or -1 if the end control doesn't exist.
    double end();

    void setup(const QString &group, const QDomNode& node,
               const SkinContext& context,
               const WaveformSignalColors& signalColors);

  private:
    void generateImage(int weidth, int height);

    // We need here pointers, because QObjects are not move-able
    std::unique_ptr<ControlProxy> m_pMarkStartPointControl;
    std::unique_ptr<ControlProxy> m_pMarkEndPointControl;
    std::unique_ptr<ControlProxy> m_pMarkEnabledControl;

    QColor m_activeColor;
    QColor m_disabledColor;

    QImage m_activeImage;
    QImage m_disabledImage;

    friend class WaveformRenderMarkRange;
    friend class WOverview;
};

#endif // WAVEFORMMARKRANGE_H
