#include "savestate.hpp"
#include "nes/nes_cpu.hpp"
#include "nes/nes_ppu.hpp"
#include "nes/nes_apu.hpp"
#include <cstdint>

void SaveStateFile::OpenFileR(const char *Name) {
    File = fopen(Name, "rb");
    if (!File) {
        DebugPrintLog("SAVESTATE", "Can't open \"%s\"", Name);
        exit(1);
    }
    fseek(File, 0, SEEK_END);
    FileSize = ftell(File);
    fseek(File, 0, SEEK_SET);
    Data = (uint8_t *) malloc(FileSize);
    fread(Data, 1, FileSize, File);
    ReadOnly = true;
}

void SaveStateFile::OpenFileW(const char *Name) {
    File = fopen(Name, "wb");
    if (!File) {
        DebugPrintLog("SAVESTATE", "Can't open \"%s\"", Name);
        exit(1);
    }
    ReadOnly = false;
}

void SaveStateFile::CloseFile(void) {
    if (ReadOnly) free(Data);
    fclose(File);
}

void SaveStateFile::Write(const char *FileName) {
    OpenFileW(FileName);
    
    WriteBytes<uint32_t>(NYA_SIGNATURE); // "nya~"

    uint8_t dummy[1020];
    WriteLenBytes<uint8_t>(dummy, 1020);

    //getNESRom()->mapper->saveState(*this);

    CloseFile();
}

void SaveStateFile::Load(const char *FileName) {
    OpenFileR(FileName);

    uint32_t sig = ReadBytes<uint32_t>();
    if (sig != NYA_SIGNATURE) {
        DebugPrintLog("SAVESTATE", "File has invalid savestate header");
        CloseFile();
        return;
    }

    uint8_t dummy[1020];
    ReadLenBytes<uint8_t>(dummy, 1020);
    
    //getNESRom()->mapper->loadState(*this);

    DebugPrintLog("SAVESTATE", "Loaded Savestate '%s'", FileName);
    CloseFile();
}