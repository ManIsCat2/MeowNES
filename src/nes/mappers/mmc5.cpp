#include "mmc5.hpp"
#include "../nes_cpu.hpp"
#include "../nes_ppu.hpp"
#include "../nes_rom.hpp"

#include <cstring>

MMC5::MMC5() {

}

void MMC5::reset() {
    prgMode = 3;
    chrMode = 3;

    memset(prgRegs, 0, sizeof(prgRegs));
    memset(chrRegs, 0, sizeof(chrRegs));

    prgRegs[4] = 0xFF;

    nametableMap[0] = 0;
    nametableMap[1] = 0;
    nametableMap[2] = 0;
    nametableMap[3] = 0;

    fillTile = 0;
    fillAttr = 0;

    irqEnabled = false;
    irqScanline = 0;
    irqPending = false;
    currentScanline = 0;

    mulA = 0;
    mulB = 0;

    chrUpperBits = 0;

    EXRAMMode = 0;

    memset(EXRAM, 0, sizeof(EXRAM));

    updatePRG();
}

const char *MMC5::getName() {
    return "MMC5";
}

uint8_t MMC5::cpuRead(uint16_t addr) {
    if (addr >= 0x5C00 && addr <= 0x5FFF) {
        switch (EXRAMMode) {
            case 0:
            case 1:
            case 2:
                return EXRAM[addr & 0x3FF];

            case 3:
                return cpu->dataBus;
        }
    }

    switch (addr) {
        case 0x5204: {
            uint8_t val = 0;

            if (irqPending) val |= 0x80;
            if (ppu->ScanLine >= 0 && ppu->ScanLine < 240) val |= 0x40;

            irqPending = false;
            cpu->setExternalIRQ(false);

            return val;
        }

        case 0x5205:
            return (mulA * mulB) & 0xFF;

        case 0x5206:
            return ((mulA * mulB) >> 8) & 0xFF;
    }

    return MapperBase::cpuRead(addr);
}

void MMC5::cpuWrite(uint16_t addr, uint8_t value) {
    if (addr >= 0x5C00 && addr <= 0x5FFF) {
        switch (EXRAMMode) {
            case 0:
            case 1:
                EXRAM[addr & 0x3FF] = value;
                break;

            case 2:
                EXRAM[addr & 0x3FF] = value;
                break;

            case 3:
                break;
        }

        return;
    }

    switch (addr) {
        case 0x5100:
            prgMode = value & 0x03;
            break;

        case 0x5101:
            chrMode = value & 0x03;
            break;

        case 0x5104:
            EXRAMMode = value & 0x03;
            break;

        case 0x5105:
            nametableMap[0] = (value >> 0) & 0x03;
            nametableMap[1] = (value >> 2) & 0x03;
            nametableMap[2] = (value >> 4) & 0x03;
            nametableMap[3] = (value >> 6) & 0x03;
            break;

        case 0x5106:
            fillTile = value;
            break;

        case 0x5107:
            fillAttr = value & 0x03;
            break;

        case 0x5113:
        case 0x5114:
        case 0x5115:
        case 0x5116:
        case 0x5117:
            prgRegs[addr - 0x5113] = value;
            updatePRG();
            break;

        case 0x5120:
        case 0x5121:
        case 0x5122:
        case 0x5123:
        case 0x5124:
        case 0x5125:
        case 0x5126:
        case 0x5127:
            chrRegs[addr - 0x5120] = (chrUpperBits << 8) | value;
            updateCHR(true);
            break;
        case 0x5128:
        case 0x5129:
        case 0x512A:
        case 0x512B:
            chrRegs[addr - 0x5120] = (chrUpperBits << 8) | value;
            updateCHR(false);
            break;

        case 0x5130:
            chrUpperBits = value & 0x03;
            break;

        case 0x5203:
            irqScanline = value;
            break;

        case 0x5204:
            irqEnabled = (value & 0x80) != 0;

            if (!irqEnabled) {
                cpu->setExternalIRQ(false);
            }
            break;

        case 0x5205:
            mulA = value;
            break;

        case 0x5206:
            mulB = value;
            break;
        default:
            MapperBase::cpuWrite(addr, value);
            break;
    }
}

void MMC5::updatePRG() {
    auto mapPRGBank8K = [&](int slot, uint8_t reg, bool forceROM=false) {
        bool isROM = forceROM || (reg & 0x80);
        uint8_t bank = reg & 0x7F;
        uint16_t cpuAddr = 0x8000 + (slot * 0x2000);

        if (isROM) {
            mapCPUMemory(cpuAddr, cpuAddr + 0x1FFF, getNESRom()->ROM, (bank * 0x2000) % getNESRom()->PRGRomSize, false);
        } else {
            mapCPUMemory(cpuAddr, cpuAddr + 0x1FFF, getNESRom()->hasBattery ? SRAM : PRGRam, ((bank & 0x0F) * 0x2000) % getSRAMSize(), true);
        }
    };

    mapCPUMemory(0x6000, 0x7FFF, getNESRom()->hasBattery ? SRAM : PRGRam, (prgRegs[0] & 0x07) * 0x2000, true);

    switch (prgMode) {
        case 0: {
            uint8_t bank = (prgRegs[4] & 0x7C);
            mapCPUMemory(0x8000, 0xFFFF, getNESRom()->ROM, ((bank >> 2) * 0x8000) % getNESRom()->PRGRomSize, false);
            break;
        }

        case 1: {
            bool isROM = prgRegs[2] & 0x80;
            uint8_t bank = prgRegs[2] & 0x7E;

            if (isROM) {
                mapCPUMemory(0x8000, 0xBFFF, getNESRom()->ROM, ((bank >> 1) * 0x4000) % getNESRom()->PRGRomSize, false);
            } else {
                mapCPUMemory(0x8000, 0xBFFF, getNESRom()->hasBattery ? SRAM : PRGRam, (((bank >> 1) & 0x07) * 0x4000) % getSRAMSize(), true);
            }

            mapCPUMemory(0xC000, 0xFFFF, getNESRom()->ROM, (((prgRegs[4] & 0x7E) >> 1) * 0x4000) % getNESRom()->PRGRomSize, false);
            break;
        }

        case 2: {
            bool isROM = prgRegs[1] & 0x80;
            uint8_t bank = prgRegs[1] & 0x7E;

            if (isROM) {
                mapCPUMemory(0x8000, 0xBFFF, getNESRom()->ROM, ((bank >> 1) * 0x4000) % getNESRom()->PRGRomSize, false);
            } else {
                mapCPUMemory(0x8000, 0xBFFF, getNESRom()->hasBattery ? SRAM : PRGRam, (((bank >> 1) & 0x07) * 0x4000) % getSRAMSize(), true);
            }

            mapPRGBank8K(2, prgRegs[3]);
            mapPRGBank8K(3, prgRegs[4], true);
            break;
        }

        case 3:
            mapPRGBank8K(0, prgRegs[1]);
            mapPRGBank8K(1, prgRegs[2]);
            mapPRGBank8K(2, prgRegs[3]);
            mapPRGBank8K(3, prgRegs[4], true);
            break;
    }
}

void MMC5::updateCHR(bool sprite) {
    if (sprite) {
        switch (chrMode) {
            case 0: {
                uint16_t bank = chrRegs[7] << 3;

                for (int i = 0; i < 8; i++) {
                    setCHRBank(i, bank + i);
                }
                break;
            }

            case 1: {
                uint16_t bank0 = chrRegs[3] << 2;
                uint16_t bank1 = chrRegs[7] << 2;

                for (int i = 0; i < 4; i++) {
                    setCHRBank(i + 0, bank0 + i);
                    setCHRBank(i + 4, bank1 + i);
                }
                break;
            }

            case 2: {
                for (int i = 0; i < 4; i++) {
                    uint16_t bank = chrRegs[(i * 2) + 1] << 1;

                    setCHRBank(i * 2 + 0, bank + 0);
                    setCHRBank(i * 2 + 1, bank + 1);
                }
                break;
            }

            case 3: {
                for (int i = 0; i < 8; i++) {
                    setCHRBank(i, chrRegs[i]);
                }
                break;
            }
        }
    } else {
        switch (chrMode) {
            case 0: {
                uint16_t bank = chrRegs[11] << 3;

                for (int i = 0; i < 8; i++) {
                    setCHRBank(i, bank + i);
                }
                break;
            }

            case 1: {
                uint16_t bank0 = chrRegs[9] << 2;
                uint16_t bank1 = chrRegs[11] << 2;

                for (int i = 0; i < 4; i++) {
                    setCHRBank(i + 0, bank0 + i);
                    setCHRBank(i + 4, bank1 + i);
                }
                break;
            }

            case 2: {
                for (int i = 0; i < 4; i++) {
                    uint16_t bank = chrRegs[8 + (i * 2) + 1] << 1;

                    setCHRBank(i * 2 + 0, bank + 0);
                    setCHRBank(i * 2 + 1, bank + 1);
                }
                break;
            }

            case 3: {
                for (int i = 0; i < 4; i++) {
                    uint16_t bank = chrRegs[8 + i];

                    setCHRBank(i + 0, bank);
                    setCHRBank(i + 4, bank);
                }
                break;
            }
        }
    }
}

uint8_t MMC5::fillModeRead(uint16_t addr) {
    if ((addr & 0x03C0) == 0x03C0) {
        uint8_t attr = fillAttr & 0x03;
        return attr | (attr << 2) | (attr << 4) | (attr << 6);
    }

    return fillTile;
}

uint8_t MMC5::readVRAM(uint16_t addr) {
    addr &= 0x0FFF;

    uint8_t nt = (addr >> 10) & 0x03;
    uint16_t offs = addr & 0x03FF;

    switch (nametableMap[nt]) {
        case 0: return ppu->VRAM[offs];
        case 1: return ppu->VRAM[0x400 + offs];
        case 2:
            if (EXRAMMode == 3) return cpu->dataBus;
            return EXRAM[offs];
        case 3:
            return fillModeRead(offs);
    }

    return 0;
}

void MMC5::writeVRAM(uint16_t addr, uint8_t value) {
    addr &= 0x0FFF;

    uint8_t nt = (addr >> 10) & 0x03;
    uint16_t offs = addr & 0x03FF;

    switch (nametableMap[nt]) {
        case 0:
            ppu->VRAM[offs] = value;
            break;

        case 1:
            ppu->VRAM[0x400 + offs] = value;
            break;

        case 2:
            if (EXRAMMode <= 1) {
                EXRAM[offs] = 0;
            } else if (EXRAMMode == 2) {
                EXRAM[offs] = value;
            }
            break;

        case 3:
            break;
    }
}
void MMC5::clockCPU(void) {

}

void MMC5::clockPPU(void) {
    if (ppu->Dot == 0) {
        if (ppu->ScanLine >= 0 && ppu->ScanLine < 240) {
            if (ppu->mask.renderBackground || ppu->mask.renderSprites) {
                currentScanline++;
                if (irqScanline != 0 && currentScanline == irqScanline) {
                    irqPending = true;
                    if (irqEnabled) {
                        cpu->setExternalIRQ(true);
                    }
                }
            }
        } else if (ppu->ScanLine == 261) {
            currentScanline = 0;
            irqPending = false;
        }
    }
}

uint8_t MMC5::readCHR(uint16_t addr, bool sprite) {
    addr &= 0x1FFF;

    if (usingExtendedAttributes() && !sprite) {
        uint8_t ex = getEXRAMByte(ppu->VRAMAddr);
        uint32_t bank = (ex & 0x3F) | (chrUpperBits << 6);
        uint32_t finalAddr = (bank << 12) | (addr & 0x0FFF);
        return ppu->ChrData[finalAddr % getNESRom()->CHRRomSize];
    }

    return MapperBase::readCHR(addr, sprite);
}

bool MMC5::usingExtendedAttributes() {
    if (EXRAMMode != 1) {
        return false;
    }

    uint8_t nt = (ppu->VRAMAddr >> 10) & 0x03;

    return nametableMap[nt] == 2;
}

uint8_t MMC5::getEXRAMByte(uint16_t vramAddr) {
    uint16_t tileIndex = ((vramAddr >> 5) & 0x1F) * 32 + (vramAddr & 0x1F);

    return EXRAM[tileIndex];
}