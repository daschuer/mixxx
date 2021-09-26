#pragma once

#include <QString>
#include <QUuid>

#include "util/assert.h"

///
/// Utility functions for QUuid
///

/// Format a UUID without enclosing curly braces.
inline QString uuidToStringWithoutBraces(const QUuid& uuid) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
    return uuid.toString(QUuid::WithoutBraces);
#else
    QString withBraces = uuid.toString();
    DEBUG_ASSERT(withBraces.size() == 38);
    DEBUG_ASSERT(withBraces.startsWith('{'));
    DEBUG_ASSERT(withBraces.endsWith('}'));
    // We need to strip off the heading/trailing curly braces after formatting
    return withBraces.mid(1, 36);
#endif
}

/// Format a UUID without enclosing curly braces, representing a null UUID
/// by an empty string instead of 00000000-0000-0000-0000-000000000000.
inline QString uuidToNullableStringWithoutBraces(const QUuid& uuid) {
    if (uuid.isNull()) {
        return QString{};
    } else {
        return uuidToStringWithoutBraces(uuid);
    }
}
