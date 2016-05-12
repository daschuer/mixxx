#ifndef WLIBRARYSTACK_H
#define WLIBRARYSTACK_H

#include <QStackedWidget>

#include "wpushbutton.h"
#include "wwidget.h"

class WLibraryStack : public QStackedWidget, public WWidget
{
public:
    WLibraryStack(QWidget *parent = nullptr);
};

#endif // WLIBRARYSTACK_H
