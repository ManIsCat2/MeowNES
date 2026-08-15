#pragma once
#include "mapper_base.hpp"
#include <stdint.h>
#include "../nes_ppu.hpp"

class SunSoftFME7 : public MapperBase {
public:
    SunSoftFME7();

    void cpuWrite(uint16_t addr, uint8_t value) override;
    void reset() override;
    const char *getName(void) override;

    uint16_t getCHRBankSize() override {
        return 0x400;
    }
    uint16_t getPRGBankSize() override {
        return 0x2000;
    }
    uint32_t getSRAMSize() override {
        return 0x8000;
    }

    void clockCPU(void) override;
private:
    uint8_t command;
    uint8_t workRamValue;

    bool irqEnabled;
    bool irqCounterEnabled;
    uint16_t irqCounter; 

    void updateWRAM(void);
};