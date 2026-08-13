#pragma once
#include "mbc_base.hpp"
#include <ctime>

class MBC3 : public MBCBase {
public:
    MBC3();

    const char* getName(void) override;
    void reset() override;
    
    uint8_t cpuRead(uint16_t addr) override;
    void cpuWrite(uint16_t addr, uint8_t value) override;

private:
    bool ramEnable = false;
    uint8_t romBank = 1;
    uint8_t ramBank = 0;

    uint8_t rtcSeconds = 0;
    uint8_t rtcMinutes = 0;
    uint8_t rtcHours = 0;
    uint8_t rtcDayLow = 0;
    uint8_t rtcDayHigh = 0;

    uint8_t latchedSeconds = 0;
    uint8_t latchedMinutes = 0;
    uint8_t latchedHours = 0;
    uint8_t latchedDayLow = 0;
    uint8_t latchedDayHigh = 0;

    uint8_t lastLatchVal = 0xFF;
    std::time_t lastTickTime = 0;

    void updateBanks();
    void updateRTC();
};