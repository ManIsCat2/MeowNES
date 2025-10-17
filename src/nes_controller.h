#pragma once

#include "nes_cpu.h"

class Controller {
public:
    uint8_t state = 0;
    uint8_t shift = 0;
    bool strobe = false;
};

extern Controller controllers[2];

void update_controllers(void);