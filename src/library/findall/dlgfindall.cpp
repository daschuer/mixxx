#include "library/findall/dlgfindall.h"

#include <QKeyEvent>
#include <QLineEdit>
#include <QMessageBox>

#include "controllers/keyboard/keyboardeventfilter.h"
#include "library/findall/findalltablemodel.h"
#include "library/library.h"
#include "moc_dlgfindall.cpp"
#include "track/track.h"
#include "util/assert.h"
#include "util/duration.h"
#include "widget/wlibrary.h"
#include "widget/wtracktableview.h"

namespace {
const char* kPreferenceGroupName = "[Auto DJ]";
const char* kRepeatPlaylistPreference = "Requeue";
} // anonymous namespace

DlgFindAll::DlgFindAll(WLibrary* parent,
        UserSettingsPointer pConfig,
        Library* pLibrary,
        FindAllProcessor* pProcessor,
        KeyboardEventFilter* pKeyboard)
        : QWidget(parent),
          Ui::DlgFindAll(),
          m_pConfig(pConfig),
          m_pFindAllProcessor(pProcessor),
          m_pTrackTableView(new WTrackTableView(this,
                  m_pConfig,
                  pLibrary,
                  parent->getTrackTableBackgroundColorOpacity())),
          m_bShowButtonText(parent->getShowButtonText()),
          m_pFindAllTableModel(nullptr) {
    setupUi(this);

    m_pTrackTableView->installEventFilter(pKeyboard);

    connect(m_pTrackTableView,
            &WTrackTableView::loadTrack,
            this,
            &DlgFindAll::loadTrack);
    connect(m_pTrackTableView,
            &WTrackTableView::loadTrackToPlayer,
            this,
            &DlgFindAll::loadTrackToPlayer);
    connect(m_pTrackTableView,
            &WTrackTableView::trackSelected,
            this,
            &DlgFindAll::trackSelected);
    connect(m_pTrackTableView,
            &WTrackTableView::trackSelected,
            this,
            &DlgFindAll::updateSelectionInfo);

    connect(pLibrary,
            &Library::setTrackTableFont,
            m_pTrackTableView,
            &WTrackTableView::setTrackTableFont);
    connect(pLibrary,
            &Library::setTrackTableRowHeight,
            m_pTrackTableView,
            &WTrackTableView::setTrackTableRowHeight);
    connect(pLibrary,
            &Library::setSelectedClick,
            m_pTrackTableView,
            &WTrackTableView::setSelectedClick);

    QBoxLayout* box = qobject_cast<QBoxLayout*>(layout());
    VERIFY_OR_DEBUG_ASSERT(box) { // Assumes the form layout is a QVBox/QHBoxLayout!
    }
    else {
        box->removeWidget(m_pTrackTablePlaceholder);
        m_pTrackTablePlaceholder->hide();
        box->insertWidget(1, m_pTrackTableView);
    }

    // We do _NOT_ take ownership of this from AutoDJProcessor.
    m_pFindAllTableModel = m_pFindAllProcessor->getTableModel();
    m_pTrackTableView->loadTrackModel(m_pFindAllTableModel);

    // Do not set this because it disables auto-scrolling
    // m_pTrackTableView->setDragDropMode(QAbstractItemView::InternalMove);

    m_enableBtnTooltip = tr(
            "Enable Auto DJ\n"
            "\n"
            "Shortcut: Shift+F12");
    m_disableBtnTooltip = tr(
            "Disable Auto DJ\n"
            "\n"
            "Shortcut: Shift+F12");
    QString fadeBtnTooltip = tr(
            "Trigger the transition to the next track\n"
            "\n"
            "Shortcut: Shift+F11");
    QString skipBtnTooltip = tr(
            "Skip the next track in the Auto DJ queue\n"
            "\n"
            "Shortcut: Shift+F10");
    QString shuffleBtnTooltip = tr(
            "Shuffle the content of the Auto DJ queue\n"
            "\n"
            "Shortcut: Shift+F9");
    QString addRandomTrackBtnTooltip = tr(
            "Adds a random track from track sources (crates) to the Auto DJ queue.\n"
            "If no track sources are configured, the track is added from the library instead.");
    QString repeatBtnTooltip = tr(
            "Repeat the playlist");
    QString spinBoxTransitionTooltip = tr(
            "Determines the duration of the transition");
    QString labelTransitionTooltip = tr(
            // "sec" as in seconds
            "Seconds");
    QString fadeModeTooltip = tr(
            "Auto DJ Fade Modes\n"
            "\n"
            "Full Intro + Outro:\n"
            "Play the full intro and outro. Use the intro or outro length as the\n"
            "crossfade time, whichever is shorter. If no intro or outro are marked,\n"
            "use the selected crossfade time.\n"
            "\n"
            "Fade At Outro Start:\n"
            "Start crossfading at the outro start. If the outro is longer than the\n"
            "intro, cut off the end of the outro. Use the intro or outro length as\n"
            "the crossfade time, whichever is shorter. If no intro or outro are\n"
            "marked, use the selected crossfade time.\n"
            "\n"
            "Full Track:\n"
            "Play the whole track. Begin crossfading from the selected number of\n"
            "seconds before the end of the track. A negative crossfade time adds\n"
            "silence between tracks.\n"
            "\n"
            "Skip Silence:\n"
            "Play the whole track except for silence at the beginning and end.\n"
            "Begin crossfading from the selected number of seconds before the\n"
            "last sound.\n"
            "\n"
            "Skip Silence Start Full Volume:\n"
            "The same as Skip Silence, but starting transitions with a centered\n"
            "crossfader, so that the intro starts at full volume.\n");

    pushButtonFadeNow->setToolTip(fadeBtnTooltip);
    pushButtonSkipNext->setToolTip(skipBtnTooltip);
    pushButtonShuffle->setToolTip(shuffleBtnTooltip);
    pushButtonAddRandomTrack->setToolTip(addRandomTrackBtnTooltip);
    pushButtonRepeatPlaylist->setToolTip(repeatBtnTooltip);
    spinBoxTransition->setToolTip(spinBoxTransitionTooltip);
    labelTransitionAppendix->setToolTip(labelTransitionTooltip);
    fadeModeCombobox->setToolTip(fadeModeTooltip);

    // Prevent the interactive widgets from being focused with Tab or Shift+Tab
    fadeModeCombobox->setFocusPolicy(Qt::ClickFocus);
    spinBoxTransition->setFocusPolicy(Qt::ClickFocus);
    // work around QLineEdit being protected
    QLineEdit* lineEditTransition(spinBoxTransition->findChild<QLineEdit*>());
    lineEditTransition->setFocusPolicy(Qt::ClickFocus);
    // Needed to catch Enter, Return and Escape keypresses
    lineEditTransition->installEventFilter(this);

    /*
        fadeModeCombobox->addItem(tr("Full Intro + Outro"),
                static_cast<int>(AutoDJProcessor::TransitionMode::FullIntroOutro));
        fadeModeCombobox->addItem(tr("Fade At Outro Start"),
                static_cast<int>(AutoDJProcessor::TransitionMode::FadeAtOutroStart));
        fadeModeCombobox->addItem(tr("Full Track"),
                static_cast<int>(AutoDJProcessor::TransitionMode::FixedFullTrack));
        fadeModeCombobox->addItem(tr("Skip Silence"),
                static_cast<int>(AutoDJProcessor::TransitionMode::FixedSkipSilence));
        fadeModeCombobox->addItem(tr("Skip Silence Start Full Volume"),
                static_cast<int>(AutoDJProcessor::TransitionMode::FixedStartCenterSkipSilence));
        fadeModeCombobox->setCurrentIndex(
                fadeModeCombobox->findData(static_cast<int>(m_pAutoDJProcessor->getTransitionMode())));
        */

    if (m_bShowButtonText) {
        pushButtonRepeatPlaylist->setText(tr("Repeat"));
    }
    bool repeatPlaylist = m_pConfig->getValue<bool>(
            ConfigKey(kPreferenceGroupName, kRepeatPlaylistPreference));
    pushButtonRepeatPlaylist->setChecked(repeatPlaylist);

    // Setup DlgAutoDJ UI based on the current AutoDJProcessor state. Keep in
    // mind that AutoDJ may already be active when DlgAutoDJ is created (due to
    // skin changes, etc.).
    /*
//    spinBoxTransition->setValue(static_cast<int>(m_pAutoDJProcessor->getTransitionTime()));
    connect(m_pAutoDJProcessor,
            &AutoDJProcessor::transitionTimeChanged,
            this,
            &DlgFindAll::transitionTimeChanged);

    connect(m_pAutoDJProcessor,
            &AutoDJProcessor::autoDJError,
            this,
            &DlgFindAll::autoDJError);

    connect(m_pAutoDJProcessor,
            &AutoDJProcessor::autoDJStateChanged,
            this,
            &DlgFindAll::autoDJStateChanged);
    autoDJStateChanged(m_pAutoDJProcessor->getState());
    */

    updateSelectionInfo();
}

DlgFindAll::~DlgFindAll() {
    qDebug() << "~DlgFindAll()";

    // Delete m_pTrackTableView before the table model. This is because the
    // table view saves the header state using the model.
    delete m_pTrackTableView;
}

void DlgFindAll::setupActionButton(QPushButton* pButton,
        void (DlgFindAll::*pSlot)(bool),
        const QString& fallbackText) {
    connect(pButton, &QPushButton::clicked, this, pSlot);
    if (m_bShowButtonText) {
        pButton->setText(fallbackText);
    }
}

void DlgFindAll::onShow() {
    m_pFindAllTableModel->select();
}

void DlgFindAll::onSearch(const QString& text) {
    // Do not allow filtering the Auto DJ playlist, because
    // Auto DJ will work from the filtered table
    Q_UNUSED(text);
}

/*
void DlgFindAll::autoDJStateChanged(AutoDJProcessor::AutoDJState state) {
    if (state == AutoDJProcessor::ADJ_DISABLED) {
        pushButtonAutoDJ->setChecked(false);
        pushButtonAutoDJ->setToolTip(m_enableBtnTooltip);
        if (m_bShowButtonText) {
            pushButtonAutoDJ->setText(tr("Enable"));
        }
        pushButtonFadeNow->setEnabled(false);
        pushButtonSkipNext->setEnabled(false);
    } else {
        // No matter the mode, you can always disable once it is enabled.
        pushButtonAutoDJ->setChecked(true);
        pushButtonAutoDJ->setToolTip(m_disableBtnTooltip);
        if (m_bShowButtonText) {
            pushButtonAutoDJ->setText(tr("Disable"));
        }

        // If fading, you can't hit fade now.
        if (state == AutoDJProcessor::ADJ_LEFT_FADING ||
                state == AutoDJProcessor::ADJ_RIGHT_FADING ||
                state == AutoDJProcessor::ADJ_ENABLE_P1LOADED) {
            pushButtonFadeNow->setEnabled(false);
        } else {
            pushButtonFadeNow->setEnabled(true);
        }

        pushButtonSkipNext->setEnabled(true);
    }
}

void DlgFindAll::slotTransitionModeChanged(int newIndex) {
    m_pAutoDJProcessor->setTransitionMode(
            static_cast<AutoDJProcessor::TransitionMode>(
                    fadeModeCombobox->itemData(newIndex).toInt()));
    // Clicking on a transition mode item moves keyboard focus to the list widget.
    // Move focus back to the previously focused library widget.
    ControlObject::set(ConfigKey("[Library]", "refocus_prev_widget"), 1);
}
*/

void DlgFindAll::updateSelectionInfo() {
    QModelIndexList indices = m_pTrackTableView->selectionModel()->selectedRows();

    // Derive total duration from the table model. This is much faster than
    // getting the duration from individual track objects.
    mixxx::Duration duration = {}; // m_pAutoDJTableModel->getTotalDuration(indices);

    QString label;

    if (!indices.isEmpty()) {
        label.append(mixxx::DurationBase::formatTime(duration.toDoubleSeconds()));
        label.append(QString(" (%1)").arg(indices.size()));
        labelSelectionInfo->setToolTip(tr("Displays the duration and number of selected tracks."));
        labelSelectionInfo->setText(label);
        labelSelectionInfo->setEnabled(true);
    } else {
        labelSelectionInfo->setText("");
        labelSelectionInfo->setEnabled(false);
    }
}

bool DlgFindAll::hasFocus() const {
    return m_pTrackTableView->hasFocus();
}

void DlgFindAll::setFocus() {
    m_pTrackTableView->setFocus();
}

void DlgFindAll::pasteFromSidebar() {
    m_pTrackTableView->pasteFromSidebar();
}

void DlgFindAll::keyPressEvent(QKeyEvent* pEvent) {
    // If we receive key events either the mode selector or the spinbox are focused.
    // Return, Enter and Escape move focus back to the previously focused
    // library widget in order to immediately allow keyboard shortcuts again.
    if (pEvent->key() == Qt::Key_Return ||
            pEvent->key() == Qt::Key_Enter ||
            pEvent->key() == Qt::Key_Escape) {
        ControlObject::set(ConfigKey("[Library]", "refocus_prev_widget"), 1);
        return;
    }
    QWidget::keyPressEvent(pEvent);
}

void DlgFindAll::saveCurrentViewState() {
    m_pTrackTableView->saveCurrentViewState();
}

bool DlgFindAll::restoreCurrentViewState() {
    return m_pTrackTableView->restoreCurrentViewState();
}
