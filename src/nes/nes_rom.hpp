#pragma once

enum class HeaderVersion {
    NES2_0,
    INES,
};

#include <cstdint>
#include <iostream>
#include <array>
#include <string>
#include <fstream>
#include <vector>
#include <cstring>
#include <filesystem>

#include "mappers/mappers.hpp"
#include "../console.hpp"

class NesROM : public ROMImage {
public:
    NesROM();
    ~NesROM();
    uint8_t Header[16];
    bool hasBattery = false;
    HeaderVersion Version = HeaderVersion::INES;
    uint8_t *ROM = nullptr;
    uint8_t *CHR = nullptr;
    uint16_t ResetVec = 0;
    MirrorMode Mirroring = MirrorMode::HORIZONTAL;
    uint8_t PRGNumPages = 0;
    uint8_t CHRNumPages = 0;
    size_t PRGRomSize = 0;
    size_t CHRRomSize = 0;
    uint16_t MapperID = 0;
    uint16_t SubMapperID = 0;
    MapperBase *mapper = nullptr;

    MapperBase *GetMapper(uint16_t id, uint16_t subId);
    ConsoleRegion GetRegion(void);
    bool load(const std::string &file) override;
};