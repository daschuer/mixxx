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

class DlgFindAll;
class Library;
class PlayerManagerInterface;
class TrackCollection;
class FindAllProcessor;
class WLibrarySidebar;
class QAction;
class QModelIndex;
class QPoint;

class FindAllFeature : public LibraryFeature {
    Q_OBJECT
  public:
    FindAllFeature(Library* pLibrary,
            UserSettingsPointer pConfig);
    virtual ~FindAllFeature();

    QVariant title() override;

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

  private:
    std::unique_ptr<TrackCollection> m_pTrackCollection;
    IndexDAO& m_indexDao;

    std::unique_ptr<FindAllProcessor> m_pFindAllProcessor;
    parented_ptr<TreeItemModel> m_pSidebarModel;
    DlgFindAll* m_pFindAllView;

    // Initialize the list of crates loaded into the auto-DJ queue.
    void constructCrateChildModel();

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
};
