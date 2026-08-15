#pragma once
#include "mapper_base.hpp"
#include <stdint.h>
#include "../nes_ppu.hpp"

class N163 : public MapperBase {
public:
    N163();

    uint8_t cpuRead(uint16_t addr) override;
    void cpuWrite(uint16_t addr, uint8_t value) override;
    void reset() override;
    const char *getName(void) override;

    uint16_t getCHRBankSize() override { return 0x400; }
    uint16_t getPRGBankSize() override { return 0x2000; }

    void clockCPU(void) override;

    bool hasExpansionAudio() override { return true; }
    double getExpansionAudioSample() override;
private:
    enum Variant {
        NAMCO_163,
        NAMCO_175,
        NAMCO_340,
        NAMCO_UNKNOWN
    };

    Variant variant;
    uint8_t writeProtect;
    uint16_t irqCounter;

    uint8_t n163Ram[128];
    uint8_t n163Addr;
    bool n163AutoInc;
    uint8_t audioCycleCount;
    uint8_t currentChannel;
    double currentAudioSample;
    double channelOut[8];

    void updateWorkRam();
};
