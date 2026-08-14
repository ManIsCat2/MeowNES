#pragma once
#include "mbc_base.hpp"

class MBC5 : public MBCBase {
public:
    MBC5();

    const char* getName(void) override;
    void reset() override;
    
    uint8_t cpuRead(uint16_t addr) override;
    void cpuWrite(uint16_t addr, uint8_t value) override;

private:
    bool ramEnable = false;
    uint16_t romBank = 1;
    uint8_t ramBank = 0;

    void updateBanks();
};