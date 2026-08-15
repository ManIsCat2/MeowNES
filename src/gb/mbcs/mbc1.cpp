#include "mbc1.hpp"
#include "../gb_rom.hpp"
#include "../gb_cpu.hpp"
#include "../../rom.hpp"

MBC1::MBC1() {

}

const char *MBC1::getName(void) {
    return "MBC1";
}

void MBC1::reset() {
    ramEnable = false;
    romBankLow = 1;
    bankHigh = 0;
    bankingMode = 0;

    updateBanks();
}

void MBC1::updateBanks() {
    GbROM *rom = getGBRom();

    uint32_t romBank = 0;

    if (bankingMode == 0) {
        romBank = ((bankHigh & 0x03) << 5) | (romBankLow & 0x1F);
        ramBank = 0;
    } else {
        romBank = ((bankHigh & 0x03) << 5) | (romBankLow & 0x1F);
        ramBank = bankHigh & 0x03;
    }

    if ((romBank & 0x1F) == 0) {
        romBank++;
    }

    uint32_t numRomBanks = rom->RomSize / 0x4000;
    romBank &= (numRomBanks - 1);

    if (bankingMode == 0) {
        mapCPUMemory(0x0000, 0x3FFF, rom->ROM, 0, false);
    } else {
        uint32_t bank0 = (bankHigh & 0x03) << 5;
        bank0 &= (numRomBanks - 1);
        mapCPUMemory(0x0000, 0x3FFF, rom->ROM, bank0 * 0x4000, false);
    }

    mapCPUMemory(0x4000, 0x7FFF, rom->ROM, romBank * 0x4000, false);

    if (ramEnable && cartRAM) {
        mapCPUMemory(0xA000, 0xBFFF, cartRAM, ramBank * 0x2000, true, rom->ramSize);
    } else {
        unmapCPUMemory(0xA000, 0xBFFF);
    }
}

void MBC1::cpuWrite(uint16_t addr, uint8_t value) {
    if (addr >= 0x0000 && addr <= 0x1FFF) {
        ramEnable = ((value & 0x0F) == 0x0A);
        updateBanks();
        return;
    }

    if (addr >= 0x2000 && addr <= 0x3FFF) {
        romBankLow = value & 0x1F;
        if (romBankLow == 0) {
            romBankLow = 1;
        }

        updateBanks();
        return;
    }

    if (addr >= 0x4000 && addr <= 0x5FFF) {
        bankHigh = value & 0x03;
        updateBanks();
        return;
    }

    if (addr >= 0x6000 && addr <= 0x7FFF) {
        bankingMode = value & 0x01;
        updateBanks();
        return;
    }

    MBCBase::cpuWrite(addr, value);
}