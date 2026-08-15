#include "mmc3.hpp"
#include "../nes_cpu.hpp"
#include "../nes_ppu.hpp"

MMC3::MMC3() {

}

void MMC3::reset() {
    BankSelect = 0;
    PrgMode = 0;
    ChrMode = 0;

    for (int i = 0; i < 8; i++) BankRegisters[i] = 0;

    IRQReload = 0;
    IRQCounter = 0;
    IRQEnabled = false;
    LastA12 = false;

    mapCPUMemory(0x6000, 0x7FFF, getNESRom()->hasBattery ? SRAM : PRGRam, 0, true);
    updatePRG();
    updateCHR();
}

void MMC3::cpuWrite(uint16_t addr, uint8_t value) {
    if (addr >= 0x8000 && addr <= 0x9FFF) {
        if ((addr & 1) == 0) {
            BankSelect = value;
            PrgMode = (value >> 6) & 1;
            ChrMode = (value >> 7) & 1;
        } else {
            uint8_t bankReg = BankSelect & 7;
            if (bankReg <= 1) {
                value &= 0xFE;
            }
            BankRegisters[bankReg] = value;
            updatePRG();
            updateCHR();
        }
    } else if (addr >= 0xA000 && addr <= 0xBFFF) {
        if ((addr & 1) == 0) {
            ppu->Mirroring = (value & 1) ? MirrorMode::HORIZONTAL : MirrorMode::VERTICAL;
        }
    } else if (addr >= 0xC000 && addr <= 0xDFFF) {
        if ((addr & 1) == 0) {
            IRQReload = value;
        } else {
            IRQCounter = 0;
        }
    } else if (addr >= 0xE000) {
        if ((addr & 1) == 0) {
            IRQEnabled = false;
            cpu->setExternalIRQ(false);
        } else {
            IRQEnabled = true;
        }
    } else {
        MapperBase::cpuWrite(addr, value);
    }
}

const char *MMC3::getName(void) {
    return "MMC3";
}

void MMC3::updatePRG() {
    if (PrgMode == 0) {
        setPRGBank(0, BankRegisters[6]);
		setPRGBank(1, BankRegisters[7]);
        setPRGBank(2, -2);
		setPRGBank(3, -1);
    } else {
        setPRGBank(0, -2);
        setPRGBank(1, BankRegisters[7]);
		setPRGBank(2, BankRegisters[6]);
        setPRGBank(3, -1);
    }
}

void MMC3::updateCHR() {
    if (ChrMode == 0) {
        setCHRBank(0, BankRegisters[0] & 0xFE);
		setCHRBank(1, BankRegisters[0] | 0x01);
		setCHRBank(2, BankRegisters[1] & 0xFE);
		setCHRBank(3, BankRegisters[1] | 0x01);
		setCHRBank(4, BankRegisters[2]);
		setCHRBank(5, BankRegisters[3]);
		setCHRBank(6, BankRegisters[4]);
		setCHRBank(7, BankRegisters[5]);
    } else if (ChrMode == 1) {
        setCHRBank(0, BankRegisters[2]);
		setCHRBank(1, BankRegisters[3]);
		setCHRBank(2, BankRegisters[4]);
		setCHRBank(3, BankRegisters[5]);
		setCHRBank(4, BankRegisters[0] & 0xFE);
		setCHRBank(5, BankRegisters[0] | 0x01);
		setCHRBank(6, BankRegisters[1] & 0xFE);
		setCHRBank(7, BankRegisters[1] | 0x01);
    }
}

// instead of checking for a12 rising edge... i do this
// it is a really shitty hack but it will work for most games
void MMC3::clockPPU(void) {
    if ((ppu->ScanLine + 1) % 262 < 241 && ppu->Dot == 261 && IRQEnabled && !IRQReload--) {
        cpu->setExternalIRQ(true);
    }
}