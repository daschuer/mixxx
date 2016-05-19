#include <QDebug>
#include "wlibrarystack.h"


WLibraryStack::WLibraryStack(QWidget* pParent,
                             ControlObject* pNextControl,
                             ControlObject* pPrevControl,
                             ControlObject* pCurrentPageControl,
                             ControlObject* pConfigControl)
        : WWidgetStack(pParent,
                       pNextControl,
                       pPrevControl,
                       pCurrentPageControl),
          m_configControl(
              pConfigControl ?
              pConfigControl->getKey() : ConfigKey(), this),
          m_pActions(ConfigActions::NUM_ACTIONS) {

    m_configControl.connectValueChanged(SLOT(onConfigControlChanged(double)));


    m_pMenu = new QMenu(this);
    m_pMenuAdd = new QMenu(m_pMenu);
    m_pMenuAdd->setTitle(tr("Add Feature"));

    m_pActions[ConfigActions::LIBRARY] = m_pMenuAdd->addAction(tr("Library"));
    m_pActions[ConfigActions::LIBRARY_SIDEBAR] =
        m_pMenuAdd->addAction(tr("Library Sidebar"));

    m_pMenu->addMenu(m_pMenuAdd);
    m_pActions[ConfigActions::REMOVE] = m_pMenu->addAction(tr("Remove current element"));
}

void WLibraryStack::onConfigControlChanged(double v) {
    if (v > 0.0) {
        QAction* ret = m_pMenu->exec(QCursor::pos());

        int index = m_pActions.indexOf(ret);
        
        switch (index) {
            case ConfigActions::REMOVE:            
                removeWidget(currentWidget());
                break;
                
            case ConfigActions::LIBRARY:
                
                break;
                
            default:
                qWarning() << "Index" << index << "not found for" << ret;
        }
    }
}
