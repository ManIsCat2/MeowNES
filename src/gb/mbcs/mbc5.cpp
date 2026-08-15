#include "mbc5.hpp"
#include "../gb_rom.hpp"
#include "../gb_cpu.hpp"

MBC5::MBC5() {
}

const char *MBC5::getName(void) {
    return "MBC5";
}

void MBC5::reset() {
    ramEnable = false;
    romBank = 1;
    ramBank = 0;

    mapCPUMemory(0x0000, 0x3FFF, getGBRom()->ROM, 0, false);
    updateBanks();
}

void MBC5::updateBanks() {
    GbROM *rom = getGBRom();

    uint32_t numRomBanks = rom->RomSize / 0x4000;
    uint32_t activeRomBank = (numRomBanks > 0) ? (romBank % numRomBanks) : 0;
    mapCPUMemory(0x4000, 0x7FFF, rom->ROM, activeRomBank * 0x4000, false);

    if (ramEnable && rom->hasRAM() && cartRAM) {
        uint32_t numRamBanks = rom->ramSize / 0x2000;
        uint32_t activeRamBank = (numRamBanks > 0) ? (ramBank % numRamBanks) : 0;
        mapCPUMemory(0xA000, 0xBFFF, cartRAM, activeRamBank * 0x2000, true, rom->ramSize);
    } else {
        unmapCPUMemory(0xA000, 0xBFFF);
    }
}

void MBC5::cpuWrite(uint16_t addr, uint8_t value) {
    if (addr <= 0x5FFF) {
        GbROM *rom = getGBRom();
        switch (addr >> 12) {
            case 0x0:
            case 0x1:
                ramEnable = (value == 0x0A);
                break;
            
            case 0x2:
                romBank = (value & 0xFF) | (romBank & 0x100);
                break;

            case 0x3:
                romBank = (romBank & 0xFF) | ((value & 0x01) << 8);
                break;

            case 0x4:
            case 0x5:
                if (rom->cartType >= 0x1c) {
                    ramBank = value & 0x07;
                } else {
                    ramBank = value & 0x0F;
                }
                break;
        }
        
        updateBanks();
        return;
    }

    MBCBase::cpuWrite(addr, value);
}