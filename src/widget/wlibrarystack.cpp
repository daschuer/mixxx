#include "wlibrarystack.h"

WLibraryStack::WLibraryStack(QWidget* parent)
        : QStackedWidget(parent),
          WWidget(parent) {

}

void WLibraryStack::showNext() {
    int newIndex = (currentIndex() + 1) % count();
    setCurrentIndex(newIndex);
}

void WLibraryStack::showPrevious() {
    int newIndex = currentIndex() - 1;
    setCurrentIndex(newIndex >= 0 ? newIndex : count() - 1);
}

