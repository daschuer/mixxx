#include <QtDebug>

#include "control/controlproxy.h"

namespace {
    const ConfigKey kNullKey; // because we return it as a reference
}

ControlProxy::ControlProxy(QObject* pParent)
        : QObject(pParent) {
}

ControlProxy::ControlProxy(const QString& g, const QString& i, QObject* pParent)
        : QObject(pParent) {
    initialize(ConfigKey(g, i));
}

ControlProxy::ControlProxy(const ConfigKey& key, QObject* pParent)
        : QObject(pParent) {
    initialize(key);
}

void ControlProxy::initialize(const ConfigKey& key, bool warn) {
    // Don't bother looking up the control if key is NULL. Prevents log spew.
    if (!key.isNull()) {
        m_pControl = ControlDoublePrivate::getControl(key, warn);
    }
}

ControlProxy::~ControlProxy() {
    //qDebug() << "ControlProxy::~ControlProxy()";
}

const ConfigKey& ControlProxy::getKey() const {
    return m_pControl ? m_pControl->getKey() : kNullKey;
}
