#pragma once
#include "mapper_base.hpp"
#include <stdint.h>
#include "../nes_ppu.hpp"

class MMC3 : public MapperBase {
public:
    MMC3();

    void cpuWrite(uint16_t addr, uint8_t value) override;
    const char *getName(void) override;
    void reset() override;
    
    void saveState(SaveStateFile &s) override {
        s.WriteBytes<uint8_t>(BankSelect);
        s.WriteLenBytes<uint8_t>(BankRegisters, 8);

        s.WriteBytes<uint8_t>(PrgMode);
        s.WriteBytes<uint8_t>(ChrMode);

        s.WriteBytes<uint8_t>(IRQReload);
        s.WriteBytes<uint8_t>(IRQCounter);
        s.WriteBytes<bool>(IRQEnabled);
        s.WriteBytes<bool>(LastA12);
    }
    void loadState(SaveStateFile &s) override {
        BankSelect = s.ReadBytes<uint8_t>();
        s.ReadLenBytes<uint8_t>(BankRegisters, 8);

        PrgMode = s.ReadBytes<uint8_t>();
        ChrMode = s.ReadBytes<uint8_t>();

        IRQReload = s.ReadBytes<uint8_t>();
        IRQCounter = s.ReadBytes<uint8_t>();
        IRQEnabled = s.ReadBytes<bool>();
        LastA12 = s.ReadBytes<bool>();

        updatePRG();
        updateCHR();
    }

    uint16_t getCHRBankSize() override {
        return 0x400;
    }
    uint16_t getPRGBankSize() override {
        return 0x2000;
    }
    uint32_t getSRAMSize() override {
        return subMapper == 1 ? 0x400 : 0x2000;
    }

    void clockPPU(void) override;
private:
    uint8_t BankSelect;
    uint8_t BankRegisters[8];

    uint8_t PrgMode;
    uint8_t ChrMode;

    uint8_t IRQReload;
    uint8_t IRQCounter;
    bool IRQEnabled;
    bool LastA12;

    void updatePRG();
    void updateCHR();
};