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

bool NesROM::loadINES(const std::vector<uint8_t> &data, const std::string &romName) {
    uint8_t romHeader[16];
    memcpy(romHeader, data.data(), 16);

    uint8_t flags6 = romHeader[6];
    uint8_t flags7 = romHeader[7];
    uint8_t flags8 = romHeader[8];
    uint8_t flags9 = romHeader[9];

    MirrorMode romMirroring = (flags6 & 1) ? MirrorMode::VERTICAL : MirrorMode::HORIZONTAL;

    uint8_t prgPages = romHeader[4];
    uint8_t chrPages = romHeader[5];

    bool romHasBattery = (flags6 & 0x02);

    HeaderVersion romVersion = HeaderVersion::INES;
    if ((flags7 & 0x0C) == 0x08) romVersion = HeaderVersion::NES2_0;

    uint16_t romMapperID = 0;
    uint16_t romSubMapperID = 0;
    size_t romPRGSize = 0;
    size_t romCHRSize = 0;
    if (romVersion == HeaderVersion::NES2_0) {
        romMapperID = ((flags8 & 0x0F) << 8) | (flags7 & 0xF0) | (flags6 >> 4);
        romSubMapperID = flags8 >> 4;
        romPRGSize = (((flags9 & 0x0F) << 8) | prgPages) * 0x4000;
        romCHRSize = (((flags9 >> 4) << 8) | chrPages) * 0x2000;
    } else {
        romMapperID = (flags7 & 0xF0) | (flags6 >> 4);
        romPRGSize = (size_t)prgPages * 0x4000;
        romCHRSize = (size_t)chrPages * 0x2000;
    }


    MapperBase* romMapperNew = GetMapper(romMapperID, romSubMapperID);

    if (!romMapperNew) {
        DebugPrintLog("ROM", "Unimplemented mapper %u", romMapperID);
        return false;
    }

    if (mapper) delete mapper;
    mapper = romMapperNew;
    nesCpu.romMapper = nesPpu.romMapper = mapper;

    Name = romName;
    memcpy(Header, romHeader, 16);
    hasBattery = romHasBattery;
    Mirroring = nesPpu.Mirroring = romMirroring;
    Version = romVersion;
    MapperID = romMapperID;
    SubMapperID = romSubMapperID;
    PRGRomSize = romPRGSize;
    CHRRomSize = romCHRSize;
    PRGNumPages = prgPages;
    CHRNumPages = chrPages;
    Region = GetRegion();

    delete[] ROM;
    delete[] CHR;

    ROM = new uint8_t[PRGRomSize];

    size_t offset = 16;

    memcpy(ROM, &data[offset], PRGRomSize);
    offset += PRGRomSize;

    if (chrPages) {
        CHR = new uint8_t[CHRRomSize];
        memcpy(CHR, &data[offset], CHRRomSize);
    } else {
        CHR = new uint8_t[mapper->getCHRRamSize()];
    }

    mapper->initialize();
    DebugPrintLog("ROM", "Loaded iNES ROM '%s'", Name.c_str());

    return true;
}

bool NesROM::loadUNIF(const std::vector<uint8_t> &data, const std::string &romName) {
    size_t offset = 32;

    std::string boardName;
    std::vector<uint8_t> prgData;
    std::vector<uint8_t> chrData;

    MirrorMode romMirroring = MirrorMode::HORIZONTAL;

    while (offset + 8 <= data.size()) {
        char chunkID[5] = {};
        memcpy(chunkID, &data[offset], 4);
        uint32_t chunkSize = data[offset + 4] | (data[offset + 5] << 8) | (data[offset + 6] << 16) | (data[offset + 7] << 24);

        offset += 8;
        if (offset + chunkSize > data.size()) {
            DebugPrintLog("ROM", "Invalid UNIF chunk size");
            return false;
        }

        if (strcmp(chunkID, "MAPR") == 0) {
            boardName.assign((char*)&data[offset], chunkSize);
            size_t nullPos = boardName.find('\0');
            if (nullPos != std::string::npos) boardName.resize(nullPos);
        } else if (strncmp(chunkID, "PRG", 3) == 0) {
            prgData.insert(prgData.end(), data.begin() + offset, data.begin() + offset + chunkSize);
        } else if (strncmp(chunkID, "CHR", 3) == 0) {
            chrData.insert(chrData.end(), data.begin() + offset, data.begin() + offset + chunkSize);
        } else if (strcmp(chunkID, "MIRR") == 0) {
            switch (data[offset]) {
                case 0:
                    romMirroring = MirrorMode::HORIZONTAL;
                    break;
                case 1:
                    romMirroring = MirrorMode::VERTICAL;
                    break;
                case 2:
                    romMirroring = MirrorMode::FOURSCREEN;
                    break;
                case 3:
                    romMirroring = MirrorMode::SCREEN_A;
                    break;
                case 4:
                    romMirroring = MirrorMode::SCREEN_B;
                    break;
            }
        }
        offset += chunkSize;
    }

    if (boardName.empty()) {
        DebugPrintLog("ROM", "UNIF ROM has no board name");
        return false;
    }

    uint16_t romMapperID = 0;
    if (boardName == "NES-NROM") {
        romMapperID = 0;
    } else if (boardName == "NES-SLROM" || boardName == "NES-SNROM") {
        romMapperID = 1;
    } else if (boardName == "NES-UNROM") {
        romMapperID = 2;
    } else if (boardName == "NES-CNROM") {
        romMapperID = 3;
    } else if (boardName == "NES-TLROM" || boardName == "NES-TSROM" || boardName == "NES-TKROM") {
        romMapperID = 4;
    } else if (boardName == "NES-MMC2") {
        romMapperID = 9;
    } else if (boardName == "NES-MMC4") {
        romMapperID = 10;
    } else {
        DebugPrintLog("ROM", "Unknown UNIF board '%s'", boardName.c_str());
        return false;
    }

    MapperBase* romMapperNew = GetMapper(romMapperID, 0);
    if (!romMapperNew) {
        DebugPrintLog("ROM", "Unsupported UNIF mapper %u", romMapperID);
        return false;
    }

    if (mapper) delete mapper;
    mapper = romMapperNew;
    nesCpu.romMapper = nesPpu.romMapper = mapper;

    Name = romName;
    Version = HeaderVersion::UNIF;
    MapperID = romMapperID;
    SubMapperID = 0;
    Mirroring = nesPpu.Mirroring = romMirroring;
    PRGRomSize = prgData.size();
    CHRRomSize = chrData.size();

    delete[] ROM;
    delete[] CHR;

    ROM = new uint8_t[PRGRomSize];
    memcpy(ROM, prgData.data(), PRGRomSize);

    if (CHRRomSize > 0) {
        CHR = new uint8_t[CHRRomSize];
        memcpy(CHR, chrData.data(), CHRRomSize);
    } else {
        CHR = new uint8_t[mapper->getCHRRamSize()];
    }

    mapper->initialize();

    DebugPrintLog("ROM", "Loaded UNIF ROM '%s' Board: %s", Name.c_str(), boardName.c_str());

    return true;
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
    if (!rom.read((char*)(data.data()), fsize)) {
        DebugPrintLog("ROM", "Failed to read ROM '%s'", filename.c_str());
        return false;
    }

    if (data.size() >= 4 && data[0] == 'N' && data[1] == 'E' && data[2] == 'S' && data[3] == 0x1A) {
        return loadINES(data, romName);
    }

    if (data.size() >= 4 && data[0] == 'U' && data[1] == 'N' && data[2] == 'I' && data[3] == 'F') {
        return loadUNIF(data, romName);
    }

    DebugPrintLog("ROM", "Unknown ROM format");
    return false;
}