#pragma once

#include <cstdint>
#include <QImage>
#include "../filters/filters.hpp"
#include "gb_bus.hpp"

class GbPPU : public HasGBBus, public HasVideoFilter {
public:
    GbPPU();
    ~GbPPU();

    uint8_t VRAM[16384];
    uint8_t OAM[160];

    uint8_t LCDC = 0x00;
    uint8_t STAT = 0x00;
    uint8_t SCY  = 0x00;
    uint8_t SCX  = 0x00;
    uint8_t LY   = 0x00;
    uint8_t LYC  = 0x00;
    uint8_t DMA  = 0x00;
    uint8_t BGP  = 0xFC;
    uint8_t OBP0 = 0xFF;
    uint8_t OBP1 = 0xFF;
    uint8_t WY   = 0x00;
    uint8_t WX   = 0x00;

    uint8_t VBK = 0;
    uint8_t BCPS = 0, BCPD = 0;
    uint8_t OCPS = 0, OCPD = 0;
    uint8_t OPRI = 0;
    uint8_t BGPaletteRAM[64];
    uint8_t SPPaletteRAM[64];

    uint8_t windowLine = 0;
    bool windowYLatch = false;
    int scanlineCounter = 0;
    uint32_t frameBuffer[NES_NTSC_OUT_WIDTH(160) * 144];
    uint32_t backBuffer[160 * 144];
    uint8_t palIndexBuf[160 * 144];
    QImage *rawOutputImage = nullptr;
    QImage *filteredOutputImage = nullptr;

    bool VRAMCorruption = false;
    bool DisableSprites = false;

    void reset();
    void Step(uint8_t cycles);
    void RenderScanline();
    void blitPixels();
    
    uint8_t readVRAM(uint16_t addr);
    void writeVRAM(uint16_t addr, uint8_t value);
    uint8_t readOAM(uint16_t addr);
    void writeOAM(uint16_t addr, uint8_t value);
    uint8_t readRegister(uint16_t addr);
    void writeRegister(uint16_t addr, uint8_t value);
};

extern GbPPU gbPpu;
extern uint32_t gbPaletteDefault[4];
extern uint32_t gbPalette[4];
