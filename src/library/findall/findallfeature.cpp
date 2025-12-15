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
        UserSettingsPointer pConfig,
        PlayerManagerInterface*)
        : LibraryFeature(pLibrary, pConfig, QStringLiteral("findall")),
          m_pTrackCollection(pLibrary->trackCollectionManager()->internalCollection()),
          m_indexDao(m_pTrackCollection->getIndexDAO()),
          m_pSidebarModel(make_parented<TreeItemModel>(this)) {
    /*
    m_pAutoDJProcessor = new AutoDJProcessor(this,
            m_pConfig,
            pPlayerManager,
            pLibrary->trackCollectionManager(),
            m_iAutoDJPlaylistId);

    // Connect loadTrackToPlayer signal as a queued connection to make sure all callbacks of a
    // previous load attempt have been called #10504.
    connect(m_pAutoDJProcessor,
            &AutoDJProcessor::loadTrackToPlayer,
            this,
            &LibraryFeature::loadTrackToPlayer,
            Qt::QueuedConnection);
*/

    //   m_playlistDao.setAutoDJProcessor(m_pAutoDJProcessor);

    std::unique_ptr<TreeItem> pRootItem = TreeItem::newRoot(this);

    // Create the "Crates" tree-item under the root item.
    //    m_pCratesTreeItem = pRootItem->appendChild(tr("Crates"));
    //    m_pCratesTreeItem->setIcon(QIcon(":/images/library/ic_library_crates.svg"));

    // Create tree-items under "Crates".
    //    constructCrateChildModel();

    //    m_pSidebarModel->setRootItem(std::move(pRootItem));

    /*
    // Create context-menu items for enabling/disabling the auto-DJ
    m_pEnableAutoDJAction = make_parented<QAction>(tr("Enable Auto DJ"), this);
    connect(m_pEnableAutoDJAction.get(),
            &QAction::triggered,
            this,
            &AutoDJFeature::slotEnableAutoDJ);

    m_pDisableAutoDJAction = make_parented<QAction>(tr("Disable Auto DJ"), this);
    connect(m_pDisableAutoDJAction.get(),
            &QAction::triggered,
            this,
            &AutoDJFeature::slotDisableAutoDJ);

    // Create context-menu item for clearing the auto-DJ queue
    m_pClearQueueAction = make_parented<QAction>(tr("Clear Auto DJ Queue"), this);
    const auto removeKeySequence =
            // TODO(XXX): Qt6 replace enum | with QKeyCombination
            QKeySequence(static_cast<int>(kHideRemoveShortcutModifier) |
                    kHideRemoveShortcutKey);
    m_pClearQueueAction->setShortcut(removeKeySequence);
    connect(m_pClearQueueAction.get(),
            &QAction::triggered,
            this,
            &AutoDJFeature::slotClearQueue);

    */
    // Create context menu item to allow crates to be removed from AutoDJ sources.
    // onRightClickChild() gets the clicked crate's id form the sidebar model and
    // assigns it to this action's data.
    // In slotRemoveCrateFromAutoDj() we retrieve the CrateId data and finally
    // remove the crate from sources in removeCrateFromAutoDj().

    /*
    m_pRemoveCrateFromAutoDjAction =
            make_parented<QAction>(tr("Remove Crate as Track Source"), this);
    m_pRemoveCrateFromAutoDjAction->setShortcut(removeKeySequence);

    connect(m_pRemoveCrateFromAutoDjAction.get(),
            &QAction::triggered,
            this,
            &AutoDJFeature::slotRemoveCrateFromAutoDj);

            */
}

FindAllFeature::~FindAllFeature() {
    // delete m_pFindAllProcessor;
}

QVariant FindAllFeature::title() {
    return tr("Find All");
}

void FindAllFeature::bindLibraryWidget(
        WLibrary* pLibraryWidget,
        KeyboardEventFilter* pKeyboard) {
    m_pFindAllView = new DlgFindAll(
            pLibraryWidget,
            m_pConfig,
            m_pLibrary,
            nullptr,
            pKeyboard);
    pLibraryWidget->registerView(Library::kFindAllViewName, m_pFindAllView);
    connect(m_pFindAllView,
            &DlgFindAll::loadTrack,
            this,
            &FindAllFeature::loadTrack);
    connect(m_pFindAllView,
            &DlgFindAll::loadTrackToPlayer,
            this,
            &LibraryFeature::loadTrackToPlayer);

    connect(m_pFindAllView,
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
    emit restoreSearch("TODO");
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
