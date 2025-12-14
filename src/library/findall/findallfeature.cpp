#include <QMenu>
#include <QtDebug>

#include "controllers/keyboard/keyboardeventfilter.h"
#include "library/autodj/autodjfeature.h"
#include "library/autodj/autodjprocessor.h"
#include "library/autodj/dlgautodj.h"
#include "library/dao/trackschema.h"
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
          m_pSidebarModel(make_parented<TreeItemModel>(this)),
          m_viewName(Library::kFindAllViewName) {
    qRegisterMetaType<AutoDJProcessor::AutoDJState>("AutoDJState");
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

    // Create the "Crates" tree-item under the root item.
    std::unique_ptr<TreeItem> pRootItem = TreeItem::newRoot(this);
    m_pCratesTreeItem = pRootItem->appendChild(tr("Crates"));
    m_pCratesTreeItem->setIcon(QIcon(":/images/library/ic_library_crates.svg"));

    // Create tree-items under "Crates".
    constructCrateChildModel();

    m_pSidebarModel->setRootItem(std::move(pRootItem));

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
        WLibrary*,
        KeyboardEventFilter*) {
    /*
m_pAutoDJView = new DlgAutoDJ(
        libraryWidget,
        m_pConfig,
        m_pLibrary,
        m_pAutoDJProcessor,
        keyboard);
libraryWidget->registerView(m_viewName, m_pAutoDJView);
connect(m_pAutoDJView,
        &DlgAutoDJ::loadTrack,
        this,
        &AutoDJFeature::loadTrack);
connect(m_pAutoDJView,
        &DlgAutoDJ::loadTrackToPlayer,
        this,
        &LibraryFeature::loadTrackToPlayer);

connect(m_pAutoDJView,
        &DlgAutoDJ::trackSelected,
        this,
        &AutoDJFeature::trackSelected);

connect(m_pAutoDJView,
        &DlgAutoDJ::addRandomTrackButton,
        this,
        &AutoDJFeature::slotAddRandomTrack);

// Update shortcuts displayed in the context menu
QKeySequence toggleAutoDJShortcut = QKeySequence(
        keyboard->getKeyboardConfig()->getValueString(ConfigKey("[AutoDJ]", "enabled")),
        QKeySequence::PortableText);
m_pEnableAutoDJAction->setShortcut(toggleAutoDJShortcut);
m_pDisableAutoDJAction->setShortcut(toggleAutoDJShortcut);

*/
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
    emit switchToView(m_viewName);
    emit enableCoverArtDisplay(true);
    emit restoreSearch("TODO");
}

void FindAllFeature::paste() {
    emit pasteFromSidebar();
}

// Called by SidebarModel
void FindAllFeature::deleteItem(const QModelIndex& index) {
    TreeItem* pSelectedItem = static_cast<TreeItem*>(index.internalPointer());
    if (!pSelectedItem || pSelectedItem == m_pCratesTreeItem) {
        return;
    }
    CrateId crateId(pSelectedItem->getData());
    removeCrateFromAutoDj(crateId);
}

// Called by deleteItem and slotRemoveCrateFromAutoDj()
void FindAllFeature::removeCrateFromAutoDj(CrateId crateId) {
    DEBUG_ASSERT(crateId.isValid());
    // TODO Confirm dialog?
    m_pTrackCollection->updateAutoDjCrate(crateId, false);
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

void FindAllFeature::slotClearQueue() {
    clear();
}

// Add a crate to the AutoDJ sources
void FindAllFeature::slotAddCrateToAutoDj(CrateId crateId) {
    m_pTrackCollection->updateAutoDjCrate(crateId, true);
}

void FindAllFeature::slotRemoveCrateFromAutoDj() {
    CrateId crateId(m_pRemoveCrateFromAutoDjAction->data());
    removeCrateFromAutoDj(crateId);
}

void FindAllFeature::slotCrateChanged(CrateId crateId) {
    Crate crate;
    if (m_pTrackCollection->crates().readCrateById(crateId, &crate) && crate.isAutoDjSource()) {
        // Crate exists and is already a source for AutoDJ
        // -> Find and update the corresponding child item
        for (int i = 0; i < m_crateList.length(); ++i) {
            if (m_crateList[i].getId() == crateId) {
                QModelIndex parentIndex = m_pSidebarModel->index(0, 0);
                QModelIndex childIndex = m_pSidebarModel->index(i, 0, parentIndex);
                m_pSidebarModel->setData(childIndex, crate.getName(), Qt::DisplayRole);
                m_crateList[i] = crate;
                return; // early exit
            }
        }
        // No child item for crate found
        // -> Create and append a new child item for this crate
        // TODO() Use here std::span to get around the heap alloctaion of
        // std::vector for a single element.
        std::vector<std::unique_ptr<TreeItem>> rows;
        rows.push_back(std::make_unique<TreeItem>(crate.getName(), crate.getId().toVariant()));
        QModelIndex parentIndex = m_pSidebarModel->index(0, 0);
        m_pSidebarModel->insertTreeItemRows(std::move(rows), m_crateList.length(), parentIndex);
        m_crateList.append(crate);
    } else {
        // Crate does not exist or is not a source for AutoDJ
        // -> Find and remove the corresponding child item
        for (int i = 0; i < m_crateList.length(); ++i) {
            if (m_crateList[i].getId() == crateId) {
                QModelIndex parentIndex = m_pSidebarModel->index(0, 0);
                m_pSidebarModel->removeRows(i, 1, parentIndex);
                m_crateList.removeAt(i);
                return; // early exit
            }
        }
    }
}

void FindAllFeature::constructCrateChildModel() {
    m_crateList.clear();
    CrateSelectResult autoDjCrates(m_pTrackCollection->crates().selectAutoDjCrates(true));
    Crate crate;
    while (autoDjCrates.populateNext(&crate)) {
        // Create the TreeItem for this crate.
        m_pCratesTreeItem->appendChild(crate.getName(), crate.getId().toVariant());
        m_crateList.append(crate);
    }
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

void FindAllFeature::onRightClickChild(const QPoint& globalPos,
        const QModelIndex& index) {
    TreeItem* pClickedItem = static_cast<TreeItem*>(index.internalPointer());
    QMenu menu(m_pSidebarWidget);
    if (m_pCratesTreeItem == pClickedItem) {
        // The "Crates" parent item was right-clicked.
        // Bring up the context menu.
        QMenu crateMenu(m_pSidebarWidget);
        crateMenu.setTitle(tr("Add Crate as Track Source"));
        CrateSelectResult nonAutoDjCrates(m_pTrackCollection->crates().selectAutoDjCrates(false));
        Crate crate;
        while (nonAutoDjCrates.populateNext(&crate)) {
            auto pAction = std::make_unique<QAction>(crate.getName(), &crateMenu);
            auto crateId = crate.getId();
            connect(pAction.get(), &QAction::triggered, this, [this, crateId] {
                slotAddCrateToAutoDj(crateId);
            });
            crateMenu.addAction(pAction.get());
            pAction.release();
        }
        menu.addMenu(&crateMenu);
        menu.exec(globalPos);
    } else {
        // A crate child item was right-clicked.
        // Bring up the context menu.
        m_pRemoveCrateFromAutoDjAction->setData(pClickedItem->getData()); // the selected CrateId
        menu.addAction(m_pRemoveCrateFromAutoDjAction);
        menu.exec(globalPos);
    }
}
