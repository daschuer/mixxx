#ifndef PollingControlProxy_H
#define PollingControlProxy_H

#include <QSharedPointer>
#include <QString>

#include "control/control.h"

namespace {
    const ConfigKey kNullKey; // because we return it as a reference
}

// this is the light version of a control proxy without the QObject overhead.
// this should be used when no signal connections are used.
// It is basically a PIMPL version of a ControlDoublePrivate Shared pointer
class PollingControlProxy {
  public:
    PollingControlProxy() {
    }

    PollingControlProxy(const QString& g, const QString& i) {
        initialize(ConfigKey(g, i));
    }

    PollingControlProxy(const ConfigKey& key) {
        initialize(key);
    }

    void initialize(const ConfigKey& key) {
        // Don't bother looking up the control if key is NULL. Prevents log spew.
        if (!key.isNull()) {
            m_pControl = ControlDoublePrivate::getControl(key, true);
        }
    }

    inline bool valid() const { return m_pControl != NULL; }

    // Returns the value of the object. Thread safe, non-blocking.
    inline double get() const {
        return m_pControl ? m_pControl->get() : 0.0;
    }

    // Returns the bool interpretation of the value
    inline bool toBool() const {
        return get() > 0.0;
    }

    // Returns the parameterized value of the object. Thread safe, non-blocking.
    inline double getParameter() const {
        return m_pControl ? m_pControl->getParameter() : 0.0;
    }

    // Returns the parameterized value of the object. Thread safe, non-blocking.
    inline double getParameterForValue(double value) const {
        return m_pControl ? m_pControl->getParameterForValue(value) : 0.0;
    }

    // Returns the normalized parameter of the object. Thread safe, non-blocking.
    inline double getDefault() const {
        return m_pControl ? m_pControl->defaultValue() : 0.0;
    }

    // Sets the control value to v. Thread safe, non-blocking.
    void set(double v) {
        if (m_pControl) {
            m_pControl->set(v, nullptr);
        }
    }
    // Sets the control parameterized value to v. Thread safe, non-blocking.
    void setParameter(double v) {
        if (m_pControl) {
            m_pControl->setParameter(v, nullptr);
        }
    }

    inline const ConfigKey& getKey() {
        return m_pControl ?  m_pControl->getKey() : kNullKey;
    }

    void reset() {
        if (m_pControl) {
            m_pControl->reset();
        }
    }

    void clear() {
        m_pControl.clear();
    }

  private:
    QSharedPointer<ControlDoublePrivate> m_pControl;
};

#endif // CONTROLPROXYRO_H
