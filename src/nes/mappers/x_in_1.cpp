#include "x_in_1.hpp"
#include "../nes_cpu.hpp"
#include "../nes_ppu.hpp"

XIn1::XIn1() {

}

void XIn1::reset() {
    setPRGBank(0, 0);
	setPRGBank(1, 1);
	setCHRBank(0, 0);
}

void XIn1::cpuWrite(uint16_t addr, uint8_t value) {
    if (addr >= 0x8000) {
		uint8_t chr = ((addr & 0x1F) << 2) | (value & 0x03);
        uint8_t prg = ((addr & 0x3F00) >> 8) | (addr & 0x40);
		if (!(addr & 0x20)) {
			setPRGBank(0, prg & 0xFE);
			setPRGBank(1, (prg & 0xFE) + 1);
		} else {
			setPRGBank(0, prg);
			setPRGBank(1, prg);
		}

		setCHRBank(0, chr);

		ppu->Mirroring = addr & 0x80 ? MirrorMode::HORIZONTAL : MirrorMode::VERTICAL;
        return;
    }
    MapperBase::cpuWrite(addr, value);
}

const char *XIn1::getName(void) {
    return "X in 1";
}