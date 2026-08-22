#pragma once
#include "mapper_base.hpp"
#include <stdint.h>
#include "../nes_ppu.hpp"

class CPROM : public MapperBase {
public:
    CPROM();

    void cpuWrite(uint16_t addr, uint8_t value) override;
    const char *getName(void) override;
    void reset() override;

    uint16_t getCHRBankSize() override {
        return 0x1000;
    }
    uint16_t getPRGBankSize() override {
        return 0x8000;
    }
    uint16_t getCHRRamSize() override {
        return 0x4000;
    }
};