#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

// savestate header, "nya~"
#define NYA_SIGNATURE 0x7E61796E

class SaveStateFile {
public:
    FILE *File = nullptr;
    uint32_t FileSize = 0;
    uint8_t *Data = nullptr;
    uint32_t Offset = 0;
    bool ReadOnly = true;

    void OpenFileR(const char *Name);
    void OpenFileW(const char *Name);

    template <typename T>
    T ReadBytes() {
        T Buf{};
        constexpr size_t RealSize = sizeof(T);
        if ((uint32_t)(Offset + RealSize) > FileSize) return Buf;
        memcpy(&Buf, Data + Offset, RealSize);
        Offset += RealSize;
        return Buf;
    }

    template <typename T>
    T *ReadLenBytes(T *Buf, uint32_t Len) {
        size_t RealSize = sizeof(T) * Len;
        if ((uint32_t)(Offset + RealSize) > FileSize) return Buf;
        memcpy(Buf, Data + Offset, RealSize);
        Offset += RealSize;
        return Buf;
    }

    template <typename T>
    bool WriteBytes(T Value) {
        size_t Written = fwrite(&Value, sizeof(T), 1, File);
        return Written == 1;
    }

    template <typename T>
    bool WriteLenBytes(T *Buf, uint32_t Len) {
        size_t Written = fwrite(Buf, sizeof(T), Len, File);
        return Written == Len;
    }

    void CloseFile(void);
    void Write(const char *FileName);
    void Load(const char *FileName);
};