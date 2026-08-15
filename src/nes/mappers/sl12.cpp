#include "sl12.hpp"
#include "../nes_cpu.hpp"
#include "../nes_ppu.hpp"

SL12::SL12() {

}

void SL12::reset() {
    mode = 0;

    MMC3Ctrl = 0;
    MMC3Mirroring = 0;
    IRQCounter = 0;
    IRQReloadVal = 0;
    IRQEnable = false;
    IRQReload = false;
    MMC3Regs[0] = 0;
    MMC3Regs[1] = 2;
    MMC3Regs[2] = 4;
    MMC3Regs[3] = 5;
    MMC3Regs[4] = 6;
    MMC3Regs[5] = 7;
    MMC3Regs[6] = -4;
    MMC3Regs[7] = -3;
    MMC3Regs[8] = -2;
    MMC3Regs[9] = -1;

    MMC1Buffer = 0;
    MMC1Shift = 0;
    MMC1Regs[0] = 0xc;
    MMC1Regs[1] = 0;
    MMC1Regs[2] = 0;
    MMC1Regs[3] = 0;

    VRC2Mirroring = 0;
    VRC2Regs[0] = -1;
    VRC2Regs[1] = -1;
    VRC2Regs[2] = -1;
    VRC2Regs[3] = -1;
    VRC2Regs[4] = 4;
    VRC2Regs[5] = 5;
    VRC2Regs[6] = 6;
    VRC2Regs[7] = 7;
    VRC2Regs[8] = 0;
    VRC2Regs[9] = 1;

    update();
}

void SL12::cpuWrite(uint16_t addr, uint8_t value) {
    if (addr >= 0x4100) {
        if (addr < 0x8000) {
			if ((addr & 0x4100) == 0x4100) {
				mode = value;
				if (addr & 0x01) {
					MMC1Regs[0] = 0xc;
					MMC1Regs[3] = 0;
					MMC1Buffer = 0;
					MMC1Shift = 0;
				}
				update();
			}
		} else {
			switch(mode & 0x03) {
				case 0: writeVRC2(addr, value); break;
				case 1: writeMMC3(addr, value); break;

				case 2:
				case 3: writeMMC1(addr, value); break;
			}
		}
        return;
    }
    MapperBase::cpuWrite(addr, value);
}

void SL12::updatePRG(void) {
    switch (mode & 0x03) {
		case 0:
			setPRGBank(0, VRC2Regs[8]);
			setPRGBank(1, VRC2Regs[9]);
			setPRGBank(2, -2);
			setPRGBank(3, -1);
			break;

		case 1: {
			uint32_t prgMode = (MMC3Ctrl >> 5) & 0x02;
			setPRGBank(0, MMC3Regs[6 + prgMode]);
			setPRGBank(1, MMC3Regs[7]);
			setPRGBank(2, MMC3Regs[6 + (prgMode ^ 0x02)]);
			setPRGBank(3, MMC3Regs[9]);
			break;
		}

		case 2:
		case 3: {
			uint8_t bank = MMC1Regs[3] & 0x0F;
			if (MMC1Regs[0] & 0x08) {
				if (MMC1Regs[0] & 0x04) {
					setPRGBank(0, bank << 1, BANK_2K);
					setPRGBank(1, 0x0F << 1, BANK_2K);
				} else {
					setPRGBank(0, 0, BANK_2K);
					setPRGBank(1, bank << 1, BANK_2K);
				}
			} else {
				setPRGBank(0, (bank & 0xFE) << 1, BANK_4K);
			}
			break;
		}
	}
}

void SL12::updateCHR(void) {
    uint32_t bank = (mode & 0x04) << 6;
	switch (mode & 0x03) {
		case 0:
			for (int i = 0; i < 8; i++) {
				setCHRBank(i, bank | VRC2Regs[i]);
			}
			break;

		case 1: {
			uint32_t slotSwap = (MMC3Ctrl & 0x80) ? 4 : 0;
			setCHRBank(0 ^ slotSwap, bank | ((MMC3Regs[0]) & 0xFE));
			setCHRBank(1 ^ slotSwap, bank | (MMC3Regs[0] | 1));
			setCHRBank(2 ^ slotSwap, bank | ((MMC3Regs[1]) & 0xFE));
			setCHRBank(3 ^ slotSwap, bank | (MMC3Regs[1] | 1));
			setCHRBank(4 ^ slotSwap, bank | MMC3Regs[2]);
			setCHRBank(5 ^ slotSwap, bank | MMC3Regs[3]);
			setCHRBank(6 ^ slotSwap, bank | MMC3Regs[4]);
			setCHRBank(7 ^ slotSwap, bank | MMC3Regs[5]);
			break;
		}

		case 2:
		case 3: {
			if (MMC1Regs[0] & 0x10) {
				setCHRBank(0, MMC1Regs[1] << 2, BANK_4K);
				setCHRBank(1, MMC1Regs[2] << 2, BANK_4K);
			} else {
				setCHRBank(0, (MMC1Regs[1] & 0xFE) << 2, BANK_8K);
			}
			break;
		}
	}
}

void SL12::updateMirroring(void) {
    switch (mode & 0x03) {
		case 0: ppu->Mirroring = (VRC2Mirroring & 0x01) ? MirrorMode::HORIZONTAL : MirrorMode::VERTICAL; break;
		case 1: ppu->Mirroring = (MMC3Mirroring & 0x01) ? MirrorMode::HORIZONTAL : MirrorMode::VERTICAL; break;
		case 2:
		case 3:
			switch (MMC1Regs[0] & 0x03) {
				case 0: ppu->Mirroring = MirrorMode::SCREEN_A; break;
				case 1: ppu->Mirroring = MirrorMode::SCREEN_B; break;
				case 2: ppu->Mirroring = MirrorMode::VERTICAL; break;
                case 3: ppu->Mirroring = MirrorMode::HORIZONTAL; break;
			}
			break;
	}
}

void SL12::update(void) {
    updatePRG();
    updateCHR();
    updateMirroring();
}

const char *SL12::getName(void) {
    return "SL12";
}

void SL12::writeVRC2(uint16_t addr, uint8_t value) {
    if (addr >= 0xB000 && addr <= 0xE003) {
        int id = ((((addr & 0x02) | (addr >> 0x0A)) >> 1) + 0x02) & 0x07;
        int nibble = ((addr & 1) << 2);
        VRC2Regs[id] = (VRC2Regs[id] & (0xF0 >> nibble)) | ((value & 0x0F) << nibble);
        updateCHR();
    } else {
        switch (addr & 0xF000) {
            case 0x8000: 
                VRC2Regs[8] = value;
                updatePRG();
            break;
            case 0xA000:
                VRC2Regs[9] = value;
                updatePRG();
            break;
            case 0x9000:
                VRC2Mirroring = value;
                updateMirroring();
            break;
        }
    }
}

void SL12::writeMMC3(uint16_t addr, uint8_t value) {
    switch(addr & 0xE001) {
        case 0x8000:
            MMC3Ctrl = value;
            update();
            break;
        
        case 0x8001:
            MMC3Regs[MMC3Ctrl & 0x07] = value;
            update();
            break;
        
        case 0xA000:
            MMC3Mirroring = value;
            update();
            break;
        
        case 0xC000: IRQReloadVal = value; break;
        case 0xC001: IRQReload = true; break;
        
        case 0xE000:
            cpu->setExternalIRQ(false);
            IRQEnable = false;
            break;
        
        case 0xE001: IRQEnable = true; break;
    }
}


void SL12::writeMMC1(uint16_t addr, uint8_t value) {
    if (value & 0x80) {
        MMC1Regs[0] |= 0xc;
        MMC1Buffer = MMC1Shift = 0;
        update();
    } else {
        uint8_t regIndex = (addr >> 13) - 4;
        MMC1Buffer |= (value & 0x01) << (MMC1Shift++);
        if (MMC1Shift == 5) {
            MMC1Regs[regIndex] = MMC1Buffer;
            MMC1Buffer = MMC1Shift = 0;
            update();
        }
    }
}

void SL12::clockPPU(void) {
    if ((ppu->ScanLine + 1) % 262 < 241 && ppu->Dot == 261 && mode & 0x03 == 1) {
        if (IRQCounter == 0 || IRQReload) {
            IRQCounter = IRQReloadVal;
        } else {
            IRQCounter--;
        }
        
        if (IRQCounter == 0 && IRQEnable) {
            cpu->setExternalIRQ(true);
            
        }
        IRQReload = false;
    }
}
