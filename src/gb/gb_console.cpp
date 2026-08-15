#include "gb_console.hpp"
#include "../nes/nes_controller.hpp"
#include "gb_apu.hpp"
#include "gb_cpu.hpp"
#include "gb_ppu.hpp"
#include "gb_rom.hpp"

#define A_BUTTON       (1 << 0)
#define B_BUTTON       (1 << 1)
#define SELECT_BUTTON  (1 << 2)
#define START_BUTTON   (1 << 3)
#define DPAD_UP        (1 << 4)
#define DPAD_DOWN      (1 << 5)
#define DPAD_LEFT      (1 << 6)
#define DPAD_RIGHT     (1 << 7)

#define CYCLES_PER_FRAME (4194304.0 / 60.0)

bool GBConsole::loadGame(const std::string &file) {
    if (!rom) {
        rom = new GbROM;
    }
    return rom->load(file);
}

void GBConsole::runFrame(void) {
    uint32_t targetCycles = CYCLES_PER_FRAME;
    
    if (gbCpu.doubleSpeed) {
        targetCycles *= 2;
    }
    
    gbCpu.run(targetCycles);
}

int GBConsole::getDisplayWidth(void) {
    return 160;
}
int GBConsole::getDisplayHeight(void) {
    return 144;
}

void GBConsole::handleController(int id, int key, bool pressed) {
    Controller &c = controllers[id];
    uint8_t oldState = controllers[id].state;

    if (key == nesKeyBinds[0].key) pressed ? c.state|=A_BUTTON : c.state&=~A_BUTTON;
    if (key == nesKeyBinds[1].key) pressed ? c.state|=B_BUTTON : c.state&=~B_BUTTON;
    if (key == nesKeyBinds[2].key) pressed ? c.state|=DPAD_UP : c.state&=~DPAD_UP;
    if (key == nesKeyBinds[3].key) pressed ? c.state|=DPAD_DOWN : c.state&=~DPAD_DOWN;
    if (key == nesKeyBinds[4].key) pressed ? c.state|=DPAD_LEFT : c.state&=~DPAD_LEFT;
    if (key == nesKeyBinds[5].key) pressed ? c.state|=DPAD_RIGHT : c.state&=~DPAD_RIGHT;
    if (key == nesKeyBinds[6].key) pressed ? c.state|=START_BUTTON : c.state&=~START_BUTTON;
    if (key == nesKeyBinds[7].key) pressed ? c.state|=SELECT_BUTTON : c.state&=~SELECT_BUTTON;

    if (pressed && oldState != controllers[id].state) {
        gbCpu.IF |= 0x10;
    }
}

double GBConsole::getAudioOutput(void) {
    return gbApu.getOutputSample();
}

QImage *GBConsole::getOutputImage(void) {
    if (gbPpu.filtering == VideoFilter::NTSC) {
        gbPpu.vfilter->initialize();
        return gbPpu.filteredOutputImage;
    } else {
       return gbPpu.rawOutputImage;
    }
}
void GBConsole::setVideoFilter(int filter) {
    gbPpu.initFilter((VideoFilter)filter);
}

void GBConsole::loadSave(void) {
    getGBRom()->mbc->loadSRAM();
}
void GBConsole::writeSave(void) {
    getGBRom()->mbc->saveSRAM();
}

void GBConsole::init(void) {
    
}

void GBConsole::pause(void) {
    gbCpu.paused = !gbCpu.paused;
}

void GBConsole::reset(void) {
    gbCpu.reset();
}

GbROM *getGBRom(void) {
    return (GbROM*)getRom();
}