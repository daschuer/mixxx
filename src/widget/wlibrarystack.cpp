#include "wlibrarystack.h"


WLibraryStack::WLibraryStack(QWidget* pParent,
                             ControlObject* pNextControl,
                             ControlObject* pPrevControl,
                             ControlObject* pCurrentPageControl,
                             ControlObject* pDropDownControl)
        : WWidgetStack(pParent,
                       pNextControl,
                       pPrevControl,
                       pCurrentPageControl),
          m_dropDownControl(
              pDropDownControl ?
              pDropDownControl->getKey() : ConfigKey(), this) {
    
}
