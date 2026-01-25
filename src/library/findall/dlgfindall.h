#pragma once

#include <QString>
#include <QWidget>

#include "library/findall/findallprocessor.h"
#include "library/findall/ui_dlgfindall.h"
#include "library/libraryview.h"
#include "preferences/usersettings.h"
#include "track/track_decl.h"

class FindAllTableModel;
class WLibrary;
class WTrackTableView;
class Library;
class KeyboardEventFilter;

class DlgFindAll : public QWidget, public Ui::DlgFindAll, public LibraryView {
    Q_OBJECT
  public:
    DlgFindAll(WLibrary* parent,
            UserSettingsPointer pConfig,
            Library* pLibrary,
            FindAllProcessor* pProcessor,
            KeyboardEventFilter* pKeyboard);
    ~DlgFindAll() override;

    void onShow() override;
    bool hasFocus() const override;
    void setFocus() override;
    void pasteFromSidebar() override;
    void onSearch(const QString& text) override;
    void saveCurrentViewState() override;
    bool restoreCurrentViewState() override;

  public slots:

    //    void autoDJError(AutoDJProcessor::AutoDJError error);
    //    void autoDJStateChanged(AutoDJProcessor::AutoDJState state);
    void updateSelectionInfo();

  signals:
    void addRandomTrackButton(bool buttonChecked);
    void loadTrack(TrackPointer tio);
#ifdef __STEM__
    void loadTrackToPlayer(TrackPointer tio,
            const QString& group,
            mixxx::StemChannelSelection stemMask,
            bool);
#else
    void loadTrackToPlayer(TrackPointer tio, const QString& group, bool);
#endif
    void trackSelected(TrackPointer pTrack);

  private:
    void setupActionButton(QPushButton* pButton,
            void (DlgFindAll::*pSlot)(bool),
            const QString& fallbackText);
    void keyPressEvent(QKeyEvent* pEvent) override;

    const UserSettingsPointer m_pConfig;

    FindAllProcessor* const m_pFindAllProcessor;
    WTrackTableView* const m_pTrackTableView;
    const bool m_bShowButtonText;

    FindAllTableModel* m_pFindAllTableModel;

    QString m_enableBtnTooltip;
    QString m_disableBtnTooltip;
};
