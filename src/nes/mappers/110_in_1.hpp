#pragma once
#include "mapper_base.hpp"
#include <stdint.h>
#include "../nes_ppu.hpp"

class MultiCart110In1 : public MapperBase {
public:
    MultiCart110In1();

    void cpuWrite(uint16_t addr, uint8_t value) override;
    const char *getName(void) override;
    void reset() override;

    uint16_t getCHRBankSize() override {
        return 0x2000;
    }
    uint16_t getPRGBankSize() override {
        return 0x4000;
    }
private:
    void updateBanks(uint16_t val=0x8000);
};