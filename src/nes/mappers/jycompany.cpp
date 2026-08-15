#include "jycompany.hpp"
#include "../nes_cpu.hpp"
#include "../nes_ppu.hpp"
#include <cstring>

JyCompany::JyCompany() {

}

void JyCompany::reset() {
    memset(prgRegs, 0, sizeof(prgRegs));
    memset(chrLowRegs, 0, sizeof(chrLowRegs));
    memset(chrHighRegs, 0, sizeof(chrHighRegs));
    chrLatch[0] = 0;
    chrLatch[1] = 4;

    prgMode = 0;
    enablePrgAt6000 = false;

    chrMode = 0;
    chrBlockMode = false;
    chrBlock = 0;
    mirrorChr = false;
    mirroringReg = 0;

    irqEnabled = false;
    irqSource = 0;
    irqCounter = 0;
    irqPrescaler = 0;
    irqXorReg = 0;
    irqFunkyModeReg = 0;
    irqCountDirection = 0;
	irqFunkyMode = false;
    irqSmallPrescaler = false;

    multiplyValue1 = 0;
    multiplyValue2 = 0;
    regRamValue = 0;

    updateState();
}

uint8_t JyCompany::cpuRead(uint16_t addr) {
    if (addr < 0x8000) {
        switch(addr & 0xF803) {
            case 0x5000: return 0;
            case 0x5800: return (multiplyValue1 * multiplyValue2) & 0xFF;
			case 0x5801: return ((multiplyValue1 * multiplyValue2) >> 8) & 0xFF;
            case 0x5803: return regRamValue;
        }
        if (addr >= 0x6000 && enablePrgAt6000) {
            return PRGRam[addr - 0x6000];
        }
    }
    return MapperBase::cpuRead(addr);
}

void JyCompany::cpuWrite(uint16_t addr, uint8_t value) {
    if (addr < 0x8000) {
        switch(addr & 0xF803) {
            case 0x5800: multiplyValue1 = value; break;
            case 0x5801: multiplyValue2 = value; break;
            case 0x5803: regRamValue = value; break;
        }
        if (addr >= 0x6000 && enablePrgAt6000) {
            PRGRam[addr - 0x6000] = value;
        }
    } else {
        switch(addr & 0xF007) {
            case 0x8000: case 0x8001: case 0x8002: case 0x8003:
                prgRegs[addr & 0x03] = value & 0x7F;
                break;
            case 0x9000: case 0x9001: case 0x9002: case 0x9003:
            case 0x9004: case 0x9005: case 0x9006: case 0x9007:
                chrLowRegs[addr & 0x07] = value;
                break;
            case 0xA000: case 0xA001: case 0xA002: case 0xA003:
            case 0xA004: case 0xA005: case 0xA006: case 0xA007:
                chrHighRegs[addr & 0x07] = value;;
                break;
            case 0xC000:
                irqEnabled = value & 0x01;
                if(!irqEnabled) cpu->setExternalIRQ(false);
                break;
            case 0xC001:
				irqCountDirection = (value >> 6) & 0x03;
				irqFunkyMode = (value & 0x08) == 0x08;
				irqSmallPrescaler = ((value >> 2) & 0x01) == 0x01;
				irqSource = (value & 0x03);
              //  DebugPrintLog("MAPPER", "IRQ Src %u", irqSource);
				break;
            case 0xC002:
				irqEnabled = false;
				cpu->setExternalIRQ(false);
				break;
			case 0xC003: irqEnabled = true; break;
            case 0xC004: irqPrescaler = value ^ irqXorReg; break;
            case 0xC005: irqCounter = value ^ irqXorReg; break;
            case 0xC006: irqXorReg = value; break;
            case 0xC007: irqFunkyModeReg = value; break;
            case 0xD000:
                prgMode = value & 0x07;
                chrMode = (value >> 3) & 0x03;
				advancedNtControl = (value & 0x20) == 0x20;
                enablePrgAt6000 = (value & 0x80) == 0x80;
                break;
            case 0xD001:
                mirroringReg = value & 0x03;
                break;

            case 0xD003:
                mirrorChr = (value & 0x80) != 0;
                chrBlockMode = (value & 0x20) == 0;
                chrBlock = ((value & 0x18) >> 2) | (value & 0x01);
                break;
        }
    }
    MapperBase::cpuWrite(addr, value);
    updateState();
}

const char *JyCompany::getName() {
    return "J.Y. Company";
}

void JyCompany::updatePrg() {
    switch(prgMode & 0x03) {
        case 0:
            setPRGBank(0, (prgMode & 0x04) ? prgRegs[3] : 0x3C, BANK_4K);
            break;
        case 1:
            setPRGBank(0, prgRegs[1] << 1, BANK_2K);
			setPRGBank(1, (prgMode & 0x04) ? prgRegs[3] : 0x3E, BANK_2K);
            break;
        case 2:
        case 3:
            setPRGBank(0, prgRegs[0]);
		    setPRGBank(1, prgRegs[1]);
			setPRGBank(2, prgRegs[2]);
			setPRGBank(3, (prgMode & 0x04) ? prgRegs[3] : 0x3F);
            break;
    }
}

uint16_t JyCompany::getChrReg(int index) {
    if (chrMode >= 2 && mirrorChr && (index == 2 || index == 3)) {
        index -= 2;
    }
    
    if (chrBlockMode) {
        uint8_t mask = 0;
        uint8_t shift = 0;
        switch(chrMode) {
            default:
            case 0: mask = 0x1F; shift = 5; break;
            case 1: mask = 0x3F; shift = 6; break;
            case 2: mask = 0x7F; shift = 7; break;
            case 3: mask = 0xFF; shift = 8; break;
        }
        return (chrLowRegs[index] & mask) | (chrBlock << shift);
    } else {
        return chrLowRegs[index] | (chrHighRegs[index] << 8);
    }
}

void JyCompany::updateCHR() {
    uint16_t chrRegs[8] = {
        getChrReg(0), getChrReg(1), getChrReg(2), getChrReg(3),
        getChrReg(4), getChrReg(5), getChrReg(6), getChrReg(7)
    };

    switch(chrMode) {
        case 0:
            setCHRBank(0, chrRegs[0] << 3, BANK_8K);
            break;

        case 1:
            setCHRBank(0, chrRegs[chrLatch[0]] << 2, BANK_4K);
            setCHRBank(1, chrRegs[chrLatch[1]] << 2, BANK_4K);
            break;

        case 2:
            setCHRBank(0, chrRegs[0] << 1, BANK_2K);
            setCHRBank(1, chrRegs[2] << 1, BANK_2K);
            setCHRBank(2, chrRegs[4] << 1, BANK_2K);
            setCHRBank(3, chrRegs[6] << 1, BANK_2K);
            break;

        case 3:
            for (int i = 0; i < 8; i++) {
                setCHRBank(i, chrRegs[i]);
            }
            break;
    }
}

void JyCompany::updateState() {
    updatePrg();
    updateCHR();
    updateMirroring();
}

void JyCompany::updateMirroring() {
    switch(mirroringReg) {
        case 0: ppu->Mirroring = MirrorMode::VERTICAL; break;
        case 1: ppu->Mirroring = MirrorMode::HORIZONTAL; break;
        case 2: ppu->Mirroring = MirrorMode::SCREEN_A; break;
        case 3: ppu->Mirroring = MirrorMode::SCREEN_B; break;
    }
}

void JyCompany::clockIRQ(void) {
    uint8_t mask = irqSmallPrescaler ? 0x07 : 0xFF;
    uint8_t correctPrescale = irqPrescaler & mask;
    bool doCount = false;
    if (irqCountDirection == 0x01) {
        correctPrescale++;
        if ((correctPrescale & mask) == 0) {
            doCount = true;
        }
    } else if (irqCountDirection == 0x02) {
        if (--correctPrescale == 0) {
            doCount = true;
        }
    }
    irqPrescaler = (irqPrescaler & ~mask) | (correctPrescale & mask);
        
    if (doCount) {
        if (irqCountDirection == 0x01) {
            irqCounter++;
            if (irqCounter == 0 && irqEnabled) {
                cpu->setExternalIRQ(true);
            }
        } else if (irqCountDirection == 0x02) {
            irqCounter--;
            if (irqCounter == 0xFF && irqEnabled) {
                cpu->setExternalIRQ(true);
            }
        }
    }
}

void JyCompany::clockCPU(void) {
    if (irqSource == 0) {
        clockIRQ();
    }
}

void JyCompany::clockPPU(void) {
    if (irqSource == 1) {
        //incredible
        // clockIRQ();
        if ((ppu->ScanLine + 1) % 262 < 241 && ppu->Dot == 261) {
            if (irqCountDirection == 0x01) {
                //irqCounter++;
                if (irqCounter++ == 0 && irqEnabled) {
                    cpu->setExternalIRQ(true);
                }
            } else if (irqCountDirection == 0x02) {
                //irqCounter--;
                if (irqCounter-- == 0xFF && irqEnabled) {
                    cpu->setExternalIRQ(true);
                }
            }
        }
    }
}