#include "wlibraryviewmanager.h"

WLibraryViewManager::WLibraryViewManager() {

}

void WLibraryViewManager::addWLibraryStack(WLibraryStack *stack) {

    for (WLibraryStack* s : m_stacks) {
        // Check not to add the same stack twice
        if (s == stack) {
            return;
        }
    }
    
    m_stacks.append(stack);
}

