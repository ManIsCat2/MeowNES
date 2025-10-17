#pragma once

#include <cstdint>
#include <iostream>
#include <array>
#include <string>
#include <fstream>
#include <vector>
#include <cstring>
#include <filesystem>

#include "nes.hpp"
#include "nes_cpu.hpp"
#include "nes_controller.hpp"

extern bool romIsLoaded;
class NesROM;
extern NesROM globalROM;

class NesROM {
public:
    uint8_t Header[8];

    bool LoadNES(const std::string& filename, std::array<uint8_t, MEMORY_SIZE>& mem) {
        std::ifstream rom(filename, std::ios::binary | std::ios::ate);
        if (!rom) {
            std::cerr << "Failed to open ROM: " << filename << "\n";
            return false;
        }

        std::streamsize fsize = rom.tellg();
        if (fsize < 16) {
            std::cerr << "ROM too small\n";
            return false;
        }
        rom.seekg(0, std::ios::beg);

        std::vector<uint8_t> data((size_t)fsize);
        if (!rom.read(reinterpret_cast<char*>(data.data()), fsize)) {
            std::cerr << "Failed to read ROM\n";
            return false;
        }

        // header
        if (data.size() < 16 || data[0] != 'N' || data[1] != 'E' || data[2] != 'S' || data[3] != 0x1A) {
            std::cerr << "Invalid iNES header\n";
            return false;
        }

        std::memcpy(globalROM.Header, data.data(), 8);

        uint8_t prgPages = data[4];
        uint8_t chrPages = data[5];
        uint8_t flags6 = data[6];
        uint8_t flags7 = data[7];

        bool hasTrainer = (flags6 & 0x04) != 0;

        uint8_t mapper = (flags7 & 0xF0) | (flags6 >> 4);
        if (mapper != 0) {
            std::cerr << "Warning: mapper " << int(mapper) << " detected. only mapper 0 (NROM) is supported by MeowNES.\n";
        }

        size_t offset = 16;
        if (hasTrainer) {
            if (data.size() < offset + 512) {
                std::cerr << "ROM too small\n";
                return false;
            }
            offset += 512;
        }

        size_t totalPrgSize = size_t(prgPages) * 16 * 1024;

        if (prgPages == 0) {
            std::cerr << "ROM has zero PRG pages.\n";
            return false;
        } else if (prgPages == 1) {
            std::memcpy(&mem[0x8000], &data[offset], 0x4000);
            std::memcpy(&mem[0xC000], &data[offset], 0x4000);
        } else {
            std::memcpy(&mem[0x8000], &data[offset], 0x4000);
            std::memcpy(&mem[0xC000], &data[offset + 0x4000], 0x4000);
        }
        offset += totalPrgSize;

        size_t totalChrSize = size_t(chrPages) * 8 * 1024;
        if (chrPages == 0) {
            uint8_t zeros[0x2000] = {};
            ppu.LoadCHRROM(zeros, 0x2000);
        } else {
            ppu.LoadCHRROM(&data[offset], totalChrSize);
        }
        offset += totalChrSize;

        std::cerr << "Loaded ROM:\nPRG pages = " << int(prgPages) << "\n"
                << "CHR pages = " << int(chrPages) << "\n"
                << "CHR size = " << int(totalChrSize) << "\n"
                << "mapper = " << int(mapper) << "\n\n";
        return true;
    }
};