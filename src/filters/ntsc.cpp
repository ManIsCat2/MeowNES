#include "ntsc.hpp"
#include "../console.hpp"
#include "../gb/gb_ppu.hpp"

void NTSCFilter::initialize(void) {
    bool isGB = emuConsole ? (emuConsole->getConsoleType() == ConsoleType::GAMEBOY) : false;
    NTSCSetup._32bit_palette = isGB ? gbPalette : nesPalette;
    nes_ntsc_init(&NTSC, &NTSCSetup);
}

void NTSCFilter::blit(void) {
    int w = emuConsole ? emuConsole->getDisplayWidth() : 0;
    int h = emuConsole ? emuConsole->getDisplayHeight() : 0;
    bool isGB = emuConsole ? (emuConsole->getConsoleType() == ConsoleType::GAMEBOY) : false;
    nes_ntsc_blit(&NTSC, isGB ? gbPpu.palIndexBuf : nesPpu.palIndexBuf, w, 0, w, h, isGB ? gbPpu.frameBuffer : nesPpu.frameBuffer, NES_NTSC_OUT_WIDTH(w) * sizeof(uint32_t));
}