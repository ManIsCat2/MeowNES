#include "../gb_rom.hpp"
#include "../gb_cpu.hpp"
#include "../gb_ppu.hpp"
#include "mbc_base.hpp"

MBCBase::~MBCBase() {
    if (cartRAM) delete[] cartRAM;
}

uint8_t MBCBase::cpuRead(uint16_t addr) {
    if (!CPUPages[addr >> 8].ptr) {
        //DebugPrintLog("MEM", "Read ignored: 0x%04x", addr);
        return 0;
    }
    return CPUPages[addr >> 8].ptr[addr & 0xFF];
}
void MBCBase::cpuWrite(uint16_t addr, uint8_t value) {
    if (CPUPages[addr >> 8].write) {
        CPUPages[addr >> 8].ptr[addr & 0xFF] = value;
    } else {
        //DebugPrintLog("MEM", "Write ignored: 0x%04x", addr);
    }
}

void MBCBase::mapCPUMemory(uint16_t start, uint16_t end, uint8_t *memory, uint32_t offset, bool writable, uint32_t size) {
    uint8_t page = start >> 8;
    uint32_t memSize = size ? size : getGBRom()->RomSize;
    
    for (uint32_t addr = start; addr <= end; addr += 0x100) {
        CPUPages[page].ptr = memory + ((offset + (addr - start)) & (memSize - 1));
        CPUPages[page].write = writable;
        page++;
    }
}
void MBCBase::unmapCPUMemory(uint16_t start, uint16_t end) {
    uint8_t page = start >> 8;
    
    for (uint32_t addr = start; addr <= end; addr += 0x100) {
        CPUPages[page].ptr = nullptr;
        CPUPages[page].write = false;
        page++;
    }
}

void MBCBase::saveSRAM(void) {
    GbROM *rom = getGBRom();
    if (!rom->hasBattery()) return;

    std::string fname = ("saves/"+rom->Name+".sav");
    FILE* f = fopen(fname.c_str(), "wb");
    if (!f) return;
    fwrite(cartRAM, 1, rom->ramSize, f);
    fclose(f);
    DebugPrintLog("MAPPER", "Saved SRAM to %s", fname.c_str())
}
void MBCBase::loadSRAM(void) {
    GbROM *rom = getGBRom();
    if (!rom->hasBattery()) return;

    std::string fname = ("saves/"+rom->Name+".sav");
    FILE* f = fopen(fname.c_str(), "rb");
    if (!f) return;
    fread(cartRAM, 1, rom->ramSize, f);
    fclose(f);
    DebugPrintLog("MAPPER", "Loaded SRAM from %s", fname.c_str())
}

void MBCBase::initialize(void) {
    connectBus(&gbCpu, &gbPpu, nullptr);

    uint32_t ramSize = getGBRom()->ramSize;
    if (ramSize) {
        cartRAM = new uint8_t[ramSize];
        if (getGBRom()->hasBattery()) {
            loadSRAM();
        }
    }

    //mapCPUMemory(0x8000, 0x9FFF, ppu->VRAM, 0, true, 0x2000);
    mapCPUMemory(0xC000, 0xDFFF, WRAM, 0, true, 0x2000);
    //mapCPUMemory(0xE000, 0xFDFF, WRAM, 0, true);
    //mapCPUMemory(0xFE00, 0xFE9F, ppu->OAM, 0, true);
    //mapCPUMemory(0xFF80, 0xFFFE, HRAM, 0, true);

    reset();
}