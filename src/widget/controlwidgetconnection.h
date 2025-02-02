#pragma once

#include <QMetaProperty>
#include <QObject>
#include <QString>
#include <memory>

#include "control/controlproxy.h"
#include "util/parented_ptr.h"

class WBaseWidget;
class ValueTransformer;

class ControlWidgetConnection : public QObject {
    Q_OBJECT
  public:
    ControlWidgetConnection(WBaseWidget* pBaseWidget,
            const ConfigKey& key,
            std::unique_ptr<ValueTransformer> pTransformer);
    ~ControlWidgetConnection() override;

    double getControlParameter() const;
    double getControlParameterForValue(double value) const;

    const ConfigKey& getKey() const {
        return m_pControl->getKey();
    }

    virtual QString toDebugString() const = 0;

  protected slots:
    virtual void slotControlValueChanged(double v) = 0;

  protected:
    void setControlParameter(double parameter);

    WBaseWidget* m_pWidget;

    // This ControlProxys is created as parent to this and deleted by
    // the Qt object tree. This helps that they are deleted by the creating
    // thread, which is required to avoid segfaults.
    parented_ptr<ControlProxy> m_pControl;

  private:
    std::unique_ptr<ValueTransformer> m_pValueTransformer;
};

class ControlParameterWidgetConnection final : public ControlWidgetConnection {
    Q_OBJECT
  public:
    enum EmitOption {
        EMIT_NEVER                = 0x00,
        EMIT_ON_PRESS             = 0x01,
        EMIT_ON_RELEASE           = 0x02,
        EMIT_ON_PRESS_AND_RELEASE = 0x03,
        EMIT_DEFAULT              = 0x04
    };

    static QString emitOptionToString(EmitOption option) {
        switch (option & EMIT_ON_PRESS_AND_RELEASE) {
            case EMIT_NEVER:
                return "NEVER";
            case EMIT_ON_PRESS:
                return "PRESS";
            case EMIT_ON_RELEASE:
                return "RELEASE";
            case EMIT_ON_PRESS_AND_RELEASE:
                return "PRESS_AND_RELEASE";
            default:
                return "UNKNOWN";
        }
    }

    enum DirectionOption {
        DIR_NON                  = 0x00,
        DIR_FROM_WIDGET          = 0x01,
        DIR_TO_WIDGET            = 0x02,
        DIR_FROM_AND_TO_WIDGET   = 0x03,
        DIR_DEFAULT              = 0x04
    };

    static QString directionOptionToString(DirectionOption option) {
        switch (option & DIR_FROM_AND_TO_WIDGET) {
            case DIR_NON:
                return "NON";
            case DIR_FROM_WIDGET:
                return "FROM_WIDGET";
            case DIR_TO_WIDGET:
                return "TO_WIDGET";
            case DIR_FROM_AND_TO_WIDGET:
                return "FROM_AND_TO_WIDGET";
            default:
                return "UNKNOWN";
        }
    }

    ControlParameterWidgetConnection(WBaseWidget* pBaseWidget,
            const ConfigKey& key,
            std::unique_ptr<ValueTransformer> pTransformer,
            DirectionOption directionOption,
            EmitOption emitOption);

    ~ControlParameterWidgetConnection() override;

    void Init();

    QString toDebugString() const override;

    int getDirectionOption() const { return m_directionOption; };
    int getEmitOption() const { return m_emitOption; };

    void setDirectionOption(enum DirectionOption v) { m_directionOption = v; };
    void setEmitOption(enum EmitOption v) { m_emitOption = v; };

    void resetControl();
    void setControlParameter(double v);
    void setControlParameterDown(double v);
    void setControlParameterUp(double v);

  private slots:
    void slotControlValueChanged(double value) override;

  private:
    DirectionOption m_directionOption;
    EmitOption m_emitOption;
};

class ControlWidgetPropertyConnection final : public ControlWidgetConnection {
    Q_OBJECT
  public:
    ControlWidgetPropertyConnection(WBaseWidget* pBaseWidget,
            const ConfigKey& key,
            std::unique_ptr<ValueTransformer> pTransformer,
            const QString& propertyName);

    ~ControlWidgetPropertyConnection() override;

    QString toDebugString() const override;

  private slots:
    void slotControlValueChanged(double v) override;

  private:
    const QString m_propertyName;
    const QMetaProperty m_property;
    QVariant m_propertyValue;
};
