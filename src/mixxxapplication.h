#ifndef MIXXXAPPLICATION_H
#define MIXXXAPPLICATION_H

#include <QApplication>
#include "control/controlproxylt.h"

class MixxxApplication : public QApplication {
    Q_OBJECT
  public:
    MixxxApplication(int& argc, char** argv);
    ~MixxxApplication() override;

#ifndef Q_OS_MAC
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    virtual bool notify(QObject*, QEvent*);
#endif
#endif

  private:
    bool touchIsRightButton();
    void registerMetaTypes();

    int m_fakeMouseSourcePointId;
    QWidget* m_fakeMouseWidget;
    enum Qt::MouseButton m_activeTouchButton;
    ControlProxyLt m_touchShift;

};

#endif // MIXXXAPPLICATION_H
