#ifndef WLIBRARYVIEWMANAGER_H
#define WLIBRARYVIEWMANAGER_H

#include "wlibrarystack.h"

class WLibraryViewManager
{
  public:
    WLibraryViewManager();

    void addWLibraryStack(WLibraryStack *stack);

  private:

    QList<WLibraryStack*> m_stacks;
};

#endif // WLIBRARYVIEWMANAGER_H
