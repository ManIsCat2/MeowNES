#pragma once
#include "mbc_base.hpp"

class MBC1 : public MBCBase {
public:
    MBC1();

    const char *getName(void) override;
    void reset() override;
    void cpuWrite(uint16_t addr, uint8_t value) override;
private:
    bool ramEnable = false;
    uint8_t romBankLow = 1;
    uint8_t bankHigh = 0;
    uint8_t bankingMode = 0;
    uint32_t ramBank = 0;

    void updateBanks();
};
