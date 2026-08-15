#include "mbc3.hpp"
#include "../gb_rom.hpp"
#include "../gb_cpu.hpp"

MBC3::MBC3() {

}

const char *MBC3::getName(void) {
    return "MBC3";
}

void MBC3::reset() {
    ramEnable = false;
    romBank = 1;
    ramBank = 0;
    lastLatchVal = 0xFF;
    lastTickTime = std::time(nullptr);

    rtcSeconds = 0;
    rtcMinutes = 0;
    rtcHours = 0;
    rtcDayLow = 0;
    rtcDayHigh = 0;

    latchedSeconds = 0;
    latchedMinutes = 0;
    latchedHours = 0;
    latchedDayLow = 0;
    latchedDayHigh = 0;

    mapCPUMemory(0x0000, 0x3FFF, getGBRom()->ROM, 0, false);
    updateBanks();
}

void MBC3::updateRTC() {
    GbROM *rom = getGBRom();
    if (!rom->hasRTC()) return;

    std::time_t now = std::time(nullptr);
    std::time_t elapsed = now - lastTickTime;
    lastTickTime = now;

    if (elapsed <= 0 || (rtcDayHigh & 0x40)) {
        return;
    }

    uint32_t totalSeconds = rtcSeconds + elapsed;
    rtcSeconds = totalSeconds % 60;
    
    uint32_t totalMinutes = rtcMinutes + (totalSeconds / 60);
    rtcMinutes = totalMinutes % 60;

    uint32_t totalHours = rtcHours + (totalMinutes / 60);
    rtcHours = totalHours % 24;

    uint32_t totalDays = ((rtcDayHigh & 0x01) << 8) | rtcDayLow;
    totalDays += (totalHours / 24);

    if (totalDays > 511) {
        rtcDayHigh |= 0x80;
        totalDays %= 512;
    }

    rtcDayLow = totalDays & 0xFF;
    rtcDayHigh = (rtcDayHigh & ~0x01) | ((totalDays >> 8) & 0x01);
}

void MBC3::updateBanks() {
    GbROM *rom = getGBRom();

    uint32_t numRomBanks = rom->RomSize / 0x4000;
    uint32_t activeRomBank = (romBank == 0) ? 1 : (romBank & 0x7F);
    mapCPUMemory(0x4000, 0x7FFF, rom->ROM, (activeRomBank % numRomBanks) * 0x4000, false);

    if (ramEnable && rom->hasRAM() && cartRAM && ramBank <= 0x03) {
        mapCPUMemory(0xA000, 0xBFFF, cartRAM, ramBank * 0x2000, true, rom->ramSize);
    } else {
        unmapCPUMemory(0xA000, 0xBFFF);
    }
}

uint8_t MBC3::cpuRead(uint16_t addr) {
    GbROM *rom = getGBRom();
    if (addr >= 0xA000 && addr <= 0xBFFF && ramEnable) {
        if (rom->hasRTC() && ramBank >= 0x08 && ramBank <= 0x0C) {
            updateRTC();
            switch (ramBank) {
                case 0x08: return latchedSeconds;
                case 0x09: return latchedMinutes;
                case 0x0A: return latchedHours;
                case 0x0B: return latchedDayLow;
                case 0x0C: return latchedDayHigh;
            }
        }
    }
    return MBCBase::cpuRead(addr);
}

void MBC3::cpuWrite(uint16_t addr, uint8_t value) {
    GbROM *rom = getGBRom();

    if (addr >= 0x0000 && addr <= 0x1FFF) {
        ramEnable = ((value & 0x0F) == 0x0A);
        updateBanks();
        return;
    }

    if (addr >= 0x2000 && addr <= 0x3FFF) {
        romBank = value & 0x7F;
        updateBanks();
        return;
    }

    if (addr >= 0x4000 && addr <= 0x5FFF) {
        ramBank = value;
        updateBanks();
        return;
    }

    if (addr >= 0x6000 && addr <= 0x7FFF) {
        if (rom->hasRTC()) {
            if (lastLatchVal == 0x00 && value == 0x01) {
                updateRTC();
                latchedSeconds = rtcSeconds;
                latchedMinutes = rtcMinutes;
                latchedHours = rtcHours;
                latchedDayLow = rtcDayLow;
                latchedDayHigh = rtcDayHigh;
            }
            lastLatchVal = value;
        }
        return;
    }

    if (addr >= 0xA000 && addr <= 0xBFFF && ramEnable) {
        if (rom->hasRTC() && ramBank >= 0x08 && ramBank <= 0x0C) {
            updateRTC();
            switch (ramBank) {
                case 0x08: rtcSeconds = value & 0x3F; break;
                case 0x09: rtcMinutes = value & 0x3F; break;
                case 0x0A: rtcHours = value & 0x1F; break;
                case 0x0B: rtcDayLow = value; break;
                case 0x0C: rtcDayHigh = value; break;
            }
            return;
        }
    }

    MBCBase::cpuWrite(addr, value);
}