#ifndef WLIBRARYSTACK_H
#define WLIBRARYSTACK_H

#include <QMenu>

#include "wwidgetstack.h"

class WLibraryStack : public WWidgetStack
{
    Q_OBJECT
    
    enum ConfigActions {
        REMOVE,
        LIBRARY,
        LIBRARY_SIDEBAR,
        
        // This should be always the last value
        NUM_ACTIONS
    };

  public:
    WLibraryStack(QWidget* pParent,
                  ControlObject* pNextControl,
                  ControlObject* pPrevControl,
                  ControlObject* pCurrentPageControl,
                  ControlObject* pConfigControl);
    
  private slots:

    void onConfigControlChanged(double v);

  private:

    ControlProxy m_configControl;
    
    QMenu* m_pMenu;
    QMenu* m_pMenuAdd;
    
    QVector<QAction*> m_pActions;
};

#endif // WLIBRARYSTACK_H
