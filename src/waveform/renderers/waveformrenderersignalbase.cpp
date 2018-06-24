#include "waveformrenderersignalbase.h"

#include <QDomNode>

#include "waveform/waveformwidgetfactory.h"
#include "waveformwidgetrenderer.h"
#include "control/controlobject.h"
#include "control/controlproxy.h"
#include "widget/wskincolor.h"
#include "widget/wwidget.h"

WaveformRendererSignalBase::WaveformRendererSignalBase(
        WaveformWidgetRenderer* waveformWidgetRenderer)
    : WaveformRendererAbstract(waveformWidgetRenderer),
      m_alignment(Qt::AlignCenter),
      m_orientation(Qt::Horizontal),
      m_pColors(NULL),
      m_axesColor_r(0),
      m_axesColor_g(0),
      m_axesColor_b(0),
      m_axesColor_a(0),
      m_signalColor_r(0),
      m_signalColor_g(0),
      m_signalColor_b(0),
      m_lowColor_r(0),
      m_lowColor_g(0),
      m_lowColor_b(0),
      m_midColor_r(0),
      m_midColor_g(0),
      m_midColor_b(0),
      m_highColor_r(0),
      m_highColor_g(0),
      m_highColor_b(0),
      m_rgbLowColor_r(0),
      m_rgbLowColor_g(0),
      m_rgbLowColor_b(0),
      m_rgbMidColor_r(0),
      m_rgbMidColor_g(0),
      m_rgbMidColor_b(0),
      m_rgbHighColor_r(0),
      m_rgbHighColor_g(0),
      m_rgbHighColor_b(0) {
}

WaveformRendererSignalBase::~WaveformRendererSignalBase() {
}


bool WaveformRendererSignalBase::init() {
    m_eqEnabled.initialize(ConfigKey(
            m_waveformRenderer->getGroup(), "filterWaveformEnable"));
    m_lowFilterControlObject.initialize(ConfigKey(
            m_waveformRenderer->getGroup(), "filterLow"));
    m_midFilterControlObject.initialize(ConfigKey(
            m_waveformRenderer->getGroup(), "filterMid"));
    m_highFilterControlObject.initialize(ConfigKey(
            m_waveformRenderer->getGroup(), "filterHigh"));
    m_lowKillControlObject.initialize(ConfigKey(
            m_waveformRenderer->getGroup(), "filterLowKill"));
    m_midKillControlObject.initialize(ConfigKey(
            m_waveformRenderer->getGroup(), "filterMidKill"));
    m_highKillControlObject.initialize(ConfigKey(
            m_waveformRenderer->getGroup(), "filterHighKill"));

    return onInit();
}

void WaveformRendererSignalBase::setup(const QDomNode& node,
                                       const SkinContext& context) {
    QString orientationString = context.selectString(node, "Orientation").toLower();
    if (orientationString == "vertical") {
        m_orientation = Qt::Vertical;
    } else {
        m_orientation = Qt::Horizontal;
    }

    QString alignString = context.selectString(node, "Align").toLower();
    if (m_orientation == Qt::Horizontal) {
        if (alignString == "bottom") {
            m_alignment = Qt::AlignBottom;
        } else if (alignString == "top") {
            m_alignment = Qt::AlignTop;
        } else {
            m_alignment = Qt::AlignCenter;
        }
    } else {
        if (alignString == "left") {
            m_alignment = Qt::AlignLeft;
        } else if (alignString == "right") {
            m_alignment = Qt::AlignRight;
        } else {
            m_alignment = Qt::AlignCenter;
        }
    }

    m_pColors = m_waveformRenderer->getWaveformSignalColors();

    const QColor& l = m_pColors->getLowColor();
    l.getRgbF(&m_lowColor_r, &m_lowColor_g, &m_lowColor_b);

    const QColor& m = m_pColors->getMidColor();
    m.getRgbF(&m_midColor_r, &m_midColor_g, &m_midColor_b);

    const QColor& h = m_pColors->getHighColor();
    h.getRgbF(&m_highColor_r, &m_highColor_g, &m_highColor_b);

    const QColor& rgbLow = m_pColors->getRgbLowColor();
    rgbLow.getRgbF(&m_rgbLowColor_r, &m_rgbLowColor_g, &m_rgbLowColor_b);

    const QColor& rgbMid = m_pColors->getRgbMidColor();
    rgbMid.getRgbF(&m_rgbMidColor_r, &m_rgbMidColor_g, &m_rgbMidColor_b);

    const QColor& rgbHigh = m_pColors->getRgbHighColor();
    rgbHigh.getRgbF(&m_rgbHighColor_r, &m_rgbHighColor_g, &m_rgbHighColor_b);

    const QColor& axes = m_pColors->getAxesColor();
    axes.getRgbF(&m_axesColor_r, &m_axesColor_g, &m_axesColor_b,
                 &m_axesColor_a);

    const QColor& signal = m_pColors->getSignalColor();
    signal.getRgbF(&m_signalColor_r, &m_signalColor_g, &m_signalColor_b);

    onSetup(node);
}

void WaveformRendererSignalBase::getGains(float* pAllGain, float* pLowGain,
                                          float* pMidGain, float* pHighGain) {
    WaveformWidgetFactory* factory = WaveformWidgetFactory::instance();
    if (pAllGain != NULL) {
        float allGain = m_waveformRenderer->getGain();
        allGain *= factory->getVisualGain(::WaveformWidgetFactory::All);
        *pAllGain = allGain;
    }

    if (pLowGain || pMidGain || pHighGain) {
        // Per-band gain from the EQ knobs.
        float lowGain(1.0), midGain(1.0), highGain(1.0);

        // Only adjust low/mid/high gains if EQs are enabled.
        if (m_eqEnabled.toBool()) {
            if (m_lowFilterControlObject.valid() &&
                m_midFilterControlObject.valid() &&
                m_highFilterControlObject.valid()) {
                lowGain = m_lowFilterControlObject.get();
                midGain = m_midFilterControlObject.get();
                highGain = m_highFilterControlObject.get();
            }

            lowGain *= factory->getVisualGain(WaveformWidgetFactory::Low);
            midGain *= factory->getVisualGain(WaveformWidgetFactory::Mid);
            highGain *= factory->getVisualGain(WaveformWidgetFactory::High);

            if (m_lowKillControlObject.toBool()) {
                lowGain = 0;
            }

            if (m_midKillControlObject.toBool()) {
                midGain = 0;
            }

            if (m_highKillControlObject.toBool()) {
                highGain = 0;
            }
        }

        if (pLowGain != NULL) {
            *pLowGain = lowGain;
        }
        if (pMidGain != NULL) {
            *pMidGain = midGain;
        }
        if (pHighGain != NULL) {
            *pHighGain = highGain;
        }
    }
}
