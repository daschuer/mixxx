#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QUrl>
#include <QVariant>

#include "library/dao/autodjcratesdao.h"
#include "library/dao/indexdao.h"
#include "library/libraryfeature.h"
#include "library/trackset/crate/crate.h"
#include "preferences/usersettings.h"
#include "util/parented_ptr.h"

class DlgAutoDJ;
class Library;
class PlayerManagerInterface;
class TrackCollection;
class AutoDJProcessor;
class WLibrarySidebar;
class QAction;
class QModelIndex;
class QPoint;

class FindAllFeature : public LibraryFeature {
    Q_OBJECT
  public:
    FindAllFeature(Library* pLibrary,
            UserSettingsPointer pConfig,
            PlayerManagerInterface* pPlayerManager);
    virtual ~FindAllFeature();

    QVariant title() override;

    void paste() override;
    void deleteItem(const QModelIndex& index) override;

    bool dragMoveAccept(const QUrl& url) override;

    void bindLibraryWidget(WLibrary* libraryWidget,
            KeyboardEventFilter* keyboard) override;
    void bindSidebarWidget(WLibrarySidebar* pSidebarWidget) override;

    TreeItemModel* sidebarModel() const override;

    bool hasTrackTable() override {
        return true;
    }

  public slots:
    void activate() override;

    void onRightClick(const QPoint& globalPos) override;
    // Temporary, until WCrateTableView can be written.
    void onRightClickChild(const QPoint& globalPos, const QModelIndex& index) override;

  private:
    TrackCollection* const m_pTrackCollection;
    IndexDAO& m_indexDao;

    //    FindAllProcessor* m_pFindAllProcessor;
    parented_ptr<TreeItemModel> m_pSidebarModel;
    //    FindAllView* m_pFindAllView;
    const QString m_viewName;

    // Initialize the list of crates loaded into the auto-DJ queue.
    void constructCrateChildModel();
    void removeCrateFromAutoDj(CrateId crateId = CrateId());

    // The "Crates" tree-item under the "Auto DJ" tree-item.
    TreeItem* m_pCratesTreeItem;

    // The crate ID and name of all loaded crates.
    // Its indices correspond one-to-one with tree-items contained by the
    // "Crates" tree-item.
    QList<Crate> m_crateList;

    parented_ptr<QAction> m_pEnableAutoDJAction;
    parented_ptr<QAction> m_pDisableAutoDJAction;
    parented_ptr<QAction> m_pClearQueueAction;

    // A context-menu item that allows crates to be removed from the
    // auto-DJ list.
    parented_ptr<QAction> m_pRemoveCrateFromAutoDjAction;

    QPointer<WLibrarySidebar> m_pSidebarWidget;

  private slots:
    void slotEnableAutoDJ();
    void slotDisableAutoDJ();
    void slotClearQueue();

    // Add a crate to the auto-DJ queue.
    void slotAddCrateToAutoDj(CrateId crateId);
    // Implements the context-menu item.
    void slotRemoveCrateFromAutoDj();
    void slotCrateChanged(CrateId crateId);
    // Adds a random track from all loaded crates to the auto-DJ queue.
    void slotAddRandomTrack();
};
