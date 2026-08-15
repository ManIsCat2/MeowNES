#pragma once
#include "mapper_base.hpp"
#include <stdint.h>
#include "../nes_ppu.hpp"

class JyCompany : public MapperBase {
public:
    JyCompany();

    uint8_t cpuRead(uint16_t addr) override;
    void cpuWrite(uint16_t addr, uint8_t value) override;
    const char *getName(void) override;
    void reset() override;

    void saveState(SaveStateFile &s) override {
        for (int i = 0; i < 4; i++) s.WriteBytes<uint8_t>(prgRegs[i]);
        s.WriteBytes<uint8_t>(prgMode);
        s.WriteBytes<bool>(enablePrgAt6000);

        for (int i = 0; i < 8; i++) s.WriteBytes<uint8_t>(chrLowRegs[i]);
        for (int i = 0; i < 8; i++) s.WriteBytes<uint8_t>(chrHighRegs[i]);
        for (int i = 0; i < 2; i++) s.WriteBytes<uint8_t>(chrLatch[i]);

        s.WriteBytes<uint8_t>(chrMode);
        s.WriteBytes<bool>(advancedNtControl);
        s.WriteBytes<bool>(chrBlockMode);
        s.WriteBytes<uint8_t>(chrBlock);
        s.WriteBytes<bool>(mirrorChr);

        s.WriteBytes<uint8_t>(mirroringReg);

        s.WriteBytes<bool>(irqEnabled);
        s.WriteBytes<uint8_t>(irqSource);
        s.WriteBytes<uint8_t>(irqCounter);
        s.WriteBytes<uint8_t>(irqPrescaler);
        s.WriteBytes<uint8_t>(irqXorReg);
        s.WriteBytes<uint8_t>(irqFunkyModeReg);
        s.WriteBytes<uint8_t>(irqCountDirection);
        s.WriteBytes<bool>(irqFunkyMode);
        s.WriteBytes<bool>(irqSmallPrescaler);

        s.WriteBytes<uint8_t>(multiplyValue1);
        s.WriteBytes<uint8_t>(multiplyValue2);
        s.WriteBytes<uint8_t>(regRamValue);
    }
    void loadState(SaveStateFile &s) override {
        for (int i = 0; i < 4; i++) prgRegs[i] = s.ReadBytes<uint8_t>();
        prgMode = s.ReadBytes<uint8_t>();
        enablePrgAt6000 = s.ReadBytes<bool>();

        for (int i = 0; i < 8; i++) chrLowRegs[i] = s.ReadBytes<uint8_t>();
        for (int i = 0; i < 8; i++) chrHighRegs[i] = s.ReadBytes<uint8_t>();
        for (int i = 0; i < 2; i++) chrLatch[i] = s.ReadBytes<uint8_t>();

        chrMode = s.ReadBytes<uint8_t>();
        advancedNtControl = s.ReadBytes<bool>();
        chrBlockMode = s.ReadBytes<bool>();
        chrBlock = s.ReadBytes<uint8_t>();
        mirrorChr = s.ReadBytes<bool>();

        mirroringReg = s.ReadBytes<uint8_t>();

        irqEnabled = s.ReadBytes<bool>();
        irqSource = s.ReadBytes<uint8_t>();
        irqCounter = s.ReadBytes<uint8_t>();
        irqPrescaler = s.ReadBytes<uint8_t>();
        irqXorReg = s.ReadBytes<uint8_t>();
        irqFunkyModeReg = s.ReadBytes<uint8_t>();
        irqCountDirection = s.ReadBytes<uint8_t>();
        irqFunkyMode = s.ReadBytes<bool>();
        irqSmallPrescaler = s.ReadBytes<bool>();

        multiplyValue1 = s.ReadBytes<uint8_t>();
        multiplyValue2 = s.ReadBytes<uint8_t>();
        regRamValue = s.ReadBytes<uint8_t>();

        updateState();
    }

    uint16_t getCHRBankSize() override {
        return 0x400;
    }
    uint16_t getPRGBankSize() override {
        return 0x2000;
    }

    void clockCPU(void) override;
    void clockPPU(void) override;
private:
    uint8_t prgRegs[4];
    uint8_t prgMode;
    bool enablePrgAt6000;

    uint8_t chrLowRegs[8];
    uint8_t chrHighRegs[8];
    uint8_t chrLatch[2];
    uint8_t chrMode;
    bool advancedNtControl;
    bool chrBlockMode;
    uint8_t chrBlock;
    bool mirrorChr;

    uint8_t mirroringReg;

    bool irqEnabled;
    uint8_t irqSource; 
    uint8_t irqCounter;
    uint8_t irqPrescaler;
    uint8_t irqXorReg;
    uint8_t irqFunkyModeReg;
    uint8_t irqCountDirection;
	bool irqFunkyMode = false;
    bool irqSmallPrescaler = false;

    uint8_t multiplyValue1;
    uint8_t multiplyValue2;
    uint8_t regRamValue;

    void clockIRQ(void);
    void updatePrg();
    void updateCHR();
    void updateMirroring();
    void updateState();
    uint16_t getChrReg(int index);
};