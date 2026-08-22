#include "nes_rom.hpp"
#include "../main.hpp"
#include "mappers/cprom.hpp"
#include "nes_ppu.hpp"

NesROM::NesROM() {
}
NesROM::~NesROM() {
    delete mapper;
    nesCpu.romMapper = nesPpu.romMapper = mapper = nullptr;
    delete[] ROM;
    delete[] CHR;
}

MapperBase *NesROM::GetMapper(uint16_t id, uint16_t subId) {
    switch (id) {
        case 0: return new NROM();
        case 1: return new MMC1();
        case 2: return new UxROM();
        case 3: return new CNROM();
        case 4: return new MMC3();
        case 5: return new MMC5();
        case 9: return new MMC2();
        case 13: return new CPROM();
        case 19: return new N163();
        case 34: 
            switch (subId) {
                case 0: 
                    if (CHRRomSize > 0) {
                        return new Mapper34(true);
                    } else {
                        return new Mapper34();
                    }
                case 1: return new Mapper34(true);
                case 2: default: return new Mapper34();
            }
			break;
        case 62: return new XIn1();
        case 69: return new SunSoftFME7();
        case 90: return new JyCompany();
        case 116: return new SL12();
        case 209: return new JyCompany();
        case 210: return new N163();
        case 211: return new JyCompany();
        case 225: case 255: return new MultiCart110In1();
        default: {
            QMessageBox::critical((QMainWindow*)globalQTWin, "Error", ("Mapper " + std::to_string(id) + " is unimplemented, failed to open ROM").c_str());
            return nullptr;
        }
    }
}

ConsoleRegion NesROM::GetRegion(void) {
    ConsoleRegion romRegion = ConsoleRegion::NTSC;
    if (Version == HeaderVersion::NES2_0) {
        switch (Header[12] & 0x03) {
            case 1:
                romRegion = ConsoleRegion::PAL;
                break;
            case 3:
                romRegion = ConsoleRegion::DENDY;
                break;
            default:
                romRegion = ConsoleRegion::NTSC;
                break;
        }
    } else {
        romRegion = (Header[9] & 1) ? ConsoleRegion::PAL : ConsoleRegion::NTSC;
    }

    if (Name.find("(E)") != std::string::npos || Name.find("(e)") != std::string::npos) {
        if (romRegion == ConsoleRegion::NTSC) {
            romRegion = ConsoleRegion::PAL;
        }
    }

    return romRegion;
}

bool NesROM::load(const std::string &filename) {
    std::ifstream rom(filename, std::ios::binary | std::ios::ate);
    if (!rom) {
        DebugPrintLog("ROM", "Failed to open ROM '%s'", filename.c_str());
        return false;
    }

    std::filesystem::path Path(filename);
    std::string romName = Path.filename().string();

    std::streamsize fsize = rom.tellg();
    rom.seekg(0, std::ios::beg);

    std::vector<uint8_t> data((size_t)fsize);
    if (!rom.read(reinterpret_cast<char*>(data.data()), fsize)) {
        DebugPrintLog("ROM", "Failed to read ROM '%s'", filename.c_str());
        return false;
    }

    // header
    if (data.size() < 16 || data[0] != 'N' || data[1] != 'E' || data[2] != 'S' || data[3] != 0x1A) {
        DebugPrintLog("ROM", "Invalid NES Header");
        QMessageBox::critical((QMainWindow*)globalQTWin, "Error", "ROM has invalid NES Header");
        return false;
    }

    uint8_t romHeader[16];
    std::memcpy(romHeader, data.data(), 16);

    MirrorMode romMirroring = (romHeader[6] & 1) ? MirrorMode::VERTICAL :  MirrorMode::HORIZONTAL;

    uint8_t prgPages = data[4];
    uint8_t chrPages = data[5];
    uint8_t flags6 = data[6];
    uint8_t flags7 = data[7];
    uint8_t flags8 = data[8];
    uint8_t flags9 = data[9];

    //bool hasTrainer = (flags6 & 0x04) != 0;
    bool romHasBattery = (flags6 & 0x02) != 0;
    HeaderVersion romVersion = HeaderVersion::INES;

    if ((flags7 & 0x0C) == 0x08) {
        romVersion = HeaderVersion::NES2_0;
    }

    uint16_t romSubMapperID = 0;
    uint16_t romMapperID = 0;
    size_t romPRGSize, romCHRSize = 0;

    uint16_t prgNewVal = flags9 & 0x0F;
    uint16_t chrNewVal = flags9 >> 4;
    uint16_t romPRGNumPages = prgPages;
    uint16_t romCHRNumPages = chrPages;

    if (romVersion == HeaderVersion::NES2_0) {
        romMapperID = ((flags8 & 0x0F) << 8) | (flags7 & 0xF0) | (flags6 >> 4);
        romSubMapperID = flags8 >> 8;

        romPRGSize = (romPRGNumPages = ((prgNewVal << 8) | prgPages)) * 0x4000;
        romCHRSize = (romCHRNumPages = ((chrNewVal << 8) | chrPages)) * 0x2000;
    } else {
        romMapperID = (flags7 & 0xF0) | (flags6 >> 4);

        romPRGSize = size_t(prgPages) * 0x4000;
        romCHRSize = size_t(chrPages) * 0x2000;
    }

    if (!prgPages) {
       // DebugPrintLog("ROM", "ROM has no PRG Pages");
       // QMessageBox::critical((QMainWindow*)globalQTWin, "Error", "ROM doesn't have any PRG Pages");
       // return false;
        romPRGSize = 0x400000; // 0x100 * 0x4000
    }

    MapperBase *romMapperNew = GetMapper(romMapperID, romSubMapperID);
    if (!romMapperNew) { 
        DebugPrintLog("ROM", "Unimplemented mapper: %u, failed to open ROM", romMapperID)
        return false;
    } else {
        if (mapper) { delete mapper; nesCpu.romMapper = nesPpu.romMapper = mapper = nullptr; }
        Name = romName;
        std::memcpy(Header, romHeader, 16);
        hasBattery = romHasBattery;
        Mirroring = nesPpu.Mirroring = romMirroring;
        Version = romVersion;
        Region = GetRegion();
        SubMapperID = romSubMapperID;
        MapperID = romMapperID;
        PRGRomSize = romPRGSize;
        CHRRomSize = romCHRSize;
        PRGNumPages = romPRGNumPages;
        CHRNumPages = romCHRNumPages;
        mapper = romMapperNew;
    }
    mapper->subMapper = romSubMapperID;
    nesPpu.romMapper = nesCpu.romMapper = romMapperNew;

    if (ROM) { delete[] ROM; ROM = nullptr; }
    ROM = new uint8_t[PRGRomSize];

    if (CHR) { delete[] CHR; CHR = nullptr; }

    size_t offset = 16;

    std::memcpy(ROM, &data[offset], PRGRomSize);
    offset += PRGRomSize;
        
    if (chrPages == 0) {
        CHR = new uint8_t[mapper->getCHRRamSize()];
    } else {
        CHR = new uint8_t[CHRRomSize];
    }
    offset += CHRRomSize;
    DebugPrintLog("ROM", "Loaded NES ROM '%s'", Name.c_str());
    mapper->initialize();
    if (Version == HeaderVersion::NES2_0) {
        DebugPrintLog("ROM", "Detected NES2.0 Header with Submapper %u", SubMapperID)
    }
    std::string regionNames[] = {"NTSC", "PAL", "Dendy"};
    std::string fullRegionStr = "ROM uses " + regionNames[(int)Region] + " mode";
    DebugPrintLog("ROM", "%s", fullRegionStr.c_str());
    return true;
}