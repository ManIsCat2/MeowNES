#pragma once

#include <cstdint>
#include <iostream>
#include <array>
#include <stdio.h>
#include "gb_console.hpp"
#include "gb_bus.hpp"
#include "../main.hpp"

class GbCPU : public HasGBBus {
public:
    GbCPU() { }

    enum Flags {
        FlagC = (1 << 4),
        FlagH = (1 << 5),
        FlagN = (1 << 6),
        FlagZ = (1 << 7),
    };

    bool useBootROM = false;
    bool paused = false;

    uint8_t DIV = 0;
    uint16_t DIVInternal = 0;
    uint8_t TIMA = 0;
    uint8_t TMA = 0;
    uint8_t TAC = 0;
    bool IME = false;
    uint8_t EIPending = 0;
    bool HALTBug = false;

    uint8_t KEY1 = 0;
    bool doubleSpeed = false;
    uint8_t SVBK = 0;
    bool HDMAActive = false;
    uint16_t HDMALength = 0;
    uint16_t HDMASrc = 0;
    uint16_t HDMADst = 0;

    std::string serialBuffer = "";
    uint8_t serialData = 0;
    uint8_t serialControl = 0;

    void reset();

    void GetInfo(uint8_t *AReg, uint8_t *FReg, uint8_t *BReg, uint8_t *CReg, int8_t *DReg, uint8_t *EReg, uint8_t *HReg, uint8_t *LReg,  uint16_t *PCReg, uint16_t *SPReg) {
        *AReg = A; *FReg = F;
        *BReg = B; *CReg = C;
        *DReg = D; *EReg = E;
        *HReg = H; *LReg = L;
        *PCReg = PC;
        *SPReg = SP;
    }

    void SetInfo(uint8_t AReg, uint8_t FReg, uint8_t BReg, uint8_t CReg, uint8_t DReg, uint8_t EReg, uint8_t HReg, uint8_t LReg, uint16_t PCReg, uint16_t SPReg) {
        A = AReg; F = FReg & 0xF0;
        B = BReg; C = CReg;
        D = DReg; E = EReg;
        H = HReg; L = LReg;
        PC = PCReg;
        SP = SPReg;
    }

    void run(uint32_t maxCycles);
    void execute(uint8_t opcode);
    void executeCB(uint8_t opcode);
    void handleInterrupts();
    void updateTimers(uint8_t cycles);

    uint8_t read(uint16_t addr);
    void write(uint16_t addr, uint8_t value);
    
    void push(uint8_t value);
    uint8_t pop();


    uint8_t IF = 0xE1;
    uint8_t IE = 0x00;
    uint8_t joypadReg = 0x30;
private:
    uint64_t cycles = 0;
    bool halted = false;

    uint8_t A = 0, F = 0;
    uint8_t B = 0, C = 0;
    uint8_t D = 0, E = 0;
    uint8_t H = 0, L = 0;

    uint16_t PC = 0;
    uint16_t SP = 0;

    uint16_t getAF() { return (A << 8) | F; }
    uint16_t getBC() { return (B << 8) | C; }
    uint16_t getDE() { return (D << 8) | E; }
    uint16_t getHL() { return (H << 8) | L; }

    void setAF(uint16_t val) { A = (val >> 8); F = val & 0xF0; }
    void setBC(uint16_t val) { B = (val >> 8); C = val & 0xFF; }
    void setDE(uint16_t val) { D = (val >> 8); E = val & 0xFF; }
    void setHL(uint16_t val) { H = (val >> 8); L = val & 0xFF; }

    uint8_t opINC(uint8_t value) {
        uint8_t result = value + 1;
        
        F &= FlagC; 
        if (result == 0) F |= FlagZ;
        if ((value & 0x0F) == 0x0F) F |= FlagH;
        
        return result;
    }
    uint8_t opSUB(uint8_t val) {
        F = FlagN;
        if ((A & 0x0F) < (val & 0x0F)) {
            F |= FlagH;
        }
        if (A < val) {
            F |= FlagC;
        }
        uint8_t result = A - val;
        if (result == 0) {
            F |= FlagZ;
        }
        return result;
    }
    uint8_t opSBC(uint8_t val) {
        uint8_t carry = (F & FlagC) ? 1 : 0;
        int result = A - val - carry;
        
        F = FlagN;
        if (((A & 0x0F) - (val & 0x0F) - carry) < 0) {
            F |= FlagH;
        }
        if (result < 0) {
            F |= FlagC;
        }
        
        uint8_t res8 = (uint8_t)result;
        if (res8 == 0) {
            F |= FlagZ;
        }
        return res8;
    }

    uint8_t opADD(uint8_t val) {
        uint16_t result = A + val;
        F = 0;
        if ((uint8_t)result == 0) {
            F |= FlagZ;
        }
        if (((A & 0x0F) + (val & 0x0F)) > 0x0F) {
            F |= FlagH;
        }
        if (result > 0xFF) {
            F |= FlagC;
        }
        return (uint8_t)result;
    }
    uint16_t opADD16(uint16_t val) {
        uint32_t hl = getHL();
        uint32_t result = hl + val;
        
        F &= ~(FlagN | FlagH | FlagC);
        if (((hl & 0x0FFF) + (val & 0x0FFF)) > 0x0FFF) {
            F |= FlagH;
        }
        if (result > 0xFFFF) {
            F |= FlagC;
        }
        
        return (uint16_t)result;
    }

    uint8_t opADC(uint8_t val) {
        uint8_t carry = (F & FlagC) ? 1 : 0;
        int result = A + val + carry;
        F = 0;
        if (((A & 0x0F) + (val & 0x0F) + carry) > 0x0F) {
            F |= FlagH;
        }
        if (result > 0xFF) {
            F |= FlagC;
        }
        uint8_t res8 = (uint8_t)result;
        if (res8 == 0) {
            F |= FlagZ;
        }
        return res8;
    }

    uint8_t opOR(uint8_t val) {
        A |= val;
        F = (A == 0) ? FlagZ : 0;
        return A;
    }

    uint8_t opXOR(uint8_t value) {
        uint8_t result = A ^ value;

        F = 0;
        if (result == 0) {
            F |= FlagZ;
        }

        return result;
    }

    uint8_t opAND(uint8_t value) {
        uint8_t result = A & value;

        F = FlagH;
        if (result == 0) {
            F |= FlagZ;
        }

        return result;
    }

    uint8_t cbRLC(uint8_t v) {
        uint8_t carry = (v >> 7) & 1;
        uint8_t result = (v << 1) | carry;
        F = 0;
        if (result == 0) {
            F |= FlagZ;
        }
        if (carry != 0) {
            F |= FlagC;
        }
        return result;
    }

    uint8_t cbRRC(uint8_t v) {
        uint8_t carry = v & 1;
        uint8_t result = (v >> 1) | (carry << 7);
        F = 0;
        if (result == 0) {
            F |= FlagZ;
        }
        if (carry != 0) {
            F |= FlagC;
        }
        return result;
    }

    uint8_t cbRL(uint8_t v) {
        uint8_t oldCarry = 0;
        if ((F & FlagC) != 0) {
            oldCarry = 1;
        }
        uint8_t newCarry = (v >> 7) & 1;
        uint8_t result = (v << 1) | oldCarry;
        F = 0;
        if (result == 0) {
            F |= FlagZ;
        }
        if (newCarry != 0) {
            F |= FlagC;
        }
        return result;
    }

    uint8_t cbRR(uint8_t v) {
        uint8_t oldCarry = 0;
        if ((F & FlagC) != 0) {
            oldCarry = 1;
        }
        uint8_t newCarry = v & 1;
        uint8_t result = (v >> 1) | (oldCarry << 7);
        F = 0;
        if (result == 0) {
            F |= FlagZ;
        }
        if (newCarry != 0) {
            F |= FlagC;
        }
        return result;
    }

    uint8_t cbSLA(uint8_t v) {
        uint8_t carry = (v >> 7) & 1;
        uint8_t result = v << 1;
        F = 0;
        if (result == 0) {
            F |= FlagZ;
        }
        if (carry != 0) {
            F |= FlagC;
        }
        return result;
    }

    uint8_t cbSRA(uint8_t v) {
        uint8_t carry = v & 1;
        uint8_t sign = v & 0x80;
        uint8_t result = (v >> 1) | sign;
        F = 0;
        if (result == 0) {
            F |= FlagZ;
        }
        if (carry != 0) {
            F |= FlagC;
        }

        return result;
    }

    uint8_t cbSRL(uint8_t v) {
        uint8_t carry = v & 1;
        uint8_t result = v >> 1;
        F = 0;
        if (result == 0) {
            F |= FlagZ;
        }
        if (carry != 0) {
            F |= FlagC;
        }
        return result;
    }

    uint8_t cbSWAP(uint8_t v) {
        uint8_t low = v & 0x0F;
        uint8_t high = v & 0xF0;
        uint8_t result = (low << 4) | (high >> 4);
        F = 0;
        if (result == 0) {
            F |= FlagZ;
        }
        return result;
    }

    void cbBIT(int bit, uint8_t v) {
        F &= ~FlagN;
        F |= FlagH;
        if ((v & (1 << bit)) == 0) {
            F |= FlagZ;
        } else {
            F &= ~FlagZ;
        }
    }

    uint8_t fetch() { 
        uint8_t opcode = read(PC);
        if (HALTBug) {
            HALTBug = false;
        } else {
            PC++;
        }
        return opcode;
    }
    uint16_t fetch16() { return fetch() | (fetch() << 8); }
};

extern GbCPU gbCpu;