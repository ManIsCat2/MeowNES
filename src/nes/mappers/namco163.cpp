#include "namco163.hpp"
#include "../nes_cpu.hpp"
#include "../nes_ppu.hpp"

Namco163::Namco163() {

}

void Namco163::reset() {
    variant = NAMCO_163;
    writeProtect = 0;
    irqCounter = 0;
    setPRGBank(3, -1);
    updateWorkRam();
}

const char *Namco163::getName(void) {
    if (variant == Variant::NAMCO_175) return "Namco175";
    if (variant == Variant::NAMCO_340) return "Namco340";
    return "Namco163";
}

void Namco163::updateWorkRam() {
    uint8_t *memory = getNESRom()->hasBattery ? SRAM : PRGRam;
    if (variant == Variant::NAMCO_163) {
		bool WriteEnable = (writeProtect & 0x40) == 0x40;
		mapCPUMemory(0x6000, 0x67FF, memory, 0, WriteEnable && (writeProtect & 0x01) == 0x00);
		mapCPUMemory(0x6800, 0x6FFF, memory, 0, WriteEnable && (writeProtect & 0x02) == 0x00);
		mapCPUMemory(0x7000, 0x77FF, memory, 0, WriteEnable && (writeProtect & 0x04) == 0x00);
		mapCPUMemory(0x7800, 0x7FFF, memory, 0, WriteEnable && (writeProtect & 0x08) == 0x00);
	} else if (variant == Variant::NAMCO_175) {
		mapCPUMemory(0x6000, 0x7FFF, memory, 0, (writeProtect & 0x01) == 0x01);
	} else {
        unmapCPUMemory(0x6000, 0x7FFF);
    }
}

void Namco163::clockCPU(void) {
    if (irqCounter & 0x8000) {
        if ((irqCounter & 0x7FFF) != 0x7FFF) {
            irqCounter++;

            if ((irqCounter & 0x7FFF) == 0x7FFF) {
                cpu->setExternalIRQ(true);
            }
        }
    }
}

void Namco163::cpuWrite(uint16_t addr, uint8_t value) {
    switch(addr & 0xF800) {
        case 0x5000:
            irqCounter = (irqCounter & 0xFF00) | value;
            cpu->setExternalIRQ(false);
            break;

        case 0x5800:
            irqCounter = (irqCounter & 0x00FF) | (value << 8);
            cpu->setExternalIRQ(false);
            break;
        case 0x8000:
        case 0x8800:
        case 0x9000:
        case 0x9800: {
            uint8_t bank = (addr - 0x8000) >> 11;
            if (value >= 0xE0) {
                setCHRBank(bank, value & 1);
            } else {
                setCHRBank(bank, value);
            }
            break;
        }

        case 0xA000:
        case 0xA800:
        case 0xB000:
        case 0xB800: {
            uint8_t bank = ((addr - 0xA000) >> 11) + 4;
            if (value >= 0xE0) {
                setCHRBank(bank, value & 1);
            } else {
                setCHRBank(bank, value);
            }
            break;
        }
        case 0xC000:
        case 0xC800:
        case 0xD000:
        case 0xD800: {
            if (variant == NAMCO_175) {
                writeProtect = value;
                updateWorkRam();
            } else {
                uint8_t bank = ((addr - 0xC000) >> 11) + 8;

                if (value >= 0xE0)
                    setCHRBank(bank, value & 1);
                else
                    setCHRBank(bank, value);
            }
            break;
        }
        case 0xE000:
            setPRGBank(0, value & 0x3F);
            if (variant == NAMCO_340) {
                switch((value >> 6) & 3) {
                    case 0: ppu->Mirroring = MirrorMode::SCREEN_A; break;
                    case 1: ppu->Mirroring = MirrorMode::VERTICAL; break;
                    case 2: ppu->Mirroring = MirrorMode::HORIZONTAL; break;
                    case 3: ppu->Mirroring = MirrorMode::SCREEN_B; break;
                }
            }
            break;

        case 0xE800:
            setPRGBank(1, value & 0x3F);
            break;

        case 0xF000:
            setPRGBank(2, value & 0x3F);
            break;

        case 0xF800:
            writeProtect = value;
            updateWorkRam();
            break;
    }
    MapperBase::cpuWrite(addr, value);
}
