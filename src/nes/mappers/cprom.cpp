#include "cprom.hpp"
#include "../nes_cpu.hpp"
#include "../nes_ppu.hpp"

CPROM::CPROM() {

}

void CPROM::reset() {
    setPRGBank(0, 0);
	setCHRBank(0, 0);
}

void CPROM::cpuWrite(uint16_t addr, uint8_t value) {
    if (addr >= 0x8000) {
        setCHRBank(1, value & 0x03);
        return;
    }
    MapperBase::cpuWrite(addr, value);
}

const char *CPROM::getName(void) {
    return "CPROM";
}