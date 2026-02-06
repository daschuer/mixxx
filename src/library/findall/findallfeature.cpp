#include "library/findall/findallfeature.h"

#include <QMenu>
#include <QtDebug>

#include "controllers/keyboard/keyboardeventfilter.h"
#include "library/dao/trackschema.h"
#include "library/findall/dlgfindall.h"
#include "library/library.h"
#include "library/parser.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "library/trackset/crate/cratestorage.h"
#include "library/treeitem.h"
#include "moc_findallfeature.cpp"
#include "sources/soundsourceproxy.h"
#include "track/track.h"
#include "util/clipboard.h"
#include "util/defs.h"
#include "util/dnd.h"
#include "widget/wlibrary.h"
#include "widget/wlibrarysidebar.h"

namespace {

} // anonymous namespace

FindAllFeature::FindAllFeature(Library* pLibrary,
        UserSettingsPointer pConfig)
        : LibraryFeature(pLibrary, pConfig, QStringLiteral("findall")),
          m_pTrackCollection(pLibrary->trackCollectionManager()->internalCollection()),
          m_indexDao(m_pTrackCollection->getIndexDAO()),
          m_pSidebarModel(make_parented<TreeItemModel>(this)) {
    m_pFindAllProcessor = std::make_unique<FindAllProcessor>(this,
            m_pConfig,
            pLibrary->trackCollectionManager());

    std::unique_ptr<TreeItem> pRootItem = TreeItem::newRoot(this);


}

FindAllFeature::~FindAllFeature() {
}

QVariant FindAllFeature::title() {
    return tr("Find All");
}

void FindAllFeature::bindLibraryWidget(
        WLibrary* pLibraryWidget,
        KeyboardEventFilter* pKeyboard) {
    m_pFindAllView = std::make_unique<DlgFindAll>(
            pLibraryWidget,
            m_pConfig,
            m_pLibrary,
            m_pFindAllProcessor.get(),
            pKeyboard);
    pLibraryWidget->registerView(Library::kFindAllViewName, m_pFindAllView.get());
    connect(m_pFindAllView.get(),
            &DlgFindAll::loadTrack,
            this,
            &FindAllFeature::loadTrack);
    connect(m_pFindAllView.get(),
            &DlgFindAll::loadTrackToPlayer,
            this,
            &LibraryFeature::loadTrackToPlayer);

    connect(m_pFindAllView.get(),
            &DlgFindAll::trackSelected,
            this,
            &FindAllFeature::trackSelected);
}

void FindAllFeature::bindSidebarWidget(WLibrarySidebar* pSidebarWidget) {
    // store the sidebar widget pointer for later use in onRightClickChild
    m_pSidebarWidget = pSidebarWidget;
}

TreeItemModel* FindAllFeature::sidebarModel() const {
    return m_pSidebarModel;
}

void FindAllFeature::activate() {
    // qDebug() << "AutoDJFeature::activate()";
    emit switchToView(Library::kFindAllViewName);
    emit enableCoverArtDisplay(true);
}

bool FindAllFeature::dragMoveAccept(const QUrl& url) {
    return SoundSourceProxy::isUrlSupported(url) ||
            Parser::isPlaylistFilenameSupported(url.toLocalFile());
}

void FindAllFeature::slotEnableAutoDJ() {
    //  m_pAutoDJProcessor->toggleAutoDJ(true);
}

void FindAllFeature::slotDisableAutoDJ() {
    // m_pAutoDJProcessor->toggleAutoDJ(false);
}

void FindAllFeature::onRightClick(const QPoint& globalPos) {
    QMenu menu(m_pSidebarWidget);
    /*
    if (m_pAutoDJProcessor->getState() == AutoDJProcessor::ADJ_DISABLED) {
        menu.addAction(m_pEnableAutoDJAction.get());
    } else {
        menu.addAction(m_pDisableAutoDJAction.get());
    }
    */
    menu.addAction(m_pClearQueueAction.get());
    menu.exec(globalPos);
}
