#pragma once

#include <QComboBox>
#include <QWidget>
#include <functional>

#include "control/controlproxy.h"
#include "effects/effectsmanager.h"
#include "preferences/dialog/ui_dlgprefeqdlg.h"
#include "preferences/dlgpreferencepage.h"
#include "preferences/usersettings.h"

class DlgPrefEQ : public DlgPreferencePage, public Ui::DlgPrefEQDlg  {
    Q_OBJECT
  public:
    DlgPrefEQ(
            QWidget* parent,
            std::shared_ptr<EffectsManager> pEffectsManager,
            UserSettingsPointer _config);
    virtual ~DlgPrefEQ();

    QUrl helpUrl() const override;

    QString getEQEffectGroupForDeck(int deck) const;

  public slots:
    // Apply changes to widget
    void slotApply() override;
    void slotUpdate() override;
    void slotResetToDefaults() override;

  private slots:
    void slotEffectChangedOnDeck(int effectIndex);
    void slotNumDecksChanged(double numDecks);
    void slotSingleEqChecked(int checked);
    // Slot for toggling between advanced and basic views
    void slotPopulateDeckEffectSelectors();
    // Update Hi EQ
    void slotUpdateHiEQ();
    // Update Lo EQ
    void slotUpdateLoEQ();

    void slotUpdateEqAutoReset(int);
    void slotUpdateGainAutoReset(int);
    void slotBypass(int state);
    // Update the Master EQ
    void slotUpdateMasterEQParameter(int value);
    void slotMasterEQToDefault();
    void setMasterEQParameter(int i, double value);
    void slotMasterEqEffectChanged(int effectIndex);

  private:
    void loadSettings();
    void setDefaultShelves();
    double getEqFreq(int value, int minimum, int maximum);
    int getSliderPosition(double eqFreq, int minimum, int maximum);
    void validate_levels();
    void updateBandFilter(int index, double value);
    void setUpMasterEQ();
    void applySelections();

    typedef bool (*EffectManifestFilterFnc)(EffectManifest* pManifest);
    const QList<EffectManifestPointer> getFilteredManifests(
            EffectManifestFilterFnc filterFunc) const;
    void populateDeckBoxList(
            const QList<QComboBox*>& boxList,
            EffectManifestFilterFnc filterFunc);

    void applySelectionsToDecks();

    ControlProxy m_COLoFreq;
    ControlProxy m_COHiFreq;
    UserSettingsPointer m_pConfig;
    double m_lowEqFreq, m_highEqFreq;

    std::shared_ptr<EffectsManager> m_pEffectsManager;
    EffectsBackendManagerPointer m_pBackendManager;
    QLabel* m_firstSelectorLabel;
    QList<QComboBox*> m_deckEqEffectSelectors;
    ControlProxy* m_pNumDecks;

    bool m_inSlotPopulateDeckEffectSelectors;

    // Members needed for the Master EQ
    QList<QSlider*> m_masterEQSliders;
    QList<QLabel*> m_masterEQValues;
    QList<QLabel*> m_masterEQLabels;
    QWeakPointer<EffectSlot> m_pEffectMasterEQ;

    bool m_bEqAutoReset;
    bool m_bGainAutoReset;

    QList<int> m_eqIndiciesOnUpdate;
};
