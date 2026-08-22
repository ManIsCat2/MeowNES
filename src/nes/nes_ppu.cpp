#include "nes_ppu.hpp"
#include "nes_cpu.hpp"
#include <cstring>

#define NES_WIDTH  256
#define NES_HEIGHT 240

#define VRAM_FIY 0x7000 //0b111000000000000, fine Y
#define VRAM_X_NT 0x0400 //0b000010000000000, X nametable
#define VRAM_Y_NT 0x0800 //0b000100000000000, Y nametable
#define VRAM_COY 0x03E0 //0b000001111100000, coarse Y
#define VRAM_COX 0x001F //0b000000000011111, coarse X

#define PPU_BUS_DECAY_TIME 1696132
#define PPU_PIXEL_COUNT_NTSC (NES_NTSC_OUT_WIDTH(NES_WIDTH) * NES_HEIGHT)
#define PPU_PIXEL_COUNT (NES_WIDTH * NES_HEIGHT)

NesPPU nesPpu;

uint32_t nesPalette[64] = {
    0x656565, 0x002A84, 0x1513A2, 0x3A019E, 0x59007A, 0x6A003E, 0x680800, 0x531D00, 0x323400, 0x0D4600, 0x004F00, 0x004C09, 0x003F4B, 0x000000, 0x000000, 0x000000,
    0xAEAEAE, 0x175FD6, 0x4341FF, 0x7529FA, 0x9E1DCA, 0xB4207B, 0xB13322, 0x964E00, 0x6A6C00, 0x398400, 0x0F9000, 0x008D33, 0x007B8C, 0x000000, 0x000000, 0x000000,
    0xFEFFFF, 0x66AFFF, 0x9390FF, 0xC578FF, 0xEE6CFF, 0xFF6FCA, 0xFF8271, 0xE69E25, 0xBABC00, 0x88D501, 0x5EE132, 0x47DD82, 0x4ACBDC, 0x4E4E4E, 0x000000, 0x000000,
    0xFEFFFF, 0xC0DEFF, 0xD2D1FF, 0xE7C7FF, 0xF8C2FF, 0xFFC3E9, 0xFFCBC4, 0xF5D7A5, 0xE2E394, 0xCEED96, 0xBCF2AA, 0xB3F1CB, 0xB4E9F0, 0xB6B6B6, 0x000000, 0x000000,
};

uint32_t nesPaletteDefault[64] = {
    0x656565, 0x002A84, 0x1513A2, 0x3A019E, 0x59007A, 0x6A003E, 0x680800, 0x531D00, 0x323400, 0x0D4600, 0x004F00, 0x004C09, 0x003F4B, 0x000000, 0x000000, 0x000000,
    0xAEAEAE, 0x175FD6, 0x4341FF, 0x7529FA, 0x9E1DCA, 0xB4207B, 0xB13322, 0x964E00, 0x6A6C00, 0x398400, 0x0F9000, 0x008D33, 0x007B8C, 0x000000, 0x000000, 0x000000,
    0xFEFFFF, 0x66AFFF, 0x9390FF, 0xC578FF, 0xEE6CFF, 0xFF6FCA, 0xFF8271, 0xE69E25, 0xBABC00, 0x88D501, 0x5EE132, 0x47DD82, 0x4ACBDC, 0x4E4E4E, 0x000000, 0x000000,
    0xFEFFFF, 0xC0DEFF, 0xD2D1FF, 0xE7C7FF, 0xF8C2FF, 0xFFC3E9, 0xFFCBC4, 0xF5D7A5, 0xE2E394, 0xCEED96, 0xBCF2AA, 0xB3F1CB, 0xB4E9F0, 0xB6B6B6, 0x000000, 0x000000,
};

float rainbowHoverPhase = 0.0f;
uint32_t getRainbowColor() {
    float h = rainbowHoverPhase * 360.0f;
    float s = 1.0f;
    float v = 1.0f;

    float c = v * s;
    float x = c * (1 - fabs(fmod(h / 60.0f, 2) - 1));
    float m = v - c;

    float r1, g1, b1;
    if (h < 60)       { r1 = c; g1 = x; b1 = 0; }
    else if (h < 120) { r1 = x; g1 = c; b1 = 0; }
    else if (h < 180) { r1 = 0; g1 = c; b1 = x; }
    else if (h < 240) { r1 = 0; g1 = x; b1 = c; }
    else if (h < 300) { r1 = x; g1 = 0; b1 = c; }
    else              { r1 = c; g1 = 0; b1 = x; }

    uint8_t r = (uint8_t)((r1 + m) * 255);
    uint8_t g = (uint8_t)((g1 + m) * 255);
    uint8_t b = (uint8_t)((b1 + m) * 255);

    return  (r << 16) | (g << 8) | b;
}

NesPPU::NesPPU() {
    frameBuffer = new uint32_t[PPU_PIXEL_COUNT_NTSC];
    palIndexBuf = new uint8_t[PPU_PIXEL_COUNT];
    bgMaskBuf = new uint8_t[PPU_PIXEL_COUNT];
    rawOutputImage = new QImage((uint8_t*)(frameBuffer), NES_WIDTH, NES_HEIGHT, QImage::Format_RGB32);
    filteredOutputImage = new QImage((uint8_t*)(frameBuffer), NES_NTSC_OUT_WIDTH(256), NES_HEIGHT, QImage::Format_RGB32);
    initFilter(VideoFilter::NONE);
}

NesPPU::~NesPPU() {
    delete[] frameBuffer;
    delete[] palIndexBuf;
    delete[] bgMaskBuf;
    delete rawOutputImage;
    delete filteredOutputImage;
    delete vfilter;
}

void NesPPU::reset(void) {
    memset(VRAM.data(), 0, NES_VRAM_SIZE);
    memset(OAM, 0, 0x100);
    memset(paletteRAM.data(), 0, NES_PALRAM_SIZE);
    memset(frameBuffer, 0, sizeof(uint32_t) * PPU_PIXEL_COUNT_NTSC);
    memset(palIndexBuf, 0, PPU_PIXEL_COUNT);
    dataBus = 0;
    for (int i = 0; i < 8; i++) { 
        busDecayTimers[i] = 0;
    }
    WriteLatch = false;
    VRAMAddr = 0;
    OAMAddr = 0;
    TransferAddr = 0;
    ReadBuffer = 0;
    Dot = 0;
    ScanLine = 0;
    Vblank = false;
    sprite0Hit = false;
    spriteOverflow = false;

    mask.background8pxMask = false;
    mask.sprite8pxMask = false;
    mask.renderBackground = false;
    mask.renderSprites = false;

    control.nametableSelect = 0; 
    control.VRAMInc32 = false;
    control.spritePatternTable = 0;
    control.BGPatternTable = 0;
    control.use8x16Sprites = false;
    control.enableNMI = false;

    scrollFineX = 0;
}

void NesPPU::resetBusDecay(void) {
    for (int i = 0; i < 8; i++) { 
        busDecayTimers[i] = PPU_BUS_DECAY_TIME;
    }
}

void NesPPU::decayDataBus(void) {
    for (int i = 0; i < 8; i++) {
        if (busDecayTimers[i] != 0) {
            busDecayTimers[i]--;
            if (busDecayTimers[i] == 0) {
                dataBus &= busDecayMasks[i];
            }
        }
    }
}

uint16_t NesPPU::getAttributeByte() {
    uint16_t attrAddr = (VRAMAddr & 0x0C00) | 0x03C0 | ((VRAMAddr >> 4) & 0x38) | ((VRAMAddr >> 2) & 0x07);
    uint8_t attr = romMapper->readVRAM(attrAddr);
    uint8_t shift = ((VRAMAddr >> 4) & 4) | (VRAMAddr & 2);

    return (attr >> shift) & 0x03;
}

void NesPPU::RenderScreen() {
    if ((uint32_t)(Dot - NES_WIDTH) <= 63) return;

    bool renderBG = mask.renderBackground;
    bool renderSPR = mask.renderSprites;

    if (Dot < NES_WIDTH) {
        uint8_t bgColor = 0;
        uint8_t bgPalette = 0;

        if (renderBG && (Dot >= 8 || mask.background8pxMask)) {
            uint16_t bit = 0x8000 >> scrollFineX;
            bgColor = ((shiftRegHigh & bit) ? 2 : 0) | ((shiftRegLow  & bit) ? 1 : 0);
            if (bgColor != 0) {
                bgMaskBuf[ScanLine * NES_WIDTH + Dot] = 1;
            } else {
                bgMaskBuf[ScanLine * NES_WIDTH + Dot] = 0;
            }
            bgPalette = ((((shiftAttrHigh & bit) ? 2 : 0) | ((shiftAttrLow  & bit) ? 1 : 0)) << 2);
        }

        uint8_t finalColor = bgColor;
        uint8_t finalPalette = bgPalette;
        bool shadowPixel = false;
        bool spritePixelVisible = false;

        if (renderSPR) {
            uint16_t spriteH = control.use8x16Sprites ? 16 : 8;
            for (int i = 0; i < 256; i += 4) {
                uint8_t* sprite = &OAM[i];

                if ((AddShadows & 0x01) && !DisableSprites && i != 0) {
                    int shadowSpriteX = (Dot - 2) - sprite[3];
                    int shadowSpriteY = (ScanLine - 2) - sprite[0] - 1;
                    if ((uint32_t)shadowSpriteX < 8 && (uint32_t)shadowSpriteY < spriteH) {
                        int flipH = sprite[2] & 0x40;
                        int flipV = sprite[2] & 0x80;

                        int sx = flipH ? shadowSpriteX : (7 - shadowSpriteX);
                        int sy = flipV ? (spriteH - 1 - shadowSpriteY) : shadowSpriteY;

                        uint16_t spriteTile = sprite[1];
                        uint16_t spriteAddress = (control.use8x16Sprites ? ((spriteTile & 1) << 12) | ((spriteTile & 0xFE) << 4) | ((sy & 8) << 1) : ((control.spritePatternTable << 9) | (spriteTile << 4))) | (sy & 7);
                        uint8_t shadowColor = (((romMapper->readCHR(spriteAddress + 8, true) >> sx) & 1) << 1) | ((romMapper->readCHR(spriteAddress, true) >> sx) & 1);

                        if (shadowColor) shadowPixel = true;
                    }
                }

                uint16_t spriteX = Dot - sprite[3];
                uint16_t spriteY = ScanLine - sprite[0] - 1;
                if (spriteX < 8 && spriteY < spriteH) {
                    int flipH = sprite[2] & 0x40;
                    int flipV = sprite[2] & 0x80;

                    uint16_t sx = flipH ? spriteX : (7 - spriteX);
                    uint16_t sy = flipV ? (spriteH - 1 - spriteY) : spriteY;

                    uint16_t spriteTile = sprite[1];
                    uint16_t spriteAddress = (control.use8x16Sprites ? spriteTile % 2 << 12 | spriteTile << 4 & -32 | sy * 2 & 0x10 : control.spritePatternTable << 9 | spriteTile << 4) | (sy & 7);
                    uint16_t spriteColor = (romMapper->readCHR(spriteAddress + 8, true) >> sx << 1 & 2) | (romMapper->readCHR(spriteAddress, true) >> sx & 1);

                    if (spriteColor) {
                        spritePixelVisible = true;

                        if (!(sprite[2] & 0x20 && bgColor) && !DisableSprites) {
                            finalColor = spriteColor;
                            finalPalette = 0x10 | (sprite[2] * 4 & 0x0C);                                                    
                        }

                        if (i == 0 && spriteColor != 0 && bgColor != 0 && Dot != 255 && renderBG && (Dot >= 8 || mask.sprite8pxMask)) {
                            sprite0Hit = true;
                        }

                        break;
                    }
                }
            }
        }

        if (AddShadows & 0x02) {
            int shadowX = Dot - 2;
            int shadowY = ScanLine - 2;

            if (shadowX >= 0 && shadowY >= 0) {
                if (bgColor == 0) {
                    if (bgMaskBuf[shadowY * NES_WIDTH + shadowX]) {
                        shadowPixel = true;
                    }
                }
            }
        }

        uint8_t palIndex = 0;
        if (shadowPixel && !spritePixelVisible) {
            palIndex = 0x0D;
        } else {
            palIndex = paletteRAM[finalColor ? (finalPalette | finalColor) : 0] & 0x3F;
        }
        if (palIndex == hoveredPaletteIndex) palIndex = 254;
        palIndexBuf[ScanLine * NES_WIDTH + Dot] = palIndex;
    }

    if (Dot < 336) {
        shiftRegHigh <<= 1;
        shiftRegLow <<= 1;
        shiftAttrLow <<= 1;
        shiftAttrHigh <<= 1;
    }

    uint16_t fetchAddress = ((control.BGPatternTable) ? 0x1000 : 0x0000) | (nametableByte << 4) | ((VRAMAddr >> 12) & 7);
    switch ((Dot + 1) & 7) {
        case 0: {
            shiftRegLow = (shiftRegLow & 0xFF00) | patternTableLow;
            shiftRegHigh = (shiftRegHigh & 0xFF00) | patternTableHigh;
            shiftAttrLow = (shiftAttrLow & 0xFF00) | ((attributeByte & 1) ? 0xFF : 0x00);
            shiftAttrHigh = (shiftAttrHigh & 0xFF00) | ((attributeByte & 2) ? 0xFF : 0x00);

            if ((VRAMAddr & VRAM_COX) == VRAM_COX) {
                VRAMAddr &= ~VRAM_COX;
                VRAMAddr ^= VRAM_X_NT;
            } else {
                VRAMAddr++;
            }
            break;
        }

        case 1:
            nametableByte = romMapper->readVRAM(VRAMAddr);
            break;

        case 3: {
            attributeByte = getAttributeByte();
            break;
        }

        case 5:
            patternTableLow = romMapper->readCHR(fetchAddress, false);
            break;

        case 7:
            patternTableHigh = romMapper->readCHR(fetchAddress + 8, false);
            break;
    }
}

void NesPPU::Step() {
    const bool isPal = /*getNESRom()->Region == ConsoleRegion::PAL*/ false;
    const int preRenderLine = isPal ? 311 : 261;
    const int totalScanlines = isPal ? 312 : 262;

    if (Dot == 1 && ScanLine == 241) VblankPending = true;
    if (Dot == 0 && ScanLine == 241) {
        if (vfilter->hasCustomBlit()) {
            vfilter->blit();
        } else {
            blitPixels();
        }
    }

    if (Dot == 1 && ScanLine == preRenderLine) {
        Vblank = false;
        sprite0Hit = false;
        spriteOverflow = false;

        memset(bgMaskBuf, 0, PPU_PIXEL_COUNT);
    }

    if (mask.renderBackground || mask.renderSprites) {
        if (ScanLine < 240) {
            RenderScreen();

            if (Dot == 256) {
                uint16_t spriteH = control.use8x16Sprites ? 16 : 8;
                int spriteCount = 0;
                int oamIndex = 0;

                for (; oamIndex < 256; oamIndex += 4) {
                    uint16_t spriteY = ScanLine - OAM[oamIndex];
                    if (spriteY < spriteH) {
                        spriteCount++;
                        if (spriteCount == 8) {
                            oamIndex += 4;
                            break;
                        }
                    }
                }

                if (spriteCount == 8) {
                    int subOffset = 0;
                    while (oamIndex < 256) {
                        uint8_t glitchedY = OAM[oamIndex + subOffset];
                        uint16_t spriteY = ScanLine - glitchedY;

                        if (spriteY < spriteH) {
                            spriteOverflow = true;
                            break;
                        } else {
                            oamIndex += 4;
                            subOffset = (subOffset + 1) & 3; 
                        }
                    }
                }

                if ((VRAMAddr & VRAM_FIY) != VRAM_FIY) {
                    VRAMAddr += 0x1000;
                } else {
                    VRAMAddr &= 0x0FFF;
                    int y = (VRAMAddr & VRAM_COY) >> 5;
                    if (y == 29) {
                        y = 0;
                        VRAMAddr ^= VRAM_Y_NT;
                    } else if (y == 31) {
                        y = 0;
                    } else {
                        y++;
                        y &= 0x1F;
                    }
                    VRAMAddr = ((VRAMAddr & 0xFC1F) | (y << 5));
                }
            }

            if (Dot == 257) {
                VRAMAddr = ((VRAMAddr & 0x7be0) | (TransferAddr & 0x41f));
            }
        }

        if (Dot >= 280 && Dot <= 304 && ScanLine == preRenderLine) {
            VRAMAddr = ((VRAMAddr & 0x41f) | (TransferAddr & 0x7be0));
        }
        romMapper->clockPPU();
    }

    if (VblankPending) {
        VblankPending = false;
        if (!VblankSuppress) {
            Vblank = true;
        }
        VblankSuppress = false;
    }

    Dot++;
    if (Dot >= 341) {
        Dot = 0;
        ScanLine++;
        if (ScanLine >= totalScanlines) ScanLine = 0;
    }
    if (dataBus != 0) decayDataBus();
}

uint16_t NesPPU::mirrorNametable(uint16_t addr) {
    uint16_t normalized = (addr - 0x2000) & 0x0FFF;

    switch (Mirroring) {
        case MirrorMode::HORIZONTAL:
            return (normalized & 0x03FF) | ((normalized & 0x0800) >> 1);
        case MirrorMode::VERTICAL:
            return normalized & 0x07FF;
        case MirrorMode::SCREEN_A:
            return normalized & 0x03FF;
        case MirrorMode::SCREEN_B:
            return (normalized & 0x03FF) | 0x0400;
        case MirrorMode::FOURSCREEN:
            return normalized;
    }
    return 0;
}

void NesPPU::blitPixels() {
    for (int y = 0; y < NES_HEIGHT; y++) {
        for (int x = 0; x < NES_WIDTH; x++) {
            int i = y * NES_WIDTH + x;
            frameBuffer[i] = nesPalette[palIndexBuf[i] & 0x3f];
            vfilter->applyFilter(&frameBuffer[i], x, y);
            if (palIndexBuf[i] == 254) frameBuffer[i] = getRainbowColor();
        }
    }
}

void NesPPU::Init() {
    initFilter(VideoFilter::NONE);
    memset(frameBuffer, 0, sizeof(uint32_t) * PPU_PIXEL_COUNT_NTSC);
    memset(palIndexBuf, 0, PPU_PIXEL_COUNT);
}