#pragma once

#include <stdint.h>
#include "../gb_bus.hpp"

class MBCBase : public HasGBBus {
public:
    struct MemPage {
        uint8_t *ptr = nullptr;
        bool write = false;
    };

    MemPage CPUPages[256];
    uint8_t *cartRAM = nullptr;
    uint8_t WRAM[0x8000];
    uint8_t HRAM[128];

    virtual ~MBCBase();
    virtual uint8_t cpuRead(uint16_t addr);
    virtual void cpuWrite(uint16_t addr, uint8_t value);
    virtual const char *getName(void) { return ""; }
    void mapCPUMemory(uint16_t start, uint16_t end, uint8_t* memory, uint32_t offset, bool writable, uint32_t size=0);
    void unmapCPUMemory(uint16_t start, uint16_t end);
    virtual void reset(void) {}

    void saveSRAM(void);
    void loadSRAM(void);

    void initialize(void);
};
