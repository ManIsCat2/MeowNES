#include "gb_ppu.hpp"
#include "gb_cpu.hpp"
#include <cstring>
#include <algorithm>

struct OAMSprite {
    int index = 0;
    int y = 0;
    int x = 0;
    uint8_t tile = 0;
    uint8_t attributes = 0;
};

GbPPU gbPpu;

uint32_t gbPaletteDefault[4] = {
    0xFFFFFFFF,
    0xFFB0B0B0,
    0xFF686868,
    0xFF000000
};

uint32_t gbPalette[4] = {
    0xFFFFFFFF,
    0xFFB0B0B0,
    0xFF686868,
    0xFF000000
};

GbPPU::GbPPU() {
    initFilter(VideoFilter::NONE);
    rawOutputImage = new QImage((uint8_t*)frameBuffer, 160, 144, QImage::Format_RGB32);
    filteredOutputImage = new QImage((uint8_t*)(frameBuffer), NES_NTSC_OUT_WIDTH(160), 144, QImage::Format_RGB32);
}

GbPPU::~GbPPU() {
    delete rawOutputImage;
    delete filteredOutputImage;
    delete vfilter;
}

void GbPPU::reset() {
    connectBus(&gbCpu, nullptr);

    memset(VRAM, 0, sizeof(VRAM));
    memset(OAM, 0, sizeof(OAM));
    memset(frameBuffer, 0, sizeof(frameBuffer));
    memset(BGPaletteRAM, 0, sizeof(BGPaletteRAM));
    memset(SPPaletteRAM, 0, sizeof(SPPaletteRAM));

    VBK = 0;
    BCPS = 0; BCPD = 0;
    OCPS = 0; OCPD = 0;
    OPRI = 0;
    LCDC = 0x91;
    STAT = 0x85;
    SCY  = 0x00;
    SCX  = 0x00;
    LY   = 0x00;
    LYC  = 0x00;
    DMA  = 0xFF;
    BGP  = 0xFC;
    OBP0 = 0xFF;
    OBP1 = 0xFF;
    WY   = 0x00;
    WX   = 0x00;

    windowLine = 0;
    scanlineCounter = 456;
    windowYLatch = false;
}

void GbPPU::Step(uint8_t cycles) {
    if (!(LCDC & 0x80)) {
        LY = 0;
        windowLine = 0;
        scanlineCounter = 456;
        STAT = (STAT & ~0x03);
        windowYLatch = false;
        return;
    }

    scanlineCounter -= cycles;

    while (scanlineCounter <= 0) {
        scanlineCounter += 456;
        LY++;

        if (LY == WY) {
            windowYLatch = true;
        }

        if (LY == 144) {
            cpu->IF |= 0x01;

            if (vfilter->hasCustomBlit()) {
                vfilter->blit();
            } else {
                blitPixels();
            }
        }

        if (LY > 153) {
            windowLine = 0;
            LY = 0;
            windowYLatch = false;
            if (LY == WY) windowYLatch = true; 
        }

        if (LY == LYC) {
            STAT |= 0x04;

            if (STAT & 0x40) {
                cpu->IF |= 0x02;
            }
        } else {
            STAT &= ~0x04;
        }
    }

    uint8_t oldMode = STAT & 0x03;
    uint8_t newMode;

    if (LY >= 144) {
        newMode = 1;
    } else if (scanlineCounter >= 376) {
        newMode = 2;
    } else if (scanlineCounter >= 204) {
        newMode = 3;
    } else {
        newMode = 0;
    }

    if (newMode != oldMode) {
        if (oldMode == 3 && newMode == 0) {
            RenderScanline();
        }

        switch (newMode) {
            case 0:
                if (STAT & 0x08) cpu->IF |= 0x02;
                break;

            case 1:
                if (STAT & 0x10) cpu->IF |= 0x02;
                break;

            case 2:
                if (STAT & 0x20) cpu->IF |= 0x02;
                break;
        }

        STAT = (STAT & ~0x03) | newMode;
    }
}

void GbPPU::RenderScanline() {
    bool isCGB = getGBRom()->isCGB;
    bool isRealCGB = (getGBRom()->isRealCGB || cpu->useBootROM);
    bool masterPriority = (LCDC & 0x01);

    if (!isCGB && !masterPriority) {
        for (int pixel = 0; pixel < 160; pixel++) {
            frameBuffer[LY * 160 + pixel] = gbPalette[0];
        }
        return;
    }

    uint16_t bgTileMap = (LCDC & 0x08) ? 0x9C00 : 0x9800;
    uint16_t winTileMap = (LCDC & 0x40) ? 0x9C00 : 0x9800;
    uint16_t tileData = (LCDC & 0x10) ? 0x8000 : 0x8800;
    bool isUnsigned = (LCDC & 0x10);

    uint8_t bgLineColorIds[160] = {0};
    bool bgPriorityLine[160] = {false};
    bool windowDrawn = false;

    uint16_t bgYPos = (uint16_t)(SCY + LY);
    uint16_t bgTileRow = ((bgYPos / 8) & 31) * 32;
    uint8_t bgLineY = bgYPos % 8;

    uint8_t winY = windowLine;
    uint16_t winTileRow = ((winY / 8) & 31) * 32;
    uint8_t winLineY = winY % 8;

    int wx = WX - 7;
    bool windowEnabled = (LCDC & 0x20) && windowYLatch;

    for (int pixel = 0; pixel < 160; pixel++) {
        bool useWindow = windowEnabled && (pixel >= wx);
        
        uint16_t tileMap, tileRow;
        uint8_t lineY, localX;

        if (useWindow) {
            windowDrawn = true;
            int windowPixelX = pixel - wx;
            tileMap = winTileMap;
            tileRow = winTileRow;
            lineY = winLineY;
            localX = windowPixelX;
        } else {
            uint16_t xPos = (uint16_t)(pixel + SCX);
            tileMap = bgTileMap;
            tileRow = bgTileRow;
            lineY = bgLineY;
            localX = xPos;
        }

        uint16_t tileCol = (localX / 8) & 31;
        uint16_t tileAddress = tileMap + tileRow + tileCol;

        uint8_t attributes = isRealCGB ? VRAM[(tileAddress - 0x8000) + 8192] : 0;
        uint8_t cgbPalette = attributes & 0x07;
        uint8_t cgbTileBank = (attributes & 0x08) >> 3;
        bool cgbXFlip = attributes & 0x20;
        bool cgbYFlip = attributes & 0x40;
        bool cgbBgOamPrior = attributes & 0x80;

        uint16_t tileDataLocation;
        if (isUnsigned) {
            uint8_t tileNum = VRAM[(tileAddress - 0x8000)];
            tileDataLocation = tileData + (tileNum * 16);
        } else {
            int8_t tileNum = (int8_t)VRAM[(tileAddress - 0x8000)];
            tileDataLocation = 0x9000 + (tileNum * 16);
        }

        uint8_t effectiveLineY = cgbYFlip ? (7 - lineY) : lineY;
        uint16_t lineOffset = effectiveLineY * 2;
        uint16_t bankOffset = isCGB ? (cgbTileBank * 8192) : 0;

        uint8_t byte1 = VRAM[(tileDataLocation + lineOffset - 0x8000) + bankOffset];
        uint8_t byte2 = VRAM[(tileDataLocation + lineOffset + 1 - 0x8000) + bankOffset];

        int bitBit = cgbXFlip ? (localX % 8) : (7 - (localX % 8));
        uint8_t colorId = (((byte2 >> bitBit) & 1) << 1) | ((byte1 >> bitBit) & 1);

        bgLineColorIds[pixel] = colorId;
        bgPriorityLine[pixel] = isCGB ? cgbBgOamPrior : false;

        if (isCGB) {
            uint8_t palBase = (cgbPalette * 8) + (colorId * 2);
            uint16_t colorData = BGPaletteRAM[palBase] | (BGPaletteRAM[palBase + 1] << 8);
            uint32_t finalColor = 0xFF000000 | (((colorData & 0x001F) << 3) << 16) | (((colorData & 0x03E0) >> 2) << 8) | (((colorData & 0x7C00) >> 7));
            frameBuffer[LY * 160 + pixel] = finalColor;
        } else {
            uint8_t colorPaletteShade = (BGP >> (colorId * 2)) & 0x03;
            frameBuffer[LY * 160 + pixel] = gbPalette[colorPaletteShade];
        }
    }

    if (windowDrawn) {
        windowLine++;
    }

    if (!(LCDC & 0x02)) return;

    bool use8x16 = (LCDC & 0x04);
    int spriteHeight = use8x16 ? 16 : 8;
    int spritesFound = 0;
    OAMSprite sprites[10];

    for (int i = 0; i < 40; i++) {
        uint16_t oamBase = i * 4;
        int spriteY = OAM[oamBase] - 16;
        
        if (LY >= spriteY && LY < (spriteY + spriteHeight)) {
            sprites[spritesFound] = {
                i,
                spriteY,
                OAM[oamBase + 1] - 8,
                OAM[oamBase + 2],
                OAM[oamBase + 3]
            };
            
            spritesFound++;
            if (spritesFound >= 10) break;
        }
    }

    std::sort(sprites, sprites + spritesFound, [&](const OAMSprite &a, const OAMSprite &b) {
        if ((OPRI & 0x01 && isCGB) || !isCGB) {
            if (a.x != b.x) {
                return a.x > b.x;
            }
        }
    
        return a.index > b.index;
    });

    for (int i = 0; i < spritesFound; i++) {
        OAMSprite &spr = sprites[i];
        
        uint8_t tileNum = spr.tile;
        if (use8x16) {
            tileNum &= 0xFE; 
        }
        
        uint8_t cgbPalette  = spr.attributes & 0x07;
        uint8_t cgbTileBank = (spr.attributes & 0x08) >> 3;
        bool objToBgPriority = (spr.attributes & 0x80); 
        bool yFlip = (spr.attributes & 0x40); 
        bool xFlip = (spr.attributes & 0x20); 
        uint8_t paletteReg = (spr.attributes & 0x10) ? OBP1 : OBP0; 

        int lineInsideSprite = LY - spr.y;
        if (yFlip) {
            lineInsideSprite = spriteHeight - 1 - lineInsideSprite;
        }

        uint16_t tileDataLocation = 0x8000 + (tileNum * 16) + (lineInsideSprite * 2);
        uint16_t bankOffset = isCGB ? (cgbTileBank * 8192) : 0;

        uint8_t byte1 = VRAM[(tileDataLocation - 0x8000) + bankOffset];
        uint8_t byte2 = VRAM[(tileDataLocation + 1 - 0x8000) + bankOffset];
        
        for (int tilePixel = 0; tilePixel < 8; tilePixel++) {
            int pixelX = spr.x + tilePixel;
            
            if (pixelX < 0 || pixelX >= 160) continue;

            int bitBit = xFlip ? tilePixel : (7 - tilePixel);
            int colorBit0 = (byte1 >> bitBit) & 0x01;
            int colorBit1 = (byte2 >> bitBit) & 0x01;
            uint8_t colorId = (colorBit1 << 1) | colorBit0;

            if (colorId == 0) continue;

            if (masterPriority) {
                if (isCGB && bgPriorityLine[pixelX] && bgLineColorIds[pixelX] != 0) continue;
                if (objToBgPriority && bgLineColorIds[pixelX] != 0) continue; 
            }

            if (isCGB) {
                uint8_t palBase = (cgbPalette * 8) + (colorId * 2);
                uint16_t colorData = SPPaletteRAM[palBase] | (SPPaletteRAM[palBase + 1] << 8);
                uint32_t finalColor = 0xFF000000 | (((colorData & 0x001F) << 3) << 16) | (((colorData & 0x03E0) >> 2) << 8) | (((colorData & 0x7C00) >> 7));
                
                if (!DisableSprites) frameBuffer[LY * 160 + pixelX] = finalColor;
            } else {
                uint8_t colorPaletteShade = (paletteReg >> (colorId * 2)) & 0x03;
                if (!DisableSprites) frameBuffer[LY * 160 + pixelX] = gbPalette[colorPaletteShade];
            }
        }
    }
}

void GbPPU::blitPixels() {
    for (int y = 0; y < 144; y++) {
        for (int x = 0; x < 160; x++) {
            int i = y * 160 + x;
            vfilter->applyFilter(&frameBuffer[i], x, y);
        }
    }
}

uint8_t GbPPU::readVRAM(uint16_t addr) {
    uint16_t offset = (addr - 0x8000) + (VBK * 8192);
    return VRAM[offset];
}
void GbPPU::writeVRAM(uint16_t addr, uint8_t value) {
    uint16_t offset = (addr - 0x8000) + (VBK * 8192); 

    if (VRAMCorruption && (rand() & 7) == 0) {
        offset ^= (1 << (rand() % 13)); 
    }

    offset &= 0x3FFF;
    VRAM[offset] = value;
}
uint8_t GbPPU::readOAM(uint16_t addr) {
    return OAM[addr - 0xFE00];
}
void GbPPU::writeOAM(uint16_t addr, uint8_t value) {
    OAM[addr - 0xFE00] = value;
}

uint8_t GbPPU::readRegister(uint16_t addr) {
    switch (addr) {
        case 0xFF40: return LCDC;
        case 0xFF41: return STAT | 0x80;
        case 0xFF42: return SCY;
        case 0xFF43: return SCX;
        case 0xFF44: return LY;
        case 0xFF45: return LYC;
        case 0xFF46: return DMA;
        case 0xFF47: return BGP;
        case 0xFF48: return OBP0;
        case 0xFF49: return OBP1;
        case 0xFF4A: return WY;
        case 0xFF4B: return WX;
        case 0xFF4F: return VBK | 0xFE; 
        case 0xFF68: return BCPS | 0x40;
        case 0xFF69: return BGPaletteRAM[BCPS & 0x3F];
        case 0xFF6A: return OCPS | 0x40;
        case 0xFF6B: return SPPaletteRAM[OCPS & 0x3F];
        case 0xFF6C: return OPRI | 0xFE;
        default: return 0xFF;
    }
}

void GbPPU::writeRegister(uint16_t addr, uint8_t value) {
    switch (addr) {
        case 0xFF40: {
            LCDC = value; 
            if (!(value & 0x80)) {
                LY = 0;
                windowLine = 0;
                scanlineCounter = 456;
                STAT = (STAT & ~0x03);
                windowYLatch = false;
            }
            break;
        }
        case 0xFF41:
            STAT = (STAT & 0x07) | (value & 0x78);
            break;
        case 0xFF42: SCY = value; break;
        case 0xFF43: SCX = value; break;
        case 0xFF44:
            LY = 0;
            if (LY == LYC) {
                STAT |= 0x04;
            } else {
                STAT &= ~0x04;
            }
            break;
        case 0xFF45:
            LYC = value;
            if (LY == LYC) {
                STAT |= 0x04;
                if (STAT & 0x40) cpu->IF |= 0x02;
            } else {
                STAT &= ~0x04;
            }
            break;
        case 0xFF46: {
            DMA = value;

            uint16_t src = value << 8;
            for (int i = 0; i < 160; i++) {
                OAM[i] = cpu->read(src + i);
            }
            break;
        }
        case 0xFF47: BGP = value; break;
        case 0xFF48: OBP0 = value; break;
        case 0xFF49: OBP1 = value; break;
        case 0xFF4A: WY = value; break;
        case 0xFF4B: WX = value; break;
        case 0xFF4F: VBK = value & 1; break;
        case 0xFF68: BCPS = value; break;
        case 0xFF69: {
            uint8_t index = BCPS & 0x3F;
            BGPaletteRAM[index] = value;
            if (BCPS & 0x80) {
                BCPS = (BCPS & 0x80) | ((index + 1) & 0x3F);
            }
            break;
        }
        case 0xFF6A: OCPS = value; break;
        case 0xFF6B: {
            uint8_t index = OCPS & 0x3F;
            SPPaletteRAM[index] = value;
            if (OCPS & 0x80) {
                OCPS = (OCPS & 0x80) | ((index + 1) & 0x3F);
            }
            break;
        }
        case 0xFF6C: OPRI = value & 1; break;
    }
}