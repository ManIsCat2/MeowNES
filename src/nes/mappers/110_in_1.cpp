#include "110_in_1.hpp"
#include "../nes_cpu.hpp"
#include "../nes_ppu.hpp"

MultiCart110In1::MultiCart110In1() {

}

void MultiCart110In1::reset() {
    updateBanks();
}

void MultiCart110In1::cpuWrite(uint16_t addr, uint8_t value) {
    if (addr >= 0x8000) {
		updateBanks(addr);
        return;
    }
    MapperBase::cpuWrite(addr, value);
}

const char *MultiCart110In1::getName(void) {
    return "110 in 1";
}

void MultiCart110In1::updateBanks(uint16_t val) {
    uint8_t bankHi = (val >> 8) & 0x40;
    uint8_t chrBank = bankHi | (val & 0x3F);
    uint8_t prgBank = bankHi | ((val >> 6) & 0x3F);

    setCHRBank(0, chrBank);

    if ((val & 0x1000) == 0) {
        setPRGBank(0, prgBank & ~1);
        setPRGBank(1, prgBank | 1);
    } else {
        setPRGBank(0, prgBank);
        setPRGBank(1, prgBank);
    }

    ppu->Mirroring = (val & 0x2000) ? MirrorMode::HORIZONTAL : MirrorMode::VERTICAL;
}