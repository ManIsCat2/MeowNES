#pragma once
#include <array>
#include <cstdint>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

class PPU {
public:
    std::array<uint8_t, 0x4000> chrROM{};
    std::array<uint8_t, 0x4000> VRAM{};
    std::array<uint8_t, 0x20> paletteRAM{};
    std::array<uint8_t, 256> OAM{};

    bool writeLatch = false;
    unsigned short transferAddr = 0;
    unsigned short VRAMAddr = 0;
    unsigned short OAMAddr = 0;
    unsigned short tempVRAMAddr = 0;
    uint8_t readBuf = 0;
    int dot = 0;
    int scanline = 0;
    bool vblank = false;

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


    static constexpr int NES_WIDTH = 256;
    static constexpr int NES_HEIGHT = 240;

    void step();

    void loadCHR(const uint8_t* chrData, int chrSize);

    bool initSDL(SDL_Renderer * renderer);
    void shutdownSDL();
    void renderPPU(SDL_Renderer * renderer);
};

extern PPU ppu;