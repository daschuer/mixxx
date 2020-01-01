#pragma once

#include <QGridLayout>
#include <QPushButton>
#include <QMap>
#include <QWidget>
#include <QStyleFactory>

#include "util/color/color.h"
#include "util/color/colorpalette.h"

class WColorPicker : public QWidget {
    Q_OBJECT
  public:
    enum class ColorOption {
        DenyNoColor,
        AllowNoColor,
    };

    explicit WColorPicker(ColorOption colorOption, QWidget* parent = nullptr);

    void resetSelectedColor();
    void setSelectedColor(QRgb color);
    void useColorSet(const ColorPalette& palette);

  signals:
    void colorPicked(QRgb color);

  private slots:
    void slotColorPicked(const QColor& color);

  private:
    void addColorButton(const QColor& color, QGridLayout* pLayout, int row, int column);
    ColorOption m_colorOption;
    QRgb m_selectedColor;
    ColorPalette m_palette;
    QList<QPushButton*> m_colorButtons;
    QStyle* m_pStyle;
};
