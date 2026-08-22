#include "mapper_base.hpp"
#include "../nes_rom.hpp"
#include "../../main.hpp"

MapperBase::~MapperBase() {
    if (SRAM) delete[] SRAM;
}

void MapperBase::setCHRBank(uint16_t page, uint16_t val, enum BankSize size) {
    switch (size) {
        case BANK_1K: {
            uint32_t chrSlotSize = getCHRBankSize();
            uint32_t bankOffset = val * chrSlotSize;
            uint16_t ppuStart = page * chrSlotSize;
            uint16_t ppuEnd = ppuStart + chrSlotSize - 1;
            mapPPUMemory(ppuStart, ppuEnd, getNESRom()->CHR, bankOffset, getNESRom()->CHRRomSize == 0);
            break;
        }
        case BANK_2K:
            setCHRBank(page * 2, val, BANK_1K);
            setCHRBank(page * 2 + 1, val + 1, BANK_1K);
            break;
        case BANK_4K:
            setCHRBank(page * 2, val, BANK_2K);
            setCHRBank(page * 2 + 1, val + 2, BANK_2K);
            break;
        case BANK_8K:
            setCHRBank(page * 2, val, BANK_4K);
            setCHRBank(page * 2 + 1, val + 4, BANK_4K);
            break;
        case BANK_16K:
            setCHRBank(page * 2, val, BANK_8K);
            setCHRBank(page * 2 + 1, val + 8, BANK_8K);
            break;
        case BANK_32K:
            setCHRBank(page * 2, val, BANK_16K);
            setCHRBank(page * 2 + 1, val + 16, BANK_16K);
            break;
    }
}
void MapperBase::setPRGBank(uint16_t page, uint16_t val, enum BankSize size) {
    switch (size) {
        case BANK_1K: {
            uint32_t prgSlotSize = getPRGBankSize();
            uint32_t bankOffset = val * prgSlotSize;
            uint16_t cpuStart = 0x8000 + (page * prgSlotSize);
            uint16_t cpuEnd = cpuStart + prgSlotSize - 1;
            mapCPUMemory(cpuStart, cpuEnd, getNESRom()->ROM, bankOffset, false);
            break;
        }
        case BANK_2K:
            setPRGBank(page * 2, val, BANK_1K);
            setPRGBank(page * 2 + 1, val + 1, BANK_1K);
            break;
        case BANK_4K:
            setPRGBank(page * 2, val, BANK_2K);
            setPRGBank(page * 2 + 1, val + 2, BANK_2K);
            break;
        case BANK_8K:
            setPRGBank(page * 2, val, BANK_4K);
            setPRGBank(page * 2 + 1, val + 4, BANK_4K);
            break;
        case BANK_16K:
            setPRGBank(page * 2, val, BANK_8K);
            setPRGBank(page * 2 + 1, val + 8, BANK_8K);
            break;
        case BANK_32K:
            setPRGBank(page * 2, val, BANK_16K);
            setPRGBank(page * 2 + 1, val + 16, BANK_16K);
            break;
    }
}

uint8_t MapperBase::cpuRead(uint16_t addr) {
    if (!PRGPages[addr >> 8].ptr) {
        return cpu->dataBus;
    }
    return cpu->dataBus = (PRGPages[addr >> 8].ptr[addr & 0xFF]);
}
void MapperBase::cpuWrite(uint16_t addr, uint8_t value) {
    if (PRGPages[addr >> 8].write) {
        PRGPages[addr >> 8].ptr[addr & 0xFF] = value;
    }
}

uint8_t MapperBase::readCHR(uint16_t addr, bool sprite) {
    if (!CHRPages[addr >> 8].ptr) {
        return ppu->dataBus;
    }

    return CHRPages[addr >> 8].ptr[addr & 0xFF];
}
void MapperBase::writeCHR(uint16_t addr, uint8_t value) {
    if (CHRPages[addr >> 8].write) {
        CHRPages[addr >> 8].ptr[addr & 0xFF] = value;
    }
}

uint8_t MapperBase::readVRAM(uint16_t addr) {
    return ppu->VRAM[ppu->mirrorNametable(addr)];
}
void MapperBase::writeVRAM(uint16_t addr, uint8_t value) {
    ppu->VRAM[ppu->mirrorNametable(addr)] = value;
}

void MapperBase::mapCPUMemory(uint16_t start, uint16_t end, uint8_t *memory, uint32_t offset, bool writable) {
    uint8_t page = start >> 8;
    
    for (uint32_t addr = start; addr <= end; addr += 0x100) {
        PRGPages[page].ptr = memory + ((offset + (addr - start)) & (getNESRom()->PRGRomSize-1));
        PRGPages[page].write = writable;
        page++;
    }
}
void MapperBase::unmapCPUMemory(uint16_t start, uint16_t end) {
    uint8_t page = start >> 8;
    
    for (uint32_t addr = start; addr <= end; addr += 0x100) {
        PRGPages[page].ptr = nullptr;
        PRGPages[page].write = false;
        page++;
    }
}

void MapperBase::mapPPUMemory(uint16_t start, uint16_t end, uint8_t *memory, uint32_t offset, bool writable) {
    uint8_t page = start >> 8;

    for (uint32_t addr = start; addr <= end; addr += 0x100) {
        CHRPages[page & 0x1F].ptr = memory + (offset + (addr - start));
        CHRPages[page & 0x1F].write = writable;
        page++;
    }
}

void MapperBase::saveSRAM() {
    if (!getNESRom()->hasBattery) return;
    std::string fname = ("saves/"+getNESRom()->Name+".sav");
    FILE* f = fopen(fname.c_str(), "wb");
    if (!f) return;
    fwrite(SRAM, 1, getSRAMSize(), f);
    fclose(f);
    DebugPrintLog("MAPPER", "Saved SRAM to %s", fname.c_str())
}
void MapperBase::loadSRAM() {
    if (!getNESRom()->hasBattery) return;
    std::string fname = ("saves/"+getNESRom()->Name+".sav");
    FILE* f = fopen(fname.c_str(), "rb");
    if (!f) return;
    fread(SRAM, 1, getSRAMSize(), f);
    fclose(f);
    DebugPrintLog("MAPPER", "Loaded SRAM from %s", fname.c_str())
}

void MapperBase::initialize() {
    connectBus(&nesCpu, &nesPpu, nullptr);
    if (getNESRom()->hasBattery) {
        //DebugPrintLog("MAPPER", "Loaded Save");
        SRAM = new uint8_t[getSRAMSize()];
        loadSRAM();
    }
    reset();
}