#include "mmc5.hpp"
#include "../nes_cpu.hpp"
#include "../nes_ppu.hpp"
#include "../nes_rom.hpp"

#include <cstdint>
#include <cstring>


MMC5::MMC5(){

}

void MMC5::reset() {
    prgMode = 3;
    chrMode = 3;

    std::memset(prgBanks, 0, sizeof(prgBanks));
    std::memset(chrBanks, 0, sizeof(chrBanks));
    std::memset(extraRam, 0, sizeof(extraRam));

    prgBanks[4] = 0xFF;

    ramProtectA = 0;
    ramProtectB = 0;

    nameTableSource[0] = 0;
    nameTableSource[1] = 0;
    nameTableSource[2] = 0;
    nameTableSource[3] = 0;

    fillPattern = 0;
    fillPalette = 0;

    splitEnabled = false;
    splitRight = false;
    splitBoundary = 0;
    splitScrollValue = 0;
    splitChrBank = 0;

    splitActive = false;
    splitChrAddress = 0;
    splitTileIndex = 0;

    multiplyA = 0;
    multiplyB = 0;

    chrHighBits = 0;
    extraRamMode = 0;

    extAttrNameTableAddress = 0;
    extAttrCountdown = 0;
    extAttrChrBank = 0;

    frameStartPending = false;
    renderingActive = false;
    ppuIdleTicks = 0;
    prevPPUAddr = 0;
    repeatedNtReads = 0;

    lastChrRegisterAddress = 0;

    updatePRG();
}

const char *MMC5::getName(void) {
    return "MMC5";
}

static uint32_t wrapBankOffset(uint32_t offset, uint32_t size) {
    return size == 0 ? 0 : offset % size;
}

static uint8_t expand2Bit(uint8_t value) {
    value &= 0x03;
    return (uint8_t)(value | (value << 2) | (value << 4) | (value << 6));
}

static bool isNameTableAddress(uint16_t address) {
    return address >= 0x2000 && address <= 0x2FFF;
}

static bool isNameTableTileFetch(uint16_t address) {
    return isNameTableAddress(address) && (address & 0x03FF) < 0x03C0;
}

bool MMC5::isPrgRamWritable() {
    return ramProtectA == 0x02 && ramProtectB == 0x01;
}

uint8_t *MMC5::getPrgRam(uint32_t &size) {
    if (getNESRom()->hasBattery && SRAM) {
        size = getSRAMSize();
        return SRAM;
    }

    size = sizeof(PRGRam);
    return (uint8_t *)(PRGRam);
}

void MMC5::mapCpuRange(uint16_t start, uint16_t end, uint8_t *memory, uint32_t memorySize, uint32_t offset, bool writable) {
    uint16_t firstPage = start >> 8;
    uint16_t pageCount = (uint16_t)(((end - start) + 1) >> 8);

    for (uint16_t page = 0; page < pageCount; page++) {
        PRGPages[firstPage + page].ptr = memory + wrapBankOffset(offset + page * 0x100, memorySize);
        PRGPages[firstPage + page].write = writable;
    }
}

void MMC5::updatePRG() {
    NesROM *rom = getNESRom();
    uint32_t romSize = rom->PRGRomSize;

    uint32_t ramSize = 0;
    uint8_t *ram = getPrgRam(ramSize);
    bool writable = isPrgRamWritable();

    auto mapRom8K = [&](uint16_t cpuAddress, uint8_t bankRegister) {
        uint32_t offset = wrapBankOffset((bankRegister & 0x7F) * 0x2000, romSize);
        mapCpuRange(cpuAddress, cpuAddress + 0x1FFF, rom->ROM, romSize, offset, false);
    };

    auto mapRam8K = [&](uint16_t cpuAddress, uint8_t bankRegister) {
        uint32_t offset = wrapBankOffset((bankRegister & 0x07) * 0x2000, ramSize);
        mapCpuRange(cpuAddress, cpuAddress + 0x1FFF, ram, ramSize, offset, writable);
    };

    auto mapBank8K = [&](uint16_t cpuAddress, uint8_t bankRegister, bool forceRom) {
        if (forceRom || (bankRegister & 0x80))
            mapRom8K(cpuAddress, bankRegister);
        else
            mapRam8K(cpuAddress, bankRegister);
    };

    auto mapBank16K = [&](uint16_t cpuAddress, uint8_t bankRegister, bool forceRom) {
        uint8_t evenRegister = bankRegister & 0x7E;

        if (forceRom || (bankRegister & 0x80)) {
            uint32_t offset = wrapBankOffset(evenRegister * 0x2000, romSize);
            mapCpuRange(cpuAddress, cpuAddress + 0x3FFF, rom->ROM, romSize, offset, false);
        } else {
            uint32_t offset = wrapBankOffset((evenRegister & 0x07) * 0x2000, ramSize);
            mapCpuRange(cpuAddress, cpuAddress + 0x3FFF, ram, ramSize, offset, writable);
        }
    };

    auto mapBank32K = [&](uint16_t cpuAddress, uint8_t bankRegister) {
        uint8_t bank = bankRegister & 0x7C;
        uint32_t offset = wrapBankOffset(bank * 0x2000, romSize);
        mapCpuRange(cpuAddress, cpuAddress + 0x7FFF, rom->ROM, romSize, offset, false);
    };

    mapRam8K(0x6000, prgBanks[0]);

    switch (prgMode) {
        case 0:
            mapBank32K(0x8000, prgBanks[4]);
            break;

        case 1:
            mapBank16K(0x8000, prgBanks[2], false);
            mapBank16K(0xC000, prgBanks[4], true);
            break;

        case 2:
            mapBank16K(0x8000, prgBanks[2], false);
            mapBank8K(0xC000, prgBanks[3], false);
            mapBank8K(0xE000, prgBanks[4], true);
            break;

        case 3:
        default:
            mapBank8K(0x8000, prgBanks[1], false);
            mapBank8K(0xA000, prgBanks[2], false);
            mapBank8K(0xC000, prgBanks[3], false);
            mapBank8K(0xE000, prgBanks[4], true);
            break;
    }
}

bool MMC5::useChrA() {
    if (!ppu->control.use8x16Sprites) return true;

    return (splitTileIndex >= 32 && splitTileIndex < 40) || (!renderingActive && lastChrRegisterAddress >= 0x5120 && lastChrRegisterAddress <= 0x5127);
}

uint32_t MMC5::getChrAddress(uint16_t address, bool sprite) {
    address &= 0x1FFF;

    bool chrA = useChrA();
    uint8_t base = chrA ? 0 : 8;

    switch (chrMode) {
        case 0: {
            uint16_t reg = chrBanks[base + 7];
            uint32_t bank = (uint32_t)(reg) << 3;
            return (bank << 10) | address;
        }

        case 1: {
            uint16_t reg = address < 0x1000 ? chrBanks[base + 3] : chrBanks[base + 7];
            uint32_t bank = (uint32_t)(reg) << 2;
            return (bank << 10) | (address & 0x0FFF);
        }

        case 2: {
            uint8_t index = (uint8_t)(address >> 11);
            const uint8_t chrARegisters[] = {1, 3, 5, 7};
            const uint8_t chrBRegisters[] = {9, 11, 9, 11};

            uint8_t regIndex = chrA ? chrARegisters[index] : chrBRegisters[index];
            uint32_t bank = (uint32_t)(chrBanks[regIndex]) << 1;

            return (bank << 10) | (address & 0x07FF);
        }

        case 3:
        default: {
            uint8_t index = (uint8_t)(address >> 10);
            const uint8_t chrARegisters[] = {0, 1, 2, 3, 4, 5, 6, 7};
            const uint8_t chrBRegisters[] = {8, 9, 10, 11, 8, 9, 10, 11};

            uint8_t regIndex = chrA ? chrARegisters[index] : chrBRegisters[index];

            return ((uint32_t)(chrBanks[regIndex]) << 10) | (address & 0x03FF);
        }
    }
}

uint8_t MMC5::readCHRMem(uint32_t address) {
    NesROM *rom = getNESRom();
    uint32_t size = (uint32_t)(rom->CHRRomSize);
    return rom->CHR[address % size];
}

void MMC5::trackScanlineStart(uint16_t address) {
    if (repeatedNtReads >= 2) {
        if (!renderingActive && !frameStartPending) {
            frameStartPending = true;
            scanlineCounter = 0;
        } else {
            scanlineCounter++;
        }
    } else if (isNameTableAddress(address)) {
        if (prevPPUAddr == address) {
            repeatedNtReads++;
            if (repeatedNtReads >= 2) splitTileIndex = 0;
        }
    }

    if (prevPPUAddr != address) repeatedNtReads = 0;
}

void MMC5::trackPpuRead(uint16_t address, bool nameTableData) {
    address &= 0x3FFF;

    if (nameTableData) {
        splitTileIndex++;

        if (renderingActive) {
        } else if (frameStartPending) {
            frameStartPending = false;
            renderingActive = true;
        }
    }

    trackScanlineStart(address);

    ppuIdleTicks = 3;
    prevPPUAddr = address;
}

uint8_t MMC5::readFillMode(uint16_t address) {
    address &= 0x03FF;
    return address < 0x03C0 ? fillPattern : expand2Bit(fillPalette);
}

uint8_t MMC5::cpuRead(uint16_t address) {
    if (address >= 0x5C00 && address <= 0x5FFF) {
        uint16_t offset = address & 0x03FF;

        switch (extraRamMode) {
            case 0:
            case 1:
                return cpu->dataBus;

            case 2:
            case 3:
                return cpu->dataBus = extraRam[offset];
        }
    }

    switch (address) {
        case 0x5204: {
            uint8_t status = 0;

            if (ppu->ScanLine < 240) status |= 0x40;
            if (irqPending) status |= 0x80;

            irqPending = false;
            cpu->setExternalIRQ(false);

            return status;
        }

        case 0x5205: {
            uint16_t product = (uint16_t)(multiplyA) * multiplyB;
            return (uint8_t)(product & 0xFF);
        }

        case 0x5206: {
            uint16_t product = (uint16_t)(multiplyA) * multiplyB;
            return (uint8_t)(product >> 8);
        }

        case 0xFFFA:
        case 0xFFFB:
            renderingActive = false;
            frameStartPending = false;
            prevPPUAddr = 0;
            repeatedNtReads = 0;
            splitTileIndex = 0;
            break;

        default:
            break;
    }

    return MapperBase::cpuRead(address);
}

void MMC5::cpuWrite(uint16_t address, uint8_t value) {
    if (address >= 0x5C00 && address <= 0x5FFF) {
        uint16_t offset = address & 0x03FF;

        switch (extraRamMode) {
            case 0:
            case 1:
                if (!renderingActive) value = 0;
                extraRam[offset] = value;
                break;

            case 2:
                extraRam[offset] = value;
                break;

            case 3:
                break;
        }

        return;
    }

    switch (address) {
        case 0x5100:
            prgMode = value & 0x03;
            updatePRG();
            break;

        case 0x5101:
            chrMode = value & 0x03;
            break;

        case 0x5102:
            ramProtectA = value & 0x03;
            updatePRG();
            break;

        case 0x5103:
            ramProtectB = value & 0x03;
            updatePRG();
            break;

        case 0x5104:
            extraRamMode = value & 0x03;
            break;

        case 0x5105:
            nameTableSource[0] = (value >> 0) & 0x03;
            nameTableSource[1] = (value >> 2) & 0x03;
            nameTableSource[2] = (value >> 4) & 0x03;
            nameTableSource[3] = (value >> 6) & 0x03;
            break;

        case 0x5106:
            fillPattern = value;
            break;

        case 0x5107:
            fillPalette = value & 0x03;
            break;

        case 0x5113:
        case 0x5114:
        case 0x5115:
        case 0x5116:
        case 0x5117:
            prgBanks[address - 0x5113] = value;
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
        case 0x5128:
        case 0x5129:
        case 0x512A:
        case 0x512B: {
            uint8_t index = (uint8_t)(address - 0x5120);
            uint16_t combined = (uint16_t)(value) | ((uint16_t)(chrHighBits) << 8);

            if (chrBanks[index] != combined || lastChrRegisterAddress != address) {
                chrBanks[index] = combined;
                lastChrRegisterAddress = address;
            }
            break;
        }

        case 0x5130:
            chrHighBits = value & 0x03;
            break;

        case 0x5200:
            splitEnabled = (value & 0x80) != 0;
            splitRight = (value & 0x40) != 0;
            splitBoundary = value & 0x1F;
            break;

        case 0x5201:
            splitScrollValue = value;
            break;

        case 0x5202:
            splitChrBank = value;
            break;

        case 0x5203:
            irqScanline = value;
            break;
        case 0x5204:
            irqEnabled = (value & 0x80) != 0;

            if (!irqEnabled) {
                cpu->setExternalIRQ(false);
            } else if (irqPending) {
                cpu->setExternalIRQ(true);
            }
            break;

        case 0x5205:
            multiplyA = value;
            break;

        case 0x5206:
            multiplyB = value;
            break;

        default:
            MapperBase::cpuWrite(address, value);
            break;
    }
}

uint8_t MMC5::readVRAM(uint16_t address) {
    uint16_t ppuAddress = address & 0x3FFF;
    bool nameTableFetch = isNameTableTileFetch(ppuAddress);

    trackPpuRead(ppuAddress, nameTableFetch);

    if (extraRamMode <= 1 && renderingActive && splitEnabled) {
        uint8_t scanline = (uint8_t)(splitTileIndex >= 41 ? scanlineCounter + 1 : scanlineCounter);
        uint8_t scroll = (uint8_t)((scanline + splitScrollValue) % 240);
        uint8_t column = (uint8_t)((splitTileIndex + 2) % 42);

        if (ppuAddress >= 0x2000) {
            if (nameTableFetch) {
                if (column == 0) splitActive = !splitRight;

                if (column == splitBoundary && splitTileIndex < 42)
                    splitActive = !splitActive;
                else if (column > 32)
                    splitActive = false;

                if (splitActive) {
                    splitChrAddress = (uint16_t)(((scroll & 0xF8) << 2) | column);
                    return extraRam[splitChrAddress & 0x03FF];
                }
            } else if (splitActive) {
                uint8_t shift = (uint8_t)(((splitChrAddress >> 4) & 0x04) | (splitChrAddress & 0x02));

                uint16_t attrAddress = (uint16_t)(0x03C0 | ((splitChrAddress & 0x0380) >> 4) | ((splitChrAddress & 0x001F) >> 2));

                uint8_t palette = (uint8_t)(
                    (extraRam[attrAddress & 0x03FF] >> shift) & 0x03);

                return (uint8_t)(palette * 0x55);
            }
        }
    }

    if (extraRamMode == 1 && (splitTileIndex < 32 || splitTileIndex >= 40)) {
        if (nameTableFetch) {
            extAttrNameTableAddress = ppuAddress & 0x03FF;
            extAttrCountdown = 3;
        } else if (extAttrCountdown > 0) {
            --extAttrCountdown;

            if (extAttrCountdown == 2) {
                uint8_t attr = extraRam[
                    extAttrNameTableAddress & 0x03FF];

                extAttrChrBank = (uint8_t)(
                    (attr & 0x3F) | ((chrHighBits & 0x03) << 6));

                return expand2Bit((uint8_t)(attr >> 6));
            }
        }
    }

    uint16_t normalized = (ppuAddress - 0x2000) & 0x0FFF;
    uint8_t table = (uint8_t)((normalized >> 10) & 0x03);
    uint16_t offset = normalized & 0x03FF;

    switch (nameTableSource[table]) {
        case 0:
            return ppu->VRAM[offset];

        case 1:
            return ppu->VRAM[0x0400 + offset];

        case 2:
            if (extraRamMode <= 1) return extraRam[offset];
            return 0;

        case 3:
            return readFillMode(offset);

        default:
            return 0;
    }
}

void MMC5::writeVRAM(uint16_t address, uint8_t value) {
    uint16_t normalized = (address - 0x2000) & 0x0FFF;
    uint8_t table = (uint8_t)((normalized >> 10) & 0x03);
    uint16_t offset = normalized & 0x03FF;

    switch (nameTableSource[table]) {
        case 0:
            ppu->VRAM[offset] = value;
            break;

        case 1:
            ppu->VRAM[0x0400 + offset] = value;
            break;

        case 2:
            if (extraRamMode <= 2) extraRam[offset] = value;
            break;

        case 3:
        default:
            break;
    }
}

uint8_t MMC5::readCHR(uint16_t address, bool sprite) {
    address &= 0x1FFF;

    if (!sprite) {
        if (extraRamMode == 1 && extAttrCountdown > 0 && (splitTileIndex < 32 || splitTileIndex >= 40)) {
            --extAttrCountdown;

            if (extAttrCountdown == 1 || extAttrCountdown == 0) {
                return readCHRMem(((uint32_t)(extAttrChrBank) << 12) | (address & 0x0FFF));
            }
        }

        if (extraRamMode <= 1 && renderingActive && splitEnabled && splitActive) {
            uint8_t scanline = (uint8_t)(splitTileIndex >= 41 ? scanlineCounter + 1 : scanlineCounter);
            uint8_t scroll = (uint8_t)((scanline + splitScrollValue) % 240);
            uint32_t splitAddress = ((uint32_t)(splitChrBank) << 12) | (((address & ~0x07) | (scroll & 0x07)) & 0x0FFF);

            return readCHRMem(splitAddress);
        }
    }

    return readCHRMem(getChrAddress(address, sprite));
}

uint8_t MMC5::getEXRAMByte(uint16_t vramAddress) {
    uint16_t tileIndex = (uint16_t)(((((vramAddress >> 5) & 0x1F) * 32) | (vramAddress & 0x1F)) & 0x03FF);

    return extraRam[tileIndex];
}

void MMC5::clockCPU(void) {
    if (ppuIdleTicks == 0) return;

    if (--ppuIdleTicks == 0) {
        renderingActive = false;
        frameStartPending = false;
    }
}

void MMC5::clockPPU(void) {
    if (ppu->Dot == 0) {
        if (ppu->ScanLine >= 0 && ppu->ScanLine < 240) {
            if (ppu->mask.renderBackground || ppu->mask.renderSprites) {
                fakeScanlineCount++;
                if (irqScanline != 0 && fakeScanlineCount == irqScanline) {
                    irqPending = true;
                    if (irqEnabled) {
                        cpu->setExternalIRQ(true);
                    }
                }
            }
        } else if (ppu->ScanLine == 261) {
            fakeScanlineCount = 0;
            irqPending = false;
        }
    }
}