#include "mbc5.hpp"
#include "../gb_rom.hpp"
#include "../gb_cpu.hpp"

MBC5::MBC5() {

}

const char* MBC5::getName(void) {
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

uint8_t MBC5::cpuRead(uint16_t addr) {
    // todo do stuff here that i havent done
    // for mbc5
    return MBCBase::cpuRead(addr);
}

void MBC5::cpuWrite(uint16_t addr, uint8_t value) {
    if (addr >= 0x0000 && addr <= 0x1FFF) {
        ramEnable = ((value & 0x0F) == 0x0A);
        updateBanks();
        return;
    }

    if (addr >= 0x2000 && addr <= 0x2FFF) {
        romBank = (romBank & 0x0100) | value;
        updateBanks();
        return;
    }

    if (addr >= 0x3000 && addr <= 0x3FFF) {
        romBank = (romBank & 0x00FF) | (((uint16_t)(value & 0x01)) << 8);
        updateBanks();
        return;
    }

    if (addr >= 0x4000 && addr <= 0x5FFF) {
        ramBank = value & 0x0F;
        updateBanks();
        return;
    }

    if (addr >= 0x6000 && addr <= 0x7FFF) {
        return;
    }

    MBCBase::cpuWrite(addr, value);
}