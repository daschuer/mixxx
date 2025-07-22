#include "preferences/dialog/dlgprefrecord.h"

#include <QFileDialog>
#include <QRadioButton>
#include <QStandardPaths>

#include "control/controlproxy.h"
#include "encoder/encoder.h"
#include "encoder/encodermp3settings.h"
#include "moc_dlgprefrecord.cpp"
#include "recording/defs_recording.h"
#include "util/sandbox.h"

namespace {
constexpr bool kDefaultCueEnabled = true;
constexpr bool kDefaultCueFileAnnotationEnabled = false;

const ConfigKey kRecSampleRateCfgkey =
        ConfigKey(QStringLiteral(RECORDING_PREF_KEY), QStringLiteral("rec_samplerate"));

const ConfigKey kUseEngineSampleRateCfgkey =
        ConfigKey(QStringLiteral(RECORDING_PREF_KEY), QStringLiteral("use_engine_samplerate"));

} // anonymous namespace

DlgPrefRecord::DlgPrefRecord(QWidget* parent, UserSettingsPointer pConfig)
        : DlgPreferencePage(parent),
          m_pConfig(pConfig),
          m_selFormat({QString(), QString(), false, QString()}),
          m_recSampleRate(kRecSampleRateCfgkey),
          m_useEngineSampleRate(kUseEngineSampleRateCfgkey),
          m_oldRecSampleRate(m_recSampleRate.get()),
          m_oldRecSampleRateOpus(48000.0),
          m_bUseOpusEncoder(false),
          m_oldRecSampleRateMP3(48000.0),
          m_bUseMP3Encoder(false) {
    setupUi(this); // uses the .ui file to generate qt gui elements.
    recSampleRateComboBox->setEnabled(false);

    auto engineSampleRateProxy = PollingControlProxy(
            QStringLiteral("[App]"), QStringLiteral("samplerate"));
    double engineSampleRate = engineSampleRateProxy.get();
    m_defaultSampleRate = engineSampleRate;

    qDebug() << "engine sample rate seen by dlgprefrecord: " << m_defaultSampleRate;
    RadioButtonUseDefaultSampleRate->setText(QString("Default (%1 Hz)").arg(engineSampleRate));

    // Setting recordings path.
    QString recordingsPath = m_pConfig->getValueString(ConfigKey(RECORDING_PREF_KEY, "Directory"));
    if (recordingsPath.isEmpty()) {
        // Initialize recordings path in config to old default path.
        // Do it here so we show current value in UI correctly.
        QString musicDir = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
        QDir recordDir(musicDir + "/Mixxx/Recordings");
        recordingsPath = recordDir.absolutePath();
        m_pConfig->setValue(ConfigKey(RECORDING_PREF_KEY, "Directory"), recordingsPath);
    }
    LineEditRecordings->setText(recordingsPath);
    connect(PushButtonBrowseRecordings,
            &QAbstractButton::clicked,
            this,
            &DlgPrefRecord::slotBrowseRecordingsDir);

    // Setting Encoder
    bool found = false;
    QString prefformat = m_pConfig->getValueString(ConfigKey(RECORDING_PREF_KEY, "Encoding"));
    for (const Encoder::Format& format : EncoderFactory::getFactory().getFormats()) {
        QRadioButton* button = new QRadioButton(format.label, this);
        button->setObjectName(format.internalName);
        connect(button, &QAbstractButton::clicked, this, &DlgPrefRecord::slotFormatChanged);
        if (format.lossless) {
            LosslessEncLayout->addWidget(button);
        } else {
            LossyEncLayout->addWidget(button);
        }
        encodersgroup.addButton(button);

        if (prefformat == format.internalName) {
            m_selFormat = format;
            button->setChecked(true);
            found=true;
        }
        m_formatButtons.append(button);
    }
    if (!found) {
        // If no format was available, set to WAVE as default.
        if (!prefformat.isEmpty()) {
            qWarning() << prefformat <<" format was set in the configuration, but it is not recognized!";
        }
        m_selFormat = EncoderFactory::getFactory().getFormats().first();
        m_formatButtons.first()->setChecked(true);
        m_pConfig->set(ConfigKey(RECORDING_PREF_KEY, "Encoding"),  ConfigValue(m_selFormat.internalName));
    }

    // recording samplerates
    if (prefformat == ENCODING_OPUS) {
        updateSampleRates(kRecSampleRatesOpus);
    } else if (prefformat == ENCODING_MP3) {
        updateSampleRates(kRecSampleRatesMP3);
    } else {
        updateSampleRates(kRecSampleRates);
    }

    setupEncoderUI();

    // Setting Metadata
    loadMetaData();

    // Setting miscellaneous
    CheckBoxRecordCueFile->setChecked(m_pConfig->getValue<bool>(
            ConfigKey(RECORDING_PREF_KEY, "CueEnabled"), kDefaultCueEnabled));

    CheckBoxUseCueFileAnnotation->setChecked(m_pConfig->getValue<bool>(
            ConfigKey(RECORDING_PREF_KEY, "cue_file_annotation_enabled"),
            kDefaultCueFileAnnotationEnabled));

    // Setting split
    comboBoxSplitting->addItem(SPLIT_650MB);
    comboBoxSplitting->addItem(SPLIT_700MB);
    comboBoxSplitting->addItem(SPLIT_1024MB);
    comboBoxSplitting->addItem(SPLIT_2048MB);
    comboBoxSplitting->addItem(SPLIT_4096MB);
    comboBoxSplitting->addItem(SPLIT_60MIN);
    comboBoxSplitting->addItem(SPLIT_74MIN);
    comboBoxSplitting->addItem(SPLIT_80MIN);
    comboBoxSplitting->addItem(SPLIT_120MIN);

    QString fileSizeStr = m_pConfig->getValueString(ConfigKey(RECORDING_PREF_KEY, "FileSize"));
    int index = comboBoxSplitting->findText(fileSizeStr);
    if (index >= 0) {
        // Set file split size
        comboBoxSplitting->setCurrentIndex(index);
    } else {
        //Use max RIFF size (4GB) as default index, since usually people don't want to split.
        comboBoxSplitting->setCurrentIndex(4);
        saveSplitSize();
    }

    setScrollSafeGuard(comboBoxSplitting);

    connect(RadioButtonUseDefaultSampleRate,
            &QRadioButton::toggled,
            this,
            [this](bool checked) {
                slotToggleCustomSampleRateIgnore(checked ? Qt::Checked : Qt::Unchecked);
            });

    connect(SliderQuality, &QAbstractSlider::valueChanged, this, &DlgPrefRecord::slotSliderQuality);
    connect(SliderQuality, &QAbstractSlider::sliderMoved, this, &DlgPrefRecord::slotSliderQuality);
    connect(SliderQuality,
            &QAbstractSlider::sliderReleased,
            this,
            &DlgPrefRecord::slotSliderQuality);
    connect(SliderCompression,
            &QAbstractSlider::valueChanged,
            this,
            &DlgPrefRecord::slotSliderCompression);
    connect(SliderCompression,
            &QAbstractSlider::sliderMoved,
            this,
            &DlgPrefRecord::slotSliderCompression);
    connect(SliderCompression,
            &QAbstractSlider::sliderReleased,
            this,
            &DlgPrefRecord::slotSliderCompression);

    connect(CheckBoxRecordCueFile,
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
            &QCheckBox::checkStateChanged,
#else
            &QCheckBox::stateChanged,
#endif
            this,
            &DlgPrefRecord::slotToggleCueEnabled);

    // add new element ex here, define the slot that is
    // called when a new value is chosen. arg0 of connect
    // must be used in the corresponding .ui file
}

DlgPrefRecord::~DlgPrefRecord() {
    // Note: I don't disconnect signals, since that's supposedly done automatically
    // when the object is deleted
    for (QRadioButton* button : std::as_const(m_formatButtons)) {
        if (LosslessEncLayout->indexOf(button) != -1) {
            LosslessEncLayout->removeWidget(button);
        } else {
            LossyEncLayout->removeWidget(button);
        }
        button->deleteLater();
    }
    for (QAbstractButton* widget : std::as_const(m_optionWidgets)) {
        OptionGroupsLayout->removeWidget(widget);
        widget->deleteLater();
    }
    m_optionWidgets.clear();
}

void DlgPrefRecord::slotApply() {
    saveRecordingFolder();
    saveMetaData();
    saveEncoding();
    saveUseCueFile();
    saveUseCueFileAnnotation();
    saveSplitSize();
    saveRecSampleRate();
}

// Called whenever the use default samplerate radiobutton
// is toggled (on/off)
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
void DlgPrefRecord::slotToggleCustomSampleRateIgnore(Qt::CheckState buttonState) {
#else
void DlgPrefRecord::slotToggleCustomSampleRateIgnore(int buttonState) {
#endif
    recSampleRateComboBox->setEnabled(buttonState != Qt::Checked);

    // if custom samplerate enabled: set false
    if (RadioButtonUseCustomSampleRate->isChecked()) {
        m_useEngineSampleRate.set(false);
        const double recSampleRate = recSampleRateComboBox->currentData().value<double>();
        qDebug() << "Using custom recording samplerate: " << recSampleRate << " Hz";
        return;
    }

    m_useEngineSampleRate.set(true);
    qDebug() << "Using default (engine samplerate) as recording samplerate";
}

// m_pConfig is accessible from the enginerecord instance.
// only when a custom recording samplerate is being used.
void DlgPrefRecord::saveRecSampleRate() {
    double recSampleRate{};
    if (!m_useEngineSampleRate.toBool()) {
        // samplerate values stored as double in additem()
        recSampleRate = recSampleRateComboBox->currentData().value<double>();
        qDebug() << "Apply custom: recording samplerate changed to: " << recSampleRate << " Hz";
    } else {
        recSampleRate = m_defaultSampleRate;
        qDebug() << "Apply default: recording samplerate changed to: " << recSampleRate << " Hz";
    }
    m_recSampleRate.set(recSampleRate);

    m_pConfig->set(kRecSampleRateCfgkey, ConfigValue(m_recSampleRate.get()));
}
// This function updates/refreshes the contents of this dialog.
void DlgPrefRecord::slotUpdate() {
    // Find out the max width of the labels. This is needed to keep the
    // UI fixed in size when hiding or showing elements.
    // It is not perfect, but it didn't get better than this.
    int max=0;
    if (LabelQuality->size().width()> max) {
        max = LabelQuality->size().width() + 10; // quick fix for gui bug
    }
    LabelLossless->setMaximumWidth(max);
    LabelLossy->setMaximumWidth(max);
    LabelLossless->setMinimumWidth(max);
    LabelLossy->setMinimumWidth(max);

    QString recordingsPath = m_pConfig->getValueString(ConfigKey(RECORDING_PREF_KEY, "Directory"));
    LineEditRecordings->setText(recordingsPath);

    QString prefformat = m_pConfig->getValueString(ConfigKey(RECORDING_PREF_KEY, "Encoding"));
    for (const Encoder::Format& format : EncoderFactory::getFactory().getFormats()) {
        if (prefformat == format.internalName) {
            m_selFormat = format;
            break;
        }
    }
    setupEncoderUI();

    loadMetaData();

    // Setting miscellaneous
    CheckBoxRecordCueFile->setChecked(m_pConfig->getValue<bool>(
            ConfigKey(RECORDING_PREF_KEY, "CueEnabled"), kDefaultCueEnabled));

    slotToggleCueEnabled();

    QString fileSizeStr = m_pConfig->getValueString(ConfigKey(RECORDING_PREF_KEY, "FileSize"));
    int index = comboBoxSplitting->findText(fileSizeStr);
    if (index >= 0) {
        comboBoxSplitting->setCurrentIndex(index);
    }
}

void DlgPrefRecord::slotResetToDefaults() {
    m_formatButtons.first()->setChecked(true);
    m_selFormat = EncoderFactory::getFactory().getFormatFor(
            m_formatButtons.first()->objectName());
    setupEncoderUI();
    // TODO (XXX): It would be better that a defaultSettings() method is added
    // to the EncoderSettings interface so that we know which option to set
    m_optionWidgets.first()->setChecked(true);

    LineEditTitle->setText("");
    LineEditAlbum->setText("");
    LineEditAuthor->setText("");

    // 4GB splitting is the default
    comboBoxSplitting->setCurrentIndex(4);

    // Sets 'Create a CUE file' checkbox value
    CheckBoxRecordCueFile->setChecked(kDefaultCueEnabled);

    // Sets 'Enable File Annotation in CUE file' checkbox value
    CheckBoxUseCueFileAnnotation->setChecked(kDefaultCueFileAnnotationEnabled);

    // Let 48000 Hz be the "default" non-engine sample rate.
    int index = recSampleRateComboBox->findData(QVariant::fromValue(48000.0));
    if (index != -1) {
        recSampleRateComboBox->setCurrentIndex(index);
    }
}

void DlgPrefRecord::slotBrowseRecordingsDir() {
    QString fd = QFileDialog::getExistingDirectory(
            this, tr("Choose recordings directory"),
            m_pConfig->getValueString(
                    ConfigKey(RECORDING_PREF_KEY,"Directory")));

    if (fd != "") {
        // The user has picked a new directory via a file dialog. This means the
        // system sandboxer (if we are sandboxed) has granted us permission to
        // this folder. Create a security bookmark while we have permission so
        // that we can access the folder on future runs. We need to canonicalize
        // the path so we first wrap the directory string with a QDir.
        QDir directory(fd);
        Sandbox::createSecurityTokenForDir(directory);
        LineEditRecordings->setText(fd);
    }
}

void DlgPrefRecord::slotFormatChanged() {
    QObject *senderObj = sender();
    m_selFormat = EncoderFactory::getFactory().getFormatFor(senderObj->objectName());
    qDebug() << senderObj->objectName() << ": Format updated";

    const auto& formatName = m_selFormat.internalName;
    bool enableDefaultSampleRate = true;

    if (formatName == ENCODING_OPUS) {
        enableDefaultSampleRate = false; // OPUS never allows default
    } else if (m_defaultSampleRate > 48000 &&
            (formatName == ENCODING_MP3 || formatName == ENCODING_OGG)) {
        enableDefaultSampleRate = false; // MP3/OGG at high rates
        qDebug() << "mp3 or ogg format with 96khz samplerate, disallowed.";
    }
    RadioButtonUseDefaultSampleRate->setEnabled(enableDefaultSampleRate);

    // create format-specific sample rate set.
    if (formatName == ENCODING_OPUS) {
        m_bUseOpusEncoder = true;
        m_useEngineSampleRate.set(false);
        // select custom sample rate for OPUS
        RadioButtonUseCustomSampleRate->setChecked(true);
        recSampleRateComboBox->setEnabled(true);
        updateSampleRates(kRecSampleRatesOpus, m_oldRecSampleRateOpus);
    } else if (formatName == ENCODING_MP3) {
        m_bUseMP3Encoder = true;
        updateSampleRates(kRecSampleRatesMP3, m_oldRecSampleRateMP3);
    } else {
        updateSampleRates(kRecSampleRates, m_oldRecSampleRate);
    }

    setupEncoderUI();
}

// Update the set of custom sample rates offered in
// the recording combo-box.
void DlgPrefRecord::updateSampleRates(
        const QList<mixxx::audio::SampleRate>& sampleRates,
        double oldSampleRate) {
    // need to disconnect so clear() does not trigger
    // the slot
    qDebug() << "samplerate list being updated, default = " << m_defaultSampleRate;
    recSampleRateComboBox->blockSignals(true);

    recSampleRateComboBox->clear();
    for (const auto& sampleRate : sampleRates) {
        if (sampleRate.isValid()) {
            if (sampleRate.value() == m_defaultSampleRate) {
                continue;
            }
            recSampleRateComboBox->addItem(tr("%1 Hz").arg(sampleRate.value()),
                    QVariant::fromValue(sampleRate.value()));
        }
    }
    int recSampleRateIdx = recSampleRateComboBox->findData(
            QVariant::fromValue(oldSampleRate));

    recSampleRateComboBox->blockSignals(false);

    if (recSampleRateIdx != -1) {
        recSampleRateComboBox->setCurrentIndex(recSampleRateIdx);
    }
}

void DlgPrefRecord::setupEncoderUI() {
    EncoderRecordingSettingsPointer settings =
            EncoderFactory::getFactory().getEncoderRecordingSettings(
                    m_selFormat, m_pConfig);
    if (settings->usesQualitySlider()) {
        qualityGroup->setVisible(true);
        SliderQuality->setMinimum(0);
        SliderQuality->setMaximum(settings->getQualityValues().size()-1);
        SliderQuality->setValue(settings->getQualityIndex());
        updateTextQuality();
    } else {
        qualityGroup->setVisible(false);
    }
    if (settings->usesCompressionSlider()) {
        compressionGroup->setVisible(true);
        SliderCompression->setMinimum(0);
        SliderCompression->setMaximum(settings->getCompressionValues().size()-1);
        SliderCompression->setValue(settings->getCompression());
        updateTextCompression();
    } else {
        compressionGroup->setVisible(false);
    }

    for (QAbstractButton* widget : std::as_const(m_optionWidgets)) {
        optionsgroup.removeButton(widget);
        OptionGroupsLayout->removeWidget(widget);
        disconnect(widget, &QAbstractButton::clicked, this, &DlgPrefRecord::slotGroupChanged);
        widget->deleteLater();
    }
    m_optionWidgets.clear();
    if (!settings->getOptionGroups().isEmpty()) {
        labelOptionGroup->setVisible(true);
        // TODO (XXX): Right now i am supporting just one optiongroup.
        // The concept is already there for multiple groups
        // It will require to generate the buttongroup dynamically like:
        // >> buttongroup = new QButtonGroup(this);
        // >> buttongroup->addButton(radioButtonNoFFT);
        // >> buttongroup->addButton(radioButtonFFT);

        EncoderSettings::OptionsGroup group = settings->getOptionGroups().first();
        labelOptionGroup->setText(group.groupName);
        int controlIdx = settings->getSelectedOption(group.groupCode);
        for (const QString& name : std::as_const(group.controlNames)) {
            QAbstractButton* widget;
            if (group.controlNames.size() == 1) {
                QCheckBox* button = new QCheckBox(name, this);
                widget = button;
            } else {
                QRadioButton* button = new QRadioButton(name, this); // qradiobutton
                widget = button;
            }
            connect(widget, &QAbstractButton::clicked, this, &DlgPrefRecord::slotGroupChanged);
            widget->setObjectName(group.groupCode);
            OptionGroupsLayout->addWidget(widget);
            optionsgroup.addButton(widget);
            m_optionWidgets.append(widget);
            if (controlIdx == 0 ) {
                widget->setChecked(true);
            }
            controlIdx--;
        }
    } else {
        labelOptionGroup->setVisible(false);
    }
        // small hack for VBR
    if (m_selFormat.internalName == ENCODING_MP3) {
        updateTextQuality();
    }
}

void DlgPrefRecord::slotSliderQuality() {
    updateTextQuality();
    // Settings are only stored when doing an apply so that "cancel" can actually cancel.
}

void DlgPrefRecord::updateTextQuality() {
    EncoderRecordingSettingsPointer settings =
            EncoderFactory::getFactory().getEncoderRecordingSettings(
                    m_selFormat, m_pConfig);
    int quality;
    // This should be handled somehow by the EncoderSettings classes, but currently
    // I don't have a clean way to do it
    bool isVbr = false;
    if (m_selFormat.internalName == ENCODING_MP3) {
        EncoderSettings::OptionsGroup group = settings->getOptionGroups().first();
        for (const QAbstractButton* widget : std::as_const(m_optionWidgets)) {
            if (widget->objectName() == group.groupCode) {
                if (widget->isChecked() != Qt::Unchecked && widget->text() == "VBR") {
                    isVbr = true;
                }
            }
        }
    }
    if (isVbr) {
        EncoderMp3Settings* settingsmp3 = static_cast<EncoderMp3Settings*>(&(*settings));
        quality = settingsmp3->getVBRQualityValues().at(SliderQuality->value());
        TextQuality->setText(QString(QString::number(quality) + " kbit/s"));
    } else {
        quality = settings->getQualityValues().at(SliderQuality->value());
        TextQuality->setText(QString(QString::number(quality) + " kbit/s"));
    }
}

void DlgPrefRecord::slotSliderCompression() {
    updateTextCompression();
    // Settings are only stored when doing an apply so that "cancel" can actually cancel.
}

void DlgPrefRecord::updateTextCompression() {
    EncoderRecordingSettingsPointer settings =
            EncoderFactory::getFactory().getEncoderRecordingSettings(
                    m_selFormat, m_pConfig);
    int quality = settings->getCompressionValues().at(SliderCompression->value());
    TextCompression->setText(QString::number(quality));
}

void DlgPrefRecord::slotGroupChanged()
{
    // On complex scenarios, one could want to enable or disable some controls when changing
    // these, but we don't have these needs now.
    EncoderRecordingSettingsPointer settings =
            EncoderFactory::getFactory().getEncoderRecordingSettings(
                    m_selFormat, m_pConfig);
    if (settings->usesQualitySlider()) {
        updateTextQuality();
    }
}

void DlgPrefRecord::loadMetaData() {
    LineEditTitle->setText(m_pConfig->getValueString(ConfigKey(RECORDING_PREF_KEY, "Title")));
    LineEditAuthor->setText(m_pConfig->getValueString(ConfigKey(RECORDING_PREF_KEY, "Author")));
    LineEditAlbum->setText(m_pConfig->getValueString(ConfigKey(RECORDING_PREF_KEY, "Album")));
}

void DlgPrefRecord::saveRecordingFolder() {
    QString newPath = LineEditRecordings->text();
    if (newPath != m_pConfig->getValueString(ConfigKey(RECORDING_PREF_KEY, "Directory"))) {
        QFileInfo fileInfo(newPath);
        if (!fileInfo.exists()) {
            QMessageBox::warning(
                    this,
                    tr("Recordings directory invalid"),
                    tr("Recordings directory must be set to an existing directory."));
            return;
        }
        if (!fileInfo.isDir()) {
            QMessageBox::warning(
                    this,
                    tr("Recordings directory invalid"),
                    tr("Recordings directory must be set to a directory."));
            return;
        }
        if (!fileInfo.isWritable()) {
            QMessageBox::warning(this,
                    tr("Recordings directory not writable"),
                    tr("You do not have write access to %1. Choose a "
                       "recordings directory you have write access to.")
                            .arg(newPath));
            return;
        }
        m_pConfig->set(ConfigKey(RECORDING_PREF_KEY, "Directory"), newPath);
    }
}

void DlgPrefRecord::saveMetaData() {
    m_pConfig->set(ConfigKey(RECORDING_PREF_KEY, "Title"), ConfigValue(LineEditTitle->text()));
    m_pConfig->set(ConfigKey(RECORDING_PREF_KEY, "Author"), ConfigValue(LineEditAuthor->text()));
    m_pConfig->set(ConfigKey(RECORDING_PREF_KEY, "Album"), ConfigValue(LineEditAlbum->text()));
}

void DlgPrefRecord::saveEncoding() {
    EncoderRecordingSettingsPointer settings =
            EncoderFactory::getFactory().getEncoderRecordingSettings(
                    m_selFormat, m_pConfig);
    m_pConfig->set(ConfigKey(RECORDING_PREF_KEY, "Encoding"),
        ConfigValue(m_selFormat.internalName));

    if (settings->usesQualitySlider()) {
        settings->setQualityByIndex(SliderQuality->value());
    }
    if (settings->usesCompressionSlider()) {
        QList<int> comps = settings->getCompressionValues();
        settings->setCompression(comps.at(SliderCompression->value()));
    }
    if (!settings->getOptionGroups().isEmpty()) {
        // TODO (XXX): Right now i am supporting just one optiongroup.
        // The concept is already there for multiple groups
        EncoderSettings::OptionsGroup group = settings->getOptionGroups().first();
        int i=0;
        for (const QAbstractButton* widget : std::as_const(m_optionWidgets)) {
            if (widget->objectName() == group.groupCode) {
                if (widget->isChecked() != Qt::Unchecked) {
                    settings->setGroupOption(group.groupCode, i);
                    break;
                }
                i++;
            }
        }
    }
}

// Set 'Enable File Annotation in CUE file' checkbox value depending on 'Create
// a CUE file' checkbox value
void DlgPrefRecord::slotToggleCueEnabled() {
    CheckBoxUseCueFileAnnotation->setEnabled(CheckBoxRecordCueFile
                    ->isChecked());
}

void DlgPrefRecord::saveUseCueFile() {
    m_pConfig->set(ConfigKey(RECORDING_PREF_KEY, "CueEnabled"),
                   ConfigValue(CheckBoxRecordCueFile->isChecked()));
}

void DlgPrefRecord::saveUseCueFileAnnotation() {
    m_pConfig->set(ConfigKey(RECORDING_PREF_KEY, "cue_file_annotation_enabled"),
            ConfigValue(CheckBoxUseCueFileAnnotation->isChecked()));
}

void DlgPrefRecord::saveSplitSize() {
    m_pConfig->set(ConfigKey(RECORDING_PREF_KEY, "FileSize"),
                   ConfigValue(comboBoxSplitting->currentText()));
}

// only when recsamplerate combobox is chosen, i.e.
// custom recording samplerate.
void DlgPrefRecord::slotSampleRateChanged(int newRateIdx) {
    if (m_useEngineSampleRate.toBool()) {
        qDebug() << "Rec samplerate set changed, but using default (engine) samplerate.";
        return;
    }
    // addItem() is storing samplerate double values.
    auto recSampleRateNew = recSampleRateComboBox->itemData(newRateIdx).value<double>();

    qDebug() << "rec samplerate change triggered, new rate: " << recSampleRateNew;
    recSampleRateComboBox->setCurrentIndex(newRateIdx);

    if (m_bUseOpusEncoder) {
        m_bUseOpusEncoder = false;
        m_oldRecSampleRateOpus = recSampleRateNew;
    } else if (m_bUseMP3Encoder) {
        m_bUseMP3Encoder = false;
        m_oldRecSampleRateMP3 = recSampleRateNew;
    } else {
        m_oldRecSampleRate = m_recSampleRate.get();
    }

    // indicate that custom samplerate is being used
    // boolean flag that is visible to the enginerecord
    // and this class.
    // - in slotapply(), saverec only if boolean flag is
    // set, i.e. a custom samplerate was selected.
    // --
    // in enginerecord updateprefs(), have an if statement
    // to set m_samplerate to recording samplerate or default.
    m_useEngineSampleRate.set(false);
}

void DlgPrefRecord::slotDefaultSampleRateUpdated(mixxx::audio::SampleRate newRate) {
    qDebug() << "Engine samplerate changed, rec dlg received sample rate update:" << newRate;
    RadioButtonUseDefaultSampleRate->setText(QString("Default (%1 Hz)").arg(newRate.toDouble()));
    double rate = newRate.toDouble();

    if (rate > 48000 &&
            (m_selFormat.internalName == ENCODING_MP3 ||
                    m_selFormat.internalName == ENCODING_OGG)) {
        // gray out default samplerate button.
        RadioButtonUseDefaultSampleRate->setEnabled(false);
    } else {
        // enable default samplerate button.
        RadioButtonUseDefaultSampleRate->setEnabled(true);
    }

    m_defaultSampleRate = rate;
    if (m_selFormat.internalName == ENCODING_OPUS) {
        updateSampleRates(kRecSampleRatesOpus, m_oldRecSampleRateOpus);
    } else if (m_selFormat.internalName == ENCODING_MP3) {
        updateSampleRates(kRecSampleRatesMP3, m_oldRecSampleRateMP3);
    } else {
        updateSampleRates(kRecSampleRates, m_oldRecSampleRate);
    }
}
