#pragma once
#include "mapper_base.hpp"
#include <stdint.h>
#include "../nes_ppu.hpp"

class MMC5 : public MapperBase {
public:
    MMC5();

    uint8_t cpuRead(uint16_t addr) override;
    void cpuWrite(uint16_t addr, uint8_t value) override;

    uint8_t readVRAM(uint16_t addr) override;
    void writeVRAM(uint16_t addr, uint8_t value) override;

    const char *getName(void) override;
    void reset() override;

    uint16_t getCHRBankSize() override {
        return 0x400;
    }

    uint16_t getPRGBankSize() override {
        return 0x2000;
    }

    uint32_t getSRAMSize() override {
        return 0x10000;
    }

    void clockCPU(void) override;
    void clockPPU(void) override;

    uint8_t readCHR(uint16_t addr, bool sprite=false) override;
    bool usingExtendedAttributes() override;
    uint8_t getEXRAMByte(uint16_t vramAddr);
private:
    uint8_t prgMode;
    uint8_t chrMode;

    uint8_t prgRegs[5];
    uint16_t chrRegs[12];

    uint8_t nametableMap[4];

    uint8_t fillTile;
    uint8_t fillAttr;

    bool irqEnabled;
    int irqScanline;
    bool irqPending;
    int currentScanline;

    uint8_t mulA;
    uint8_t mulB;

    uint8_t chrUpperBits;
    uint8_t EXRAMMode;

    uint8_t EXRAM[0x400];

    uint8_t fillModeRead(uint16_t addr);

    void updatePRG();
    void updateCHR(bool sprite);
};