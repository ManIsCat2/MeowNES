#include "nes_ppu.hpp"
#include "nes_cpu.hpp"
#include <cstring>

PPU ppu;

void PPU::Step() {
    Dot++;
    if (Dot > 341) {
        Dot = 0;
        ScanLine++;
        if (ScanLine == 241) Vblank = true;
        if (ScanLine == 261) Vblank = false;
        if (ScanLine > 261) {
            ScanLine = 0;
        }
    }
}

void PPU::LoadCHRROM(const uint8_t* chrData, int chrSize) {
    if (chrSize > 0x2000) chrSize = 0x2000;
    std::memcpy(&ChrROM[0x0000], chrData, chrSize);
}

SDL_Window* window = nullptr;
SDL_Texture* texture = nullptr;

bool PPU::InitSDL(SDL_Renderer * renderer) {
    PaletteMode = 0;
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING, NES_WIDTH, NES_HEIGHT);
    return texture != nullptr;
}

void PPU::ShutdownSDL() {
    if (texture) SDL_DestroyTexture(texture);
    SDL_Quit();
}

// dummy, no finish for now
const uint32_t nesPaletteNTSC[64] = {
    0xFF757575,0xFF271B8F,0xFF0000AB,0xFF47009F,0xFF8F0077,0xFFAB0013,0xFFA70000,0xFF7F0B00,
    0xFF432F00,0xFF004700,0xFF005100,0xFF003F17,0xFF1B3F5F,0xFF000000,0xFF000000,0xFF000000,
    0xFFBCBCBC,0xFF0073EF,0xFF233BEF,0xFF8300F3,0xFFBF00BF,0xFFE7005B,0xFFDB2B00,0xFFCB4F0F,
    0xFF8B7300,0xFF009F0F,0xFF00AB00,0xFF00933B,0xFF00838B,0xFF000000,0xFF000000,0xFF000000,
    0xFFFFFFFF,0xFF3FBFFF,0xFF5F97FF,0xFFA78BFD,0xFFF77BFF,0xFFFF77B7,0xFFFF7763,0xFFFF9B3B,
    0xFFF3BF3F,0xFF83D313,0xFF4FDF4B,0xFF58F898,0xFF00EBDB,0xFF000000,0xFF000000,0xFF000000,
    0xFFFFFFFF,0xFFA7E7FF,0xFFC7D7FF,0xFFD7CBFF,0xFFFFC7FF,0xFFFFC7DB,0xFFFFBFB3,0xFFFFDBAB,
    0xFFFFE7A3,0xFFE3FFA3,0xFFABF3BF,0xFFB3FFCF,0xFF9FFFF3,0xFF000000,0xFF000000,0xFF000000
};

const uint32_t nesPalettePAL[64] = {
    0xFF6D6D6D,0xFF002492,0xFF0010A8,0xFF440096,0xFFA80020,0xFFA81000,0xFF881400,0xFF503000,
    0xFF007008,0xFF006010,0xFF005840,0xFF004058,0xFF000000,0xFF000000,0xFF000000,0xFF000000,
    0xFFB6B6B6,0xFF2048D8,0xFF4030E0,0xFF9020CC,0xFFE01070,0xFFE03020,0xFFC84000,0xFF886000,
    0xFF208800,0xFF00A000,0xFF00A848,0xFF008088,0xFF000000,0xFF000000,0xFF000000,0xFF000000,
    0xFFFFFFFF,0xFF60A0FF,0xFF8080FF,0xFFC060FF,0xFFFF60E0,0xFFFF60A0,0xFFFF8040,0xFFFFA020,
    0xFFE0C020,0xFFA0E020,0xFF40E060,0xFF20C0C0,0xFF40A0E0,0xFF000000,0xFF000000,0xFF000000,
    0xFFFFFFFF,0xFFA0D0FF,0xFFC0B0FF,0xFFE0A0FF,0xFFFFA0F0,0xFFFFA0C0,0xFFFFC0A0,0xFFFFE080,
    0xFFE0E060,0xFFC0F060,0xFF80F0A0,0xFF60E0E0,0xFF80C0F0,0xFF000000,0xFF000000,0xFF000000
};


void PPU::Render(SDL_Renderer* renderer) {
    uint32_t pixels[NES_WIDTH * NES_HEIGHT];
    uint8_t palOffset = 4;

    if (UseRandPalIndex)
        palOffset = RanPalIndex;

    uint32_t nesPalette[64] = {0};
    const uint32_t* activePalette = (PaletteMode == 0) ? nesPaletteNTSC : nesPalettePAL;
    memcpy(nesPalette, activePalette, sizeof(nesPalette));

    if (PaletteMode == 1) {
        for (int i = 0; i < 64; i++) {
            uint8_t r = (nesPalette[i] >> 16) & 0xFF;
            uint8_t g = (nesPalette[i] >> 8) & 0xFF;
            uint8_t b = (nesPalette[i] >> 0) & 0xFF;
            r = static_cast<uint8_t>(r * 0.95f);
            g = static_cast<uint8_t>(g * 0.95f);
            b = static_cast<uint8_t>(b * 0.98f);
            nesPalette[i] = (r << 16) | (g << 8) | b;
        }
    }

    for (int screenY = 0; screenY < NES_HEIGHT; screenY++) {
        for (int screenX = 0; screenX < NES_WIDTH; screenX++) {
            int scrolledX = (screenX + scrollX) % 256;
            int scrolledY = (screenY + scrollY) % 240;
            int tileX = scrolledX / 8;
            int tileY = scrolledY / 8;
            int fineX = scrolledX % 8;
            int fineY = scrolledY % 8;

            uint8_t tileIndex = VRAM[tileY * 32 + tileX];

            int useSecond = BGPatternTable ? 0x1000 : 0x0000;
            uint8_t lo = ChrROM[tileIndex * 16 + fineY + useSecond];
            uint8_t hi = ChrROM[tileIndex * 16 + fineY + 8 + useSecond];

            uint8_t attrOffset = (tileX / 4) + (tileY / 4) * 8;
            uint8_t attributes = VRAM[0x3C0 + attrOffset];
            uint8_t quadrant = ((tileX / 2) & 1) + (((tileY / 2) & 1) * 2);
            uint8_t pair = (attributes >> (quadrant * 2)) & 3;

            int bit = 7 - fineX;
            int twoBit = ((hi >> bit) & 1) << 1 | ((lo >> bit) & 1);
            uint8_t palIndex = twoBit ? paletteRAM[twoBit + pair * palOffset] : paletteRAM[0];
            uint32_t color = nesPalette[palIndex & 0x3F];

            pixels[screenY * NES_WIDTH + screenX] = color;
        }
    }

    for (int i = 0; i < 64; i++) {
        int spriteY = OAM[i * 4 + 0] + 1;
        int tile = OAM[i * 4 + 1];
        int attr = OAM[i * 4 + 2];
        int spriteX = OAM[i * 4 + 3];

        bool flipH = attr & 0x40;
        bool flipV = attr & 0x80;
        uint8_t paletteIndex = attr & 0x03;
        uint16_t spriteTable = spritePatternTable ? 0x1000 : 0x0000;
        const uint8_t* tileData = &ChrROM[spriteTable + tile * 16];

        for (int row = 0; row < 8; row++) {
            int tileRow = flipV ? 7 - row : row;
            uint8_t plane0 = tileData[tileRow];
            uint8_t plane1 = tileData[tileRow + 8];

            for (int col = 0; col < 8; col++) {
                int tileCol = flipH ? 7 - col : col;
                uint8_t colorLow  = (plane0 >> (7 - tileCol)) & 1;
                uint8_t colorHigh = (plane1 >> (7 - tileCol)) & 1;
                uint8_t colorId = (colorHigh << 1) | colorLow;
                if (colorId == 0) continue;

                int px = spriteX + col;
                int py = spriteY + row;
                if (px < 0 || px >= NES_WIDTH || py < 0 || py >= NES_HEIGHT) continue;

                uint8_t palEntry = paletteRAM[(palOffset * 4) + (paletteIndex * 4) + colorId] & 0x3F;
                pixels[py * NES_WIDTH + px] = nesPalette[palEntry];
            }
        }
    }

    SDL_UpdateTexture(texture, nullptr, pixels, NES_WIDTH * sizeof(uint32_t));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
    // SDL_RenderPresent(renderer);
}
