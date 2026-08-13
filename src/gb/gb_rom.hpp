#pragma once

#include "gb_console.hpp"
#include "mbcs/mbc_base.hpp"
#include <string>
#include <cstdint>

class GbROM : public ROMImage {
public:
    GbROM();
    ~GbROM();

    uint8_t Header[80];
    uint8_t *ROM = nullptr;
    uint8_t bootROM[0x900];
    uint32_t RomSize = 0;
    uint8_t cartType = 0;
    MBCBase *mbc = nullptr;
    uint32_t ramSize = 0;
    
    std::string Title = ""; 

    bool isCGB = false;
    bool isRealCGB = false; 
    bool isCGBOnly = false;
    bool isDualMode = false;

    bool hasBattery(void);
    bool hasRAM(void);
    bool hasRTC(void);
    bool load(const std::string &file) override;
};