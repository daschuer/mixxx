#ifndef WLIBRARYVIEWMANAGER_H
#define WLIBRARYVIEWMANAGER_H

#include "wlibrarystack.h"

class WLibraryViewManager
{
  public:
    WLibraryViewManager();

    inline void addWLibraryStack(WLibraryStack* stack) {
        if (!m_stacks.contains(stack)) {
            m_stacks.append(stack);
        }
    }

  private:

    QList<WLibraryStack*> m_stacks;
};

#endif // WLIBRARYVIEWMANAGER_H
