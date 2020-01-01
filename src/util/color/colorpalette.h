#pragma once

#include <QColor>
#include <QList>

class ColorPalette {
  public:
    ColorPalette(QList<QRgb> colorList)
            : m_colorList(colorList) {
    }

    QColor at(int i) const {
        return m_colorList.at(i);
    }

    int size() const {
        return m_colorList.size();
    }

    int indexOf(QRgb color) const {
        return m_colorList.indexOf(color);
    }

    QList<QRgb>::const_iterator begin() const {
        return m_colorList.begin();
    }

    QList<QRgb>::const_iterator end() const {
        return m_colorList.end();
    }

    static const ColorPalette mixxxPalette;

    QList<QRgb> m_colorList;
};

inline bool operator==(
        const ColorPalette& lhs, const ColorPalette& rhs) {
    return lhs.m_colorList == rhs.m_colorList;
}
