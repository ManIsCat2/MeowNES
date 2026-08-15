#pragma once
#include "mapper_base.hpp"
#include <stdint.h>
#include "../nes_ppu.hpp"

class Namco163 : public MapperBase {
public:
    Namco163();

    void cpuWrite(uint16_t addr, uint8_t value) override;
    void reset() override;
    const char *getName(void) override;

    uint16_t getCHRBankSize() override { return 0x400; }
    uint16_t getPRGBankSize() override { return 0x2000; }

    void clockCPU(void) override;
private:
    enum Variant {
        NAMCO_163,
        NAMCO_175,
        NAMCO_340,
        NAMCO_UNKNOWN
    };

    Variant variant;
    uint8_t writeProtect;
    uint16_t irqCounter;

    void updateWorkRam();
};
