#include "cnrom.hpp"
#include "../nes_cpu.hpp"
#include "../nes_ppu.hpp"

CNROM::CNROM() {

}

void CNROM::reset() {
    CHRBank = 0;

    setPRGBank(0, 0);
    setCHRBank(0, 0);
}

void CNROM::cpuWrite(uint16_t addr, uint8_t value) {
    if (addr >= 0x8000) {
        CHRBank = value;
        setCHRBank(0, CHRBank);
        return;
    }
    MapperBase::cpuWrite(addr, value);
}

const char *CNROM::getName(void) {
    return "CNROM";
}