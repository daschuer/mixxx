#ifndef WLIBRARYSTACK_H
#define WLIBRARYSTACK_H

#include <QStackedWidget>

#include "wpushbutton.h"
#include "wwidget.h"

class WLibraryStack : public QStackedWidget, public WWidget
{
  public:
    WLibraryStack(QWidget *parent = nullptr);
    
  public slots:
    
    void showNext();
    
    void showPrevious();
    
  private:
    
    WPushButton* m_nextButton;
    WPushButton* m_previousButton;
    WPushButton* m_addButton;
};

#endif // WLIBRARYSTACK_H
