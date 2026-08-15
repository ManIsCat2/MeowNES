#pragma once

#include "../../savestate.hpp"
#include "../nes_bus.hpp"
#include <stdint.h>
#include <string>

class MapperBase : public HasNESBus {
public:
    enum BankSize {
        BANK_1K,
        BANK_2K,
        BANK_4K,
        BANK_8K,
        BANK_16K,
        BANK_32K,
    };
    struct MemPage {
        uint8_t *ptr = nullptr;
        bool write = false;
    };
    struct MemPage PRGPages[256];
    struct MemPage CHRPages[32];

    uint16_t subMapper = 0;

    uint8_t PRGRam[0x2000]; //or called WRAM
    uint8_t *SRAM = nullptr;

    virtual ~MapperBase();
    virtual uint8_t cpuRead(uint16_t addr);
    virtual void cpuWrite(uint16_t addr, uint8_t value);
    virtual uint8_t readCHR(uint16_t addr, bool sprite=false);
    virtual void writeCHR(uint16_t addr, uint8_t value);
    virtual uint8_t readVRAM(uint16_t addr);
    virtual void writeVRAM(uint16_t addr, uint8_t value);
    virtual const char *getName(void) { return ""; }
    virtual void reset() {}
    void initialize();

    virtual uint16_t getCHRBankSize() {
        return 0x2000;
    }
	virtual uint16_t getPRGBankSize() {
        return 0x4000;
    }
    virtual uint32_t getSRAMSize() { 
        return 0x2000;
    }

    virtual bool usingExtendedAttributes() { return false; };
    virtual bool hasExpansionAudio() { return false; }
    virtual double getExpansionAudioSample() { return 0.0; }

    virtual void clockCPU(void) {}
    virtual void clockPPU(void) {}
    virtual void saveState(SaveStateFile &s) { (void)s; }
    virtual void loadState(SaveStateFile &s) { (void)s; }

    void setCHRBank(uint16_t page, uint16_t val, enum BankSize size=BANK_1K);
    void setPRGBank(uint16_t page, uint16_t val, enum BankSize size=BANK_1K);

    void mapCPUMemory(uint16_t start, uint16_t end, uint8_t *memory, uint32_t offset, bool writable);
    void unmapCPUMemory(uint16_t start, uint16_t end);
    void mapPPUMemory(uint16_t start, uint16_t end, uint8_t *memory, uint32_t offset, bool writable);

    void saveSRAM();
    void loadSRAM();
};
