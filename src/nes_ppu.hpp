#pragma once
#include <array>
#include <cstdint>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

class PPU {
public:
    std::array<uint8_t, 0x4000> ChrROM{};
    std::array<uint8_t, 0x4000> VRAM{};
    std::array<uint8_t, 0x20> paletteRAM{};
    std::array<uint8_t, 256> OAM{};

    bool WriteLatch = false;
    unsigned short TransferAddr = 0;
    unsigned short VRAMAddr = 0;
    unsigned short OAMAddr = 0;
    unsigned short TempVRAMAddr = 0;
    uint8_t ReadBuffer = 0;
    int Dot = 0;
    int ScanLine = 0;
    bool Vblank = false;

    bool mask8pxMaskBG = false;
    bool mask8pxMaskSprites = false;
    bool maskRenderBG = false;
    bool maskRenderSprites = false;

    int nametableSelect = 0; 
    bool VRAMInc32Mode = false;
    bool spritePatternTable = false;
    bool BGPatternTable = false;
    bool use8x16Sprites = false;
    bool enableNMI = false;

    uint16_t scrollX = 0;  // horizontal scroll in pixels
    uint16_t scrollY = 0;  // vertical scroll in pixels
    uint8_t scrollFineX = 0; // 0-7, fine pixel shift inside a tile

    void Step();

    void LoadCHRROM(const uint8_t* chrData, int chrSize);

    bool InitSDL(SDL_Renderer * renderer);
    void ShutdownSDL();
    void Render(SDL_Renderer * renderer);
};

extern PPU ppu;