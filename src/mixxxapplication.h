#pragma once

#include <QApplication>

#include "control/controlproxylt.h"

class MixxxApplication : public QApplication {
    Q_OBJECT
  public:
    MixxxApplication(int& argc, char** argv);
    ~MixxxApplication() override = default;

    bool notify(QObject*, QEvent*) override;

  private:
    bool touchIsRightButton();
    void registerMetaTypes();

    int m_rightPressedButtons;
    ControlProxyLt m_touchShift;
};
