#pragma once

#include "nes_cpu.hpp"

class Controller {
public:
    uint8_t state = 0;
    uint8_t shift = 0;
    bool strobe = false;
};

extern Controller controllers[2];

void UpdateControllers(void);