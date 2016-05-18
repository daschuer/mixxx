#ifndef WLIBRARYSTACK_H
#define WLIBRARYSTACK_H

#include <QStackedWidget>

#include "wpushbutton.h"
#include "wwidgetstack.h"

class WLibraryStack : public WWidgetStack
{
  public:
    WLibraryStack(QWidget* pParent,
                  ControlObject* pNextControl,
                  ControlObject* pPrevControl,
                  ControlObject* pCurrentPageControl,
                  ControlObject* pDropDownControl);
    
  private:
    
    ControlProxy m_dropDownControl;
};

#endif // WLIBRARYSTACK_H
