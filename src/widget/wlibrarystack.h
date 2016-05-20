#ifndef WLIBRARYSTACK_H
#define WLIBRARYSTACK_H

#include <QMenu>

#include "library/library.h"
#include "controllers/keyboard/keyboardeventfilter.h"
#include "controllers/controllerlearningeventfilter.h"
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
                  Library* pLibrary,
                  ControllerLearningEventFilter* pControllerLearningEventFilter,
                  KeyboardEventFilter* pKeyboardEventFilter,
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
    
    // Necessary to add the Library and LibrarySidebar
    Library* m_pLibrary;
    ControllerLearningEventFilter* m_pControllerLearningEventFilter;
    KeyboardEventFilter* m_pKeyboard;
};

#endif // WLIBRARYSTACK_H
