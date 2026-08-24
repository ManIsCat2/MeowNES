#pragma once

#include <QApplication>
#include <QDialog>
#include <QPushButton>
#include <QGridLayout>
#include <QColorDialog>
#include <QEvent>

extern int hoveredPaletteIndex;

class PaletteButton : public QPushButton {
public:
    int index;

    PaletteButton(int i, QWidget *parent = nullptr) : QPushButton(parent), index(i) {
        setFixedSize(23, 23);
        setAttribute(Qt::WA_Hover);
    }

protected:
    void enterEvent(QEnterEvent* event) override {
        hoveredPaletteIndex = index;
      //  printf("uhh yea %u\n", hoveredPaletteIndex);
        QPushButton::enterEvent(event);
    }

    void leaveEvent(QEvent* event) override {
        hoveredPaletteIndex = -1;
        QPushButton::leaveEvent(event);
    }
};