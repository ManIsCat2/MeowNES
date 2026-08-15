#pragma once

#include <QApplication>
#include "nes_cpu.hpp"

class Controller {
public:
    uint8_t state = 0;
    uint8_t shift = 0;
    bool strobe = false;
};

struct Keybind {
    const char *name;
    Qt::Key key;
};

extern Controller controllers[2];
extern Keybind nesKeyBinds[8];
extern Keybind nesKeyBindsDefault[8];