#pragma once

#include <cstdint>
#include <iostream>
#include <array>
#include "mappers/mapper_base.hpp"
#include "../rom.hpp"
#include "../main.hpp"

#include <stdio.h>
#include "nes_bus.hpp"

class NesCPU : public HasNESBus {
public:
    NesCPU() { }

    //make code more readable
    enum Flags {
        C = (1 << 0),
        Z = (1 << 1),
        I = (1 << 2),
        D = (1 << 3),
        B = (1 << 4),
        U = (1 << 5),
        V = (1 << 6),
        N = (1 << 7),
    };

    bool paused = false;
    uint8_t dataBus = 0;
    bool NMIDetector = false;
    bool doNMI = false;
    bool doIRQ = false;
    bool IRQPending = false;

    MapperBase* romMapper = nullptr;

    void reset();

    void GetInfo(uint8_t *AReg, uint8_t *XReg, uint8_t *YReg, uint16_t *PCReg, uint8_t *SPReg, uint8_t *PReg) {
        *AReg = A;
        *XReg = X;
        *YReg = Y;
        *PCReg = PC;
        *SPReg = SP;
        *PReg = P;
    }

    void SetInfo(uint8_t AReg, uint8_t XReg, uint8_t YReg, uint16_t PCReg, uint8_t SPReg, uint8_t PReg) {
        A = AReg;
        X = XReg;
        Y = YReg;
        PC = PCReg;
        SP = SPReg;
        P = PReg;
    }
    
    void setExternalIRQ(bool irq) {
        IRQPending = irq;
    }

    void run(uint32_t maxCycles);

    void execute(uint8_t opcode);
    void SetZN(uint8_t value);
    uint8_t read(uint16_t addr);
    void write(uint16_t addr, uint8_t value);
    uint16_t read16(uint16_t addr);
    void push(uint8_t value);
    uint8_t pop();

private:
    uint64_t cycles = 0;
    uint8_t A, X, Y = 0;
    uint16_t PC = 0;
    uint8_t SP = 0;
    uint8_t P = 0;

    uint8_t fetch() { return dataBus = read(PC++); }
    uint16_t fetch16() { return fetch() | (fetch() << 8); }
};

extern NesCPU nesCpu;