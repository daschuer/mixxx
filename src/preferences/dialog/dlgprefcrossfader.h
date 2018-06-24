#ifndef DLGPREFCROSSFADER_H
#define DLGPREFCROSSFADER_H

#include <QWidget>

#include "preferences/dialog/ui_dlgprefcrossfaderdlg.h"
#include "preferences/usersettings.h"
#include "control/controlproxylt.h"
#include "preferences/dlgpreferencepage.h"


class DlgPrefCrossfader : public DlgPreferencePage, public Ui::DlgPrefCrossfaderDlg  {
    Q_OBJECT
  public:
    DlgPrefCrossfader(QWidget* parent, UserSettingsPointer _config);
    virtual ~DlgPrefCrossfader();

  public slots:
    // Update X-Fader
    void slotUpdateXFader();
    // Apply changes to widget
    void slotApply();
    void slotUpdate();
    void slotResetToDefaults();

  signals:
    void apply(const QString &);

  private:
    void loadSettings();
    void drawXfaderDisplay();

    // Pointer to config object
    UserSettingsPointer m_config;

    QGraphicsScene* m_pxfScene;

    // X-fader values
    double m_xFaderMode, m_transform, m_cal;

    ControlProxyLt m_mode;
    ControlProxyLt m_curve;
    ControlProxyLt m_calibration;
    ControlProxyLt m_reverse;
    ControlProxyLt m_crossfader;

    bool m_xFaderReverse;
};

#endif
