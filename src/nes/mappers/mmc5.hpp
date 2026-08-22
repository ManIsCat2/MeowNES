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

    uint8_t readCHR(uint16_t addr, bool sprite = false) override;
    uint8_t getEXRAMByte(uint16_t vramAddr);
private:
    uint8_t prgMode = 3;
    uint8_t chrMode = 3;

    uint8_t prgBanks[5] = {};
    uint16_t chrBanks[12] = {};

    uint8_t nameTableSource[4] = {};
    uint8_t fillPattern = 0;
    uint8_t fillPalette = 0;

    uint8_t ramProtectA = 0;
    uint8_t ramProtectB = 0;

    bool splitEnabled = false;
    bool splitRight = false;
    uint8_t splitBoundary = 0;
    uint8_t splitScrollValue = 0;
    uint8_t splitChrBank = 0;

    bool splitActive = false;
    uint16_t splitChrAddress = 0;
    int32_t splitTileIndex = 0;

    uint8_t multiplyA = 0;
    uint8_t multiplyB = 0;

    uint8_t chrHighBits = 0;
    uint8_t extraRamMode = 0;

    uint8_t extraRam[0x400] = {};

    uint16_t extAttrNameTableAddress = 0;
    int8_t extAttrCountdown = 0;
    uint8_t extAttrChrBank = 0;

    uint8_t scanlineCounter = 0;
    int fakeScanlineCount = 0;
    bool frameStartPending = false;
    bool renderingActive = false;
    uint8_t ppuIdleTicks = 0;
    uint16_t prevPPUAddr = 0;
    uint8_t repeatedNtReads = 0;

    uint16_t lastChrRegisterAddress = 0;

    uint8_t irqScanline = 0;
    bool irqEnabled = 0;
    bool irqPending = 0;

    void updatePRG();

    void trackPpuRead(uint16_t addr, bool isNametableData);
    void trackScanlineStart(uint16_t addr);

    bool isPrgRamWritable();
    uint8_t *getPrgRam(uint32_t &size);

    bool useChrA();
    uint32_t getChrAddress(uint16_t addr, bool sprite);

    uint8_t readCHRMem(uint32_t addr);
    uint8_t readFillMode(uint16_t addr);

    void mapCpuRange(uint16_t start, uint16_t end, uint8_t *memory, uint32_t memorySize, uint32_t offset, bool writable);
};