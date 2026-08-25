#include "gb_rom.hpp"
#include "mbcs/mbc5.hpp"
#include "mbcs/mbcs.hpp"
#include "../main.hpp"
#include <fstream>
#include <iostream>

uint8_t SameBoyCGBBootROM[] = {
    #embed "bootroms/cgb_boot.bin"
};

GbROM::GbROM() {

}

GbROM::~GbROM() {
    delete[] ROM;
    delete mbc;
}

bool GbROM::hasBattery(void) {
    switch (cartType) {
        case 0x03:
        case 0x06:
        case 0x09:
        case 0x0F:
        case 0x10:
        case 0x13:
        case 0x1B:
        case 0x1E:
        case 0x22:
        case 0xFF:
            return true;
        default:
            return false;
    }
}

bool GbROM::hasRTC(void) {
    switch (cartType) {
        case 0x0F:
        case 0x10:
            return true;
        default:
            return false;
    }
}

bool GbROM::hasRAM(void) {
    switch (cartType) {
        case 0x02: case 0x03:
        case 0x08: case 0x09:
        case 0x10: case 0x12: case 0x13:
        case 0x1A: case 0x1B: case 0x1D: case 0x1E:
        case 0x22:
        case 0xFF:
            return true;
        default:
            return false;
    }
}

bool GbROM::load(const std::string &filename) {
    std::filesystem::path Path(filename);
    Name = Path.filename().string();

    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return false;
    }

    RomSize = file.tellg();
    if (RomSize < 0x0150) {
        return false;
    }

    ROM = new uint8_t[RomSize];

    file.seekg(0, std::ios::beg);
    file.read((char*)ROM, RomSize);
    file.close();

    for (int i = 0; i < 80; i++) {
        Header[i] = ROM[0x0100 + i];
    }

    uint8_t cgbFlag = ROM[0x0143];
    isDualMode = (cgbFlag == 0x80);
    isCGBOnly = (cgbFlag == 0xC0);
    isCGB = true;
    isRealCGB = isDualMode || isCGBOnly;

    memcpy(bootROM, SameBoyCGBBootROM, 0x900);

    Title = "";
    for (uint16_t addr = 0x0134; addr <= 0x0143; addr++) {
        char c = (char)ROM[addr];
        if (c == '\0') break;
        Title += c;
    }

    cartType = ROM[0x0147];

    switch (ROM[0x0149]) {
        case 0x00: ramSize = 0; break;
        case 0x01: ramSize = 2 * 1024; break;
        case 0x02: ramSize = 8 * 1024; break;
        case 0x03: ramSize = 32 * 1024; break;
        case 0x04: ramSize = 128 * 1024; break;
        case 0x05: ramSize = 64 * 1024; break;
        default: {
            ramSize = 0x2000;
            break;
        }
    }
    switch (cartType) {
        case 0x00:
            mbc = new MBC0();
            break;

        case 0x01:
        case 0x02:
        case 0x03:
            mbc = new MBC1();
            break;

        case 0x05:
        case 0x06:
            break;

        case 0x0F:
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
            mbc = new MBC3();
            break;

        case 0x19:
        case 0x1A:
        case 0x1B:
        case 0x1C:
        case 0x1D:
        case 0x1E:
            mbc = new MBC5();
            break;
    }

    if (!mbc) {
        char err[256];
        sprintf(err, "Unimplemented mapper: %u, failed to open ROM", cartType);
        ErrorEmuAndHalt("ROM", err);
        return false;
    }
    mbc->initialize();

    DebugPrintLog("ROM", "Loaded GameBoy ROM '%s'", Name.c_str());

    return true;
}