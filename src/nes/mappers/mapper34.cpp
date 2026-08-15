#include "mapper34.hpp"
#include "../nes_cpu.hpp"
#include "../nes_ppu.hpp"

Mapper34::Mapper34(bool isNina01) : nina01(isNina01) {

}

void Mapper34::reset() {
    PRGBank0 = 0;
    CHRBank0 = 0;
    CHRBank1 = 0;

    setPRGBank(0, 0);
    if (!nina01) { 
        setCHRBank(0, 0);
    } else {
        mapCPUMemory(0x6000, 0x7fff, PRGRam, 0, true);
    }
}

void Mapper34::cpuWrite(uint16_t addr, uint8_t value) {
    if (!nina01) {
        if (addr >= 0x8000) {
            PRGBank0 = value;
            setPRGBank(0, PRGBank0);
            return;
        }
    } else {
        switch(addr) {
            case 0x7FFD: PRGBank0 = value & 0x01; setPRGBank(0, PRGBank0); return;
            case 0x7FFE: CHRBank0 = value & 0x0F; setCHRBank(0, CHRBank0); return;
            case 0x7FFF: CHRBank1 = value & 0x0F; setCHRBank(1, CHRBank1); return;
        }
    }
    MapperBase::cpuWrite(addr, value);
}

const char *Mapper34::getName(void) {
    return nina01 ? "Nina001" : "BNROM";
}