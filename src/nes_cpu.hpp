#pragma once

#include <cstdint>
#include <iostream>
#include <array>
#include "nes_ppu.hpp"
#include "main.hpp"

#include <stdio.h>

class CPU {
public:
    CPU() { reset(); }

    bool CPUPaused = false;

    void reset() {
        A = X = Y = 0;
        SP = 0xFD;
        P = 0x24;
        PC = read16(0xFFFC);
        cycles = 0;
    }

    void LoadMem(const std::array<uint8_t, MEMORY_SIZE>& mem) {
        memory = mem;
    }

    bool NMIDetector = false;

    void HandleNMI() {
        write(0x100 + SP--, (PC >> 8) & 0xFF);
        write(0x100 + SP--, PC & 0xFF);
        write(0x100 + SP--, (P & ~0x10) | 0x20);
        uint8_t lo = read(0xFFFA);
        uint8_t hi = read(0xFFFB);
        PC = (hi << 8) | lo;
        cycles += 7;
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
    uint8_t A, X, Y;
    uint16_t PC;
    uint8_t SP;
    uint8_t P;
    uint64_t cycles;

    std::array<uint8_t, MEMORY_SIZE> memory{};

    uint8_t fetch() { return read(PC++); }

    uint16_t fetch16() { return fetch() | (fetch() << 8); }
};

extern CPU cpu;