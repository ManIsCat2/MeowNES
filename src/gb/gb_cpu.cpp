#include "gb_cpu.hpp"
#include "gb_ppu.hpp"
#include "gb_apu.hpp"

GbCPU gbCpu;

void GbCPU::reset() {
    connectBus(nullptr, &gbPpu, &gbApu);
    useBootROM = true;
    paused = false;

    A = 0;
    F = 0;
    B = 0; C = 0;
    D = 0; E = 0;
    H = 0; L = 0;
    SP = 0;
    PC = 0;
    cycles = 0;
    IME = false;
    EIPending = 0;
    HALTBug = false;

    IE = 0; IF = 0xe1;

    DIV = 0xAB;
    DIVInternal = 0xAB00;
    TIMA = 0;
    TMA = 0;
    TAC = 0;

    KEY0 = 0;
    KEY1 = 0;
    doubleSpeed = false;
    SVBK = 1;
    HDMAActive = false;
    HDMALength = 0;

    serialData = 0;
    serialControl = 0;
    joypadReg = 0x30;

    halted = false;

    ppu->reset();
    apu->reset();
}

//#define GBCPU_DBG
#ifdef GBCPU_DBG
#define PRINT_DBG_CPU(...) printf(__VA_ARGS__)
#else
#define PRINT_DBG_CPU(...)
#endif

static const uint16_t interruptVectors[5] = {
    0x0040, // vblank
    0x0048, // lcd stat
    0x0050, // timer
    0x0058, // serial
    0x0060  // controller/joypad
};

void GbCPU::handleInterrupts() {
    uint8_t pending = IF & IE & 0x1F;

    if (halted && pending) {
        halted = false;
        if (!IME) {
            HALTBug = true;
            return;
        }
    }
    if (!IME) return;

    for (int i = 0; i < 5; i++) {
        if (pending & (1 << i)) {
            IME = false;
            IF &= ~(1 << i);

            push(PC >> 8);
            push(PC & 0xFF);

            PC = interruptVectors[i];

            cycles = 20;
            return;
        }
    }
}

void GbCPU::updateTimers(uint8_t cycles) {
    while (cycles--) {
        uint16_t oldDiv = DIVInternal;
        DIVInternal++;

        DIV = DIVInternal >> 8;

        if (!(TAC & 0x04)) continue;

        bool oldBit = false;
        bool newBit = false;

        switch (TAC & 3) {
            case 0:
                oldBit = oldDiv & (1 << 9);
                newBit = DIVInternal & (1 << 9);
                break;

            case 1:
                oldBit = oldDiv & (1 << 3);
                newBit = DIVInternal & (1 << 3);
                break;

            case 2:
                oldBit = oldDiv & (1 << 5);
                newBit = DIVInternal & (1 << 5);
                break;

            default:
                oldBit = oldDiv & (1 << 7);
                newBit = DIVInternal & (1 << 7);
                break;
        }

        if (oldBit && !newBit) {
            if (TIMA == 0xFF) {
                TIMA = TMA;
                IF |= 0x04;
            } else {
                TIMA++;
            }
        }
    }
}

void GbCPU::run(uint32_t maxCycles) {
    uint32_t cyclesRun = 0;

    while (cyclesRun < maxCycles) {
        if (!romIsLoaded || paused) return;

        cycles = 0; 
        handleInterrupts();

        if (cycles == 0) {
            if (halted) {
                cycles = 4;
            } else {
                uint8_t opcode = fetch();
                execute(opcode);

                if (EIPending > 0) {
                    EIPending--;
                    if (EIPending == 0) {
                        IME = true;
                    }
                }
            }
        }
        
        cyclesRun += cycles;
        updateTimers(cycles);
        ppu->Step(cycles);
        apu->step(cycles);
    }
}

void GbCPU::execute(uint8_t opcode) {
    switch (opcode) {
        case 0x00: // NOP
            PRINT_DBG_CPU("NOP\n");
            cycles = 4;
            break;

        case 0x01: { // LD BC, d16
            uint16_t imm16 = fetch16();
            PRINT_DBG_CPU("LD BC, $0x%04X\n", imm16);
            setBC(imm16);
            cycles = 12;
            break;
        }

        case 0x02: // LD (BC), A
            PRINT_DBG_CPU("LD (BC), A\n");
            write(getBC(), A);
            cycles = 8;
            break;

        case 0x03: // INC BC
            PRINT_DBG_CPU("INC BC\n");
            setBC(getBC() + 1);
            cycles = 8;
            break;

        case 0x04: // INC B
            PRINT_DBG_CPU("INC B\n");
            B = opINC(B);
            
            cycles = 4;
            break;

        case 0x05:
            PRINT_DBG_CPU("DEC B\n");
            F = FlagN | (F & FlagC) | (((B & 0x0F) == 0x00) ? FlagH : 0);
            B--;
            if (B == 0) {
                F |= FlagZ;
            }
            cycles = 4;
            break;

        case 0x06: // LD B, d8
            B = fetch();
            PRINT_DBG_CPU("LD B, $0x%02X\n", B);
            cycles = 8;
            break;

        case 0x07: {
           uint8_t carry = (A >> 7) & 1;
            A = (A << 1) | carry;
            F = carry ? FlagC : 0;
            cycles = 4;
            break;
        }

        case 0x08: {
            uint16_t address = fetch16();
            PRINT_DBG_CPU("LD ($0x%04X), SP\n", address);

            write(address, (uint8_t)(SP & 0xFF));
            write(address + 1, (uint8_t)(SP >> 8));

            cycles = 20;
            break;
        }

        case 0x09: {
            PRINT_DBG_CPU("ADD HL, BC\n");
            uint16_t res16 = opADD16((B << 8) | C);
            H = (res16 >> 8) & 0xFF;
            L = res16 & 0xFF;
            cycles = 8;
            break;
        }

        case 0x0C: // INC C
            PRINT_DBG_CPU("INC C\n");
            C = opINC(C);
            
            cycles = 4;
            break;

        case 0x0A:
            PRINT_DBG_CPU("LD A, (BC)\n");
            A = read(getBC());
            cycles = 8;
            break;

        case 0x0B: // DEC BC
            PRINT_DBG_CPU("DEC BC\n");
            setBC(getBC() - 1);
            cycles = 8;
            break;

        case 0x0D:
            PRINT_DBG_CPU("DEC C\n");
            F = FlagN | (F & FlagC) | (((C & 0x0F) == 0x00) ? FlagH : 0);
            C--;
            if (C == 0) {
                F |= FlagZ;
            }
            cycles = 4;
            break;

        case 0x0E: // LD C, d8
            C = fetch();
            PRINT_DBG_CPU("LD C, $0x%02X\n", C);
            cycles = 8;
            break;

        case 0x0F: {
            PRINT_DBG_CPU("RRCA\n");
            uint8_t bit0 = A & 0x01;
            A = (A >> 1) | (bit0 << 7);

            F = 0;
            if (bit0) {
                F |= FlagC;
            }

            cycles = 4;
            break;
        }

        case 0x10: // STOP
            PRINT_DBG_CPU("STOP\n");
            fetch();
            //halted = true; 
            cycles = 4;
            break;

        case 0x11: { // LD DE, d16
            uint16_t imm16 = fetch16();
            PRINT_DBG_CPU("LD DE, $0x%04X\n", imm16);
            setDE(imm16);
            cycles = 12;
            break;
        }

        case 0x12: // LD (DE), A
            PRINT_DBG_CPU("LD (DE), A\n");
            write(getDE(), A);
            cycles = 8;
            break;

        case 0x13: // INC DE
            PRINT_DBG_CPU("INC DE\n");
            setDE(getDE() + 1);
            cycles = 8;
            break;

        case 0x14:
            PRINT_DBG_CPU("INC D\n");
            D = opINC(D);

            cycles = 4;
            break;

        case 0x15:
            PRINT_DBG_CPU("DEC D\n");
            F = FlagN | (F & FlagC) | (((D & 0x0F) == 0x00) ? FlagH : 0);
            D--;
            if (D == 0) {
                F |= FlagZ;
            }
            cycles = 4;
            break;

        case 0x16: // LD D, d8
            D = fetch();
            PRINT_DBG_CPU("LD D, $0x%02X\n", D);
            cycles = 8;
            break;

        case 0x17: {
            PRINT_DBG_CPU("RLA\n");
            uint8_t oldCarry = (F & FlagC) ? 1 : 0;
            uint8_t newCarry = (A & 0x80) ? 1 : 0;
            A = (A << 1) | oldCarry;

            F = 0;
            if (newCarry) {
                F |= FlagC;
            }
            
            cycles = 4;
            break;
        }

        case 0x18: { // JR r8
            int8_t offset = (int8_t)fetch();
            PRINT_DBG_CPU("JR %d\n", offset);
            PC += offset;
            cycles = 12;
            break;
        }

        case 0x19: {
            PRINT_DBG_CPU("ADD HL, DE\n");
            uint16_t res16 = opADD16((D << 8) | E);
            H = (res16 >> 8) & 0xFF;
            L = res16 & 0xFF;
            cycles = 8;
            break;
        }

        case 0x1A: // LD A, (DE)
            PRINT_DBG_CPU("LD A, (DE)\n");
            A = read(getDE());
            cycles = 8;
            break;

        case 0x1B: // DEC DE
            PRINT_DBG_CPU("DEC DE\n");
            setDE(getDE() - 1);
            cycles = 8;
            break;

        case 0x1C: // INC E
            PRINT_DBG_CPU("INC E\n");
            E = opINC(E);

            cycles = 4;
            break;

        case 0x1D:
            PRINT_DBG_CPU("DEC E\n");
            F = FlagN | (F & FlagC) | (((E & 0x0F) == 0x00) ? FlagH : 0);
            E--;
            if (E == 0) {
                F |= FlagZ;
            }
            cycles = 4;
            break;

        case 0x1E: // LD E, d8
            E = fetch();
            PRINT_DBG_CPU("LD E, $0x%02X\n", E);
            cycles = 8;
            break;

        case 0x1F: {
            PRINT_DBG_CPU("RRA\n");
            uint8_t old_carry = (F & FlagC) ? 1 : 0;
            uint8_t next_carry = A & 0x01;
            
            A = (A >> 1) | (old_carry << 7);
            
            F = 0;
            if (next_carry) {
                F |= FlagC;
            }
            
            cycles = 4;
            break;
        }

        case 0x20: { // JR NZ, e8
            int8_t offset = (int8_t)fetch();
            if (!(F & FlagZ)) {
                PC += offset;
                cycles = 12;
            } else {
                cycles = 8;
            }
            break;
        }

        case 0x21: { // LD HL, d16
            uint16_t imm16 = fetch16();
            PRINT_DBG_CPU("LD HL, $0x%04X\n", imm16);
            setHL(imm16);
            cycles = 12;
            break;
        }

        case 0x22: // LD (HL+), A
            PRINT_DBG_CPU("LD (HL+), A\n");
            write(getHL(), A);
            setHL(getHL() + 1);
            cycles = 8;
            break;

        case 0x23: // INC HL
            PRINT_DBG_CPU("INC HL\n");
            setHL(getHL() + 1);
            cycles = 8;
            break;

        case 0x24:
            PRINT_DBG_CPU("INC H\n");
            H = opINC(H);

            cycles = 4;
            break;

        case 0x25:
            PRINT_DBG_CPU("DEC H\n");
            F = FlagN | (F & FlagC) | (((H & 0x0F) == 0x00) ? FlagH : 0);
            H--;
            if (H == 0) {
                F |= FlagZ;
            }
            cycles = 4;
            break;

        case 0x26: // LD H, d8
            H = fetch();
            PRINT_DBG_CPU("LD H, $0x%02X\n", H);
            cycles = 8;
            break;

        case 0x27: {
            PRINT_DBG_CPU("DAA\n");
            uint8_t correction = 0;
            bool set_carry = false;

            if (!(F & FlagN)) {
                if ((F & FlagH) || ((A & 0x0F) > 0x09)) {
                    correction |= 0x06;
                }
                if ((F & FlagC) || (A > 0x99)) {
                    correction |= 0x60;
                    set_carry = true;
                }
                A += correction;
            } else {
                if (F & FlagH) {
                    correction |= 0x06;
                }
                if (F & FlagC) {
                    correction |= 0x60;
                    set_carry = true;
                }
                A -= correction;
            }

            F &= ~(FlagH | FlagZ | FlagC);
            if (set_carry) {
                F |= FlagC;
            }
            if (A == 0) {
                F |= FlagZ;
            }

            cycles = 4;
            break;
        }

        case 0x28: { // JR Z, r8
            int8_t offset = (int8_t)fetch();
            PRINT_DBG_CPU("JR Z, %d\n", offset);
            if (F & FlagZ) {
                PC += offset;
                cycles = 12;
            } else {
                cycles = 8;
            }
            break;
        }

        case 0x2A: // LD A, (HL+)
            PRINT_DBG_CPU("LD A, (HL+)\n");
            A = read(getHL());
            setHL(getHL() + 1);
            cycles = 8;
            break;

        case 0x2B: // DEC HL
            PRINT_DBG_CPU("DEC HL\n");
            setHL(getHL() - 1);
            cycles = 8;
            break;

        case 0x2C:
            PRINT_DBG_CPU("INC L\n");
            L = opINC(L);

            cycles = 4;
            break;

        case 0x2D:
            PRINT_DBG_CPU("DEC L\n");
            F = FlagN | (F & FlagC) | (((L & 0x0F) == 0x00) ? FlagH : 0);
            L--;
            if (L == 0) {
                F |= FlagZ;
            }
            cycles = 4;
            break;

        case 0x2E: // LD L, d8
            L = fetch();
            PRINT_DBG_CPU("LD L, $0x%02X\n", L);
            cycles = 8;
            break;

        case 0x2F: // CPL
            PRINT_DBG_CPU("CPL\n");
            A = ~A;
            F |= (FlagN | FlagH);
            cycles = 4;
            break;

        case 0x29: {
            PRINT_DBG_CPU("ADD HL, HL\n");
            uint16_t res16 = opADD16(getHL());
            H = (res16 >> 8) & 0xFF;
            L = res16 & 0xFF;
            cycles = 8;
            break;
        }

        case 0x30: {
            int8_t offset = (int8_t)fetch();
            PRINT_DBG_CPU("JR NC, %d\n", offset);
            if (!(F & FlagC)) {
                PC += offset;
                cycles = 12;
            } else {
                cycles = 8;
            }
            break;
        }

        case 0x31: { // LD SP, d16
            uint16_t imm16 = fetch16();
            PRINT_DBG_CPU("LD SP, $0x%04X\n", imm16);
            SP = imm16;
            cycles = 12;
            break;
        }

        case 0x32: // LD (HL-), A
            PRINT_DBG_CPU("LD (HL-), A\n");
            write(getHL(), A);
            setHL(getHL() - 1);
            cycles = 8;
            break;

        case 0x33: {
            PRINT_DBG_CPU("INC SP\n");
            SP++;
            cycles = 8;
            break;
        }

        case 0x34: {
            PRINT_DBG_CPU("INC (HL)\n");
            uint16_t address = getHL();
            uint8_t value = read(address);
            
            F &= ~(FlagN | FlagH | FlagZ);
            if ((value & 0x0F) == 0x0F) {
                F |= FlagH;
            }
            
            value++;
            
            if (value == 0) {
                F |= FlagZ;
            }
            
            write(address, value);
            cycles = 12;
            break;
        }

        case 0x35: {
            PRINT_DBG_CPU("DEC (HL)\n");
            uint16_t address = getHL();
            uint8_t value = read(address);
            
            F &= ~(FlagZ | FlagH);
            if ((value & 0x0F) == 0) {
                F |= FlagH;
            }
            
            value--;
            
            if (value == 0) {
                F |= FlagZ;
            }
            F |= FlagN;
            
            write(address, value);
            cycles = 12;
            break;
        }

        case 0x36: { // LD (HL), d8
            uint8_t imm8 = fetch();
            PRINT_DBG_CPU("LD (HL), $0x%02X\n", imm8);
            write(getHL(), imm8);
            cycles = 12;
            break;
        }

        case 0x37: { // SCF
            PRINT_DBG_CPU("SCF\n");
            F &= ~(FlagN | FlagH);
            F |= FlagC;
            cycles = 4;
            break;
        }
        
        case 0x38: {
            int8_t offset = (int8_t)fetch();
            PRINT_DBG_CPU("JR C, %d\n", offset);
            if (F & FlagC) {
                PC += offset;
                cycles = 12;
            } else {
                cycles = 8;
            }
            break;
        }

        case 0x39: {
            PRINT_DBG_CPU("ADD HL, SP\n");
            setHL(opADD16(SP));
            cycles = 8;
            break;
        }

        case 0x3A: {
            PRINT_DBG_CPU("LD A, (HL-)\n");
            uint16_t hl = getHL();
            A = read(hl);
            hl--;
            H = (hl >> 8) & 0xFF;
            L = hl & 0xFF;
            cycles = 8;
            break;
        }

        case 0x3B: {
            PRINT_DBG_CPU("DEC SP\n");
            SP--;
            cycles = 8;
            break;
        }

        case 0x3C:
            PRINT_DBG_CPU("INC A\n");
            A = opINC(A);
            
            cycles = 4;
            break;

        case 0x3D:
            PRINT_DBG_CPU("DEC A\n");
            F &= ~FlagN;
            F &= ~FlagH;
            F &= ~FlagZ;

            if ((A & 0x0F) == 0) {
                F |= FlagH;
            }
            A--;
            if (A == 0) {
                F |= FlagZ;
            }
            F |= FlagN;
            
            cycles = 4;
            break;

        case 0x3E: // LD A, d8
            A = fetch();
            PRINT_DBG_CPU("LD A, $0x%02X\n", A);
            cycles = 8;
            break;

        case 0x3F: {
            PRINT_DBG_CPU("CCF\n");

            bool carry = (F & FlagC) != 0;
            F &= ~(FlagN | FlagH | FlagC);
            if (!carry) {
                F |= FlagC;
            }
            cycles = 4;
            break;
        }

        case 0x40:
            PRINT_DBG_CPU("LD B, B\n");
            //B = B;
            cycles = 4;
            break;

        case 0x41: {
            PRINT_DBG_CPU("LD B, C\n");
            B = C;
            cycles = 4;
            break;
        }

        case 0x42: {
            PRINT_DBG_CPU("LD B, D\n");
            B = D;
            cycles = 4;
            break;
        }

        case 0x43: {
            PRINT_DBG_CPU("LD B, E\n");
            B = E;
            cycles = 4;
            break;
        }

        case 0x44: {
            PRINT_DBG_CPU("LD B, H\n");
            B = H;
            cycles = 4;
            break;
        }
        
        case 0x45: {
            PRINT_DBG_CPU("LD B, L\n");
            B = L;
            cycles = 4;
            break;
        }

        case 0x46:
            PRINT_DBG_CPU("LD B, (HL)\n");
            B = read(getHL());
            cycles = 8;
            break;

        case 0x47: // LD B, A
            PRINT_DBG_CPU("LD B, A\n");
            B = A;
            cycles = 4;
            break;

        case 0x48: {
            PRINT_DBG_CPU("LD C, B\n");
            C = B;
            cycles = 4;
            break;
        }

        case 0x49: {
            PRINT_DBG_CPU("LD C, C\n");
            //C = C;
            cycles = 4;
            break;
        }

        case 0x4A: {
            PRINT_DBG_CPU("LD C, D\n");
            C = D;
            cycles = 4;
            break;
        }

        case 0x4B: {
            PRINT_DBG_CPU("LD C, E\n");
            C = E;
            cycles = 4;
            break;
        }

        case 0x4C: {
            PRINT_DBG_CPU("LD C, H\n");
            C = H;
            cycles = 4;
            break;
        }

        case 0x4D: {
            PRINT_DBG_CPU("LD C, L\n");
            C = L;
            cycles = 4;
            break;
        }

        case 0x4E:
            PRINT_DBG_CPU("LD C, (HL)\n");
            C = read(getHL());
            cycles = 8;
            break;

        case 0x4F: // LD C, A
            PRINT_DBG_CPU("LD C, A\n");
            C = A;
            cycles = 4;
            break;

        case 0x50: {
            PRINT_DBG_CPU("LD D, B\n");
            D = B;
            cycles = 4;
            break;
        }

        case 0x51: {
            PRINT_DBG_CPU("LD D, C\n");
            D = C;
            cycles = 4;
            break;
        }

        case 0x52: {
            PRINT_DBG_CPU("LD D, D\n");
            //D = D;
            cycles = 4;
            break;
        }

        case 0x53: {
            PRINT_DBG_CPU("LD D, E\n");
            D = E;
            cycles = 4;
            break;
        }

        case 0x54: {
            PRINT_DBG_CPU("LD D, H\n");
            D = H;
            cycles = 4;
            break;
        }

        case 0x55: {
            PRINT_DBG_CPU("LD D, L\n");
            D = L;
            cycles = 4;
            break;
        }

        case 0x56: {
            PRINT_DBG_CPU("LD D, (HL)\n");
            D = read(getHL());
            cycles = 8;
            break;
        }

        case 0x57:
            PRINT_DBG_CPU("LD D, A\n");
            D = A;
            cycles = 4;
            break;

        case 0x58: {
            PRINT_DBG_CPU("LD E, B\n");
            E = B;
            cycles = 4;
            break;
        }

        case 0x59: {
            PRINT_DBG_CPU("LD E, C\n");
            E = C;
            cycles = 4;
            break;
        }

        case 0x5A: {
            PRINT_DBG_CPU("LD E, D\n");
            E = D;
            cycles = 4;
            break;
        }

        case 0x5B: {
            PRINT_DBG_CPU("LD E, E\n");
            //E = E;
            cycles = 4;
            break;
        }

        case 0x5C: {
            PRINT_DBG_CPU("LD E, H\n");
            E = H;
            cycles = 4;
            break;
        }

        case 0x5D: {
            PRINT_DBG_CPU("LD E, L\n");
            E = L;
            cycles = 4;
            break;
        }

        case 0x5E: // LD E, (HL)
            PRINT_DBG_CPU("LD E, (HL)\n");
            E = read(getHL());
            cycles = 8;
            break;

        case 0x5F:
            PRINT_DBG_CPU("LD E, A\n");
            E = A;
            cycles = 4;
            break;

        case 0x60:
            PRINT_DBG_CPU("LD H, B\n");
            H = B;
            cycles = 4;
            break;

        case 0x61:
            PRINT_DBG_CPU("LD H, C\n");
            H = C;
            cycles = 4;
            break;

        case 0x62:
            PRINT_DBG_CPU("LD H, D\n");
            H = D;
            cycles = 4;
            break;
        
        case 0x63: {
            PRINT_DBG_CPU("LD H, E\n");
            H = E;
            cycles = 4;
            break;
        }

        case 0x64: {
            PRINT_DBG_CPU("LD H, H\n");
            //H = H;
            cycles = 4;
            break;
        }

        case 0x65: {
            PRINT_DBG_CPU("LD H, L\n");
            H = L;
            cycles = 4;
            break;
        }

        case 0x66: // LD H, (HL)
            PRINT_DBG_CPU("LD H, (HL)\n");
            H = read(getHL());
            cycles = 8;
            break;

        case 0x67: {
            PRINT_DBG_CPU("LD H, A\n");
            H = A;
            cycles = 4;
            break;
        }

        case 0x68: {
            PRINT_DBG_CPU("LD L, B\n");
            L = B;
            cycles = 4;
            break;
        }

        case 0x69: {
            PRINT_DBG_CPU("LD L, C\n");
            L = C;
            cycles = 4;
            break;
        }

        case 0x6A: {
            PRINT_DBG_CPU("LD L, D\n");
            L = D;
            cycles = 4;
            break;
        }

        case 0x6B: {
            PRINT_DBG_CPU("LD L, E\n");
            L = E;
            cycles = 4;
            break;
        }

        case 0x6C: {
            PRINT_DBG_CPU("LD L, H\n");
            L = H;
            cycles = 4;
            break;
        }

        case 0x6D: {
            PRINT_DBG_CPU("LD L, L\n");
            //L = L;
            cycles = 4;
            break;
        }

        case 0x6E: // LD L, (HL)
            PRINT_DBG_CPU("LD L, (HL)\n");
            L = read(getHL());
            cycles = 8;
            break;

        case 0x6F:
            PRINT_DBG_CPU("LD L, A\n");
            L = A;
            cycles = 4;
            break;

        case 0x70:
            PRINT_DBG_CPU("LD (HL), B\n");
            write(getHL(), B);
            cycles = 8;
            break;

        case 0x71:
            PRINT_DBG_CPU("LD (HL), C\n");
            write(getHL(), C);
            cycles = 8;
            break;

        case 0x72:
            PRINT_DBG_CPU("LD (HL), D\n");
            write(getHL(), D);
            cycles = 8;
            break;

        case 0x73: {
            PRINT_DBG_CPU("LD (HL), E\n");
            uint16_t address = getHL();
            write(address, E);
            cycles = 8;
            break;
        }

        case 0x74: {
            PRINT_DBG_CPU("LD (HL), H\n");
            write(getHL(), H);
            cycles = 8;
            break;
        }

        case 0x75: {
            PRINT_DBG_CPU("LD (HL), L\n");
            write(getHL(), L);
            cycles = 8;
            break;
        }

        case 0x76: { // HALT
            PRINT_DBG_CPU("HALT\n");
            halted = true;
            cycles = 4;
            break;
        }

        case 0x77: // LD (HL), A
            PRINT_DBG_CPU("LD (HL), A\n");
            write(getHL(), A);
            cycles = 8;
            break;

        case 0x78: // LD A, B
            PRINT_DBG_CPU("LD A, B\n");
            A = B;
            cycles = 4;
            break;

        case 0x79: // LD A, C
            PRINT_DBG_CPU("LD A, C\n");
            A = C;
            cycles = 4;
            break;

        case 0x7A: // LD A, D
            PRINT_DBG_CPU("LD A, D\n");
            A = D;
            cycles = 4;
            break;

        case 0x7B: // LD A, E
            PRINT_DBG_CPU("LD A, E\n");
            A = E;
            cycles = 4;
            break;

        case 0x7C: // LD A, H
            PRINT_DBG_CPU("LD A, H\n");
            A = H;
            cycles = 4;
            break;

        case 0x7D: // LD A, L
            PRINT_DBG_CPU("LD A, L\n");
            A = L;
            cycles = 4;
            break;

        case 0x7E: // LD A, (HL)
            PRINT_DBG_CPU("LD A, (HL)\n");
            A = read(getHL());
            cycles = 8;
            break;

        case 0x7F: {
            PRINT_DBG_CPU("LD A, A\n");
            //A = A;
            cycles = 4;
            break;
        }

        case 0x80: {
            PRINT_DBG_CPU("ADD A, B\n");
            A = opADD(B);
            cycles = 4;
            break;
        }

        case 0x81: {
            PRINT_DBG_CPU("ADD A, C\n");
            A = opADD(C);
            cycles = 4;
            break;
        }

        case 0x82: {
            PRINT_DBG_CPU("ADD A, D\n");
            A = opADD(D);
            cycles = 4;
            break;
        }

        case 0x83: {
            PRINT_DBG_CPU("ADD A, E\n");
            A = opADD(E);
            cycles = 4;
            break;
        }

        case 0x84: {
            PRINT_DBG_CPU("ADD A, H\n");
            A = opADD(H);
            cycles = 4;
            break;
        }

        case 0x85: {
            PRINT_DBG_CPU("ADD A, L\n");
            A = opADD(L);
            cycles = 4;
            break;
        }

        case 0x86: {
            PRINT_DBG_CPU("ADD A, (HL)\n");
            A = opADD(read(getHL()));
            cycles = 8;
            break;
        }

        case 0x87: {
            PRINT_DBG_CPU("ADD A, A\n");
            A = opADD(A);
            cycles = 4;
            break;
        }

        case 0x88: {
            PRINT_DBG_CPU("ADC A, B\n");
            A = opADC(B);
            cycles = 4;
            break;
        }

        case 0x89: {
            PRINT_DBG_CPU("ADC A, C\n");
            A = opADC(C);
            cycles = 4;
            break;
        }

        case 0x8A: {
            PRINT_DBG_CPU("ADC A, D\n");
            A = opADC(D);
            cycles = 4;
            break;
        }

        case 0x8B: {
            PRINT_DBG_CPU("ADC A, E\n");
            A = opADC(E);
            cycles = 4;
            break;
        }

        case 0x8C: {
            PRINT_DBG_CPU("ADC A, H\n");
            A = opADC(H);
            cycles = 4;
            break;
        }

        case 0x8D: {
            PRINT_DBG_CPU("ADC A, L\n");
            A = opADC(L);
            cycles = 4;
            break;
        }

        case 0x8E: {
            PRINT_DBG_CPU("ADC A, (HL)\n");
            A = opADC(read(getHL()));
            cycles = 8;
            break;
        }

        case 0x8F: {
            PRINT_DBG_CPU("ADC A, A\n");
            A = opADC(A);
            cycles = 4;
            break;
        }

        case 0x90: {
            PRINT_DBG_CPU("SUB A, B\n");
            A = opSUB(B);
            cycles = 4;
            break;
        }

        case 0x91: { // SUB A, C
            PRINT_DBG_CPU("SUB A, C\n");
            A = opSUB(C);
            cycles = 4;
            break;
        }

        case 0x92: { // SUB A, D
            PRINT_DBG_CPU("SUB A, D\n");
            A = opSUB(D);
            cycles = 4;
            break;
        }

        case 0x93: { // SUB A, E
            PRINT_DBG_CPU("SUB A, E\n");
            A = opSUB(E);
            cycles = 4;
            break;
        }

        case 0x94: { // SUB A, H
            PRINT_DBG_CPU("SUB A, H\n");
            A = opSUB(H);
            cycles = 4;
            break;
        }

        case 0x95: { // SUB A, L
            PRINT_DBG_CPU("SUB A, L\n");
            A = opSUB(L);
            cycles = 4;
            break;
        }

        case 0x96: { // SUB A, (HL)
            PRINT_DBG_CPU("SUB A, (HL)\n");
            A = opSUB(read(getHL()));
            cycles = 8;
            break;
        }

        case 0x97: { // SUB A, A
            PRINT_DBG_CPU("SUB A, A\n");
            A = opSUB(A);
            cycles = 4;
            break;
        }

        case 0x98: { // SBC A, B
            PRINT_DBG_CPU("SBC A, B\n");
            A = opSBC(B);
            cycles = 4;
            break;
        }

        case 0x99: { // SBC A, C
            PRINT_DBG_CPU("SBC A, C\n");
            A = opSBC(C);
            cycles = 4;
            break;
        }

        case 0x9A: { // SBC A, D
            PRINT_DBG_CPU("SBC A, D\n");
            A = opSBC(D);
            cycles = 4;
            break;
        }

        case 0x9B: { // SBC A, E
            PRINT_DBG_CPU("SBC A, E\n");
            A = opSBC(E);
            cycles = 4;
            break;
        }

        case 0x9C: { // SBC A, H
            PRINT_DBG_CPU("SBC A, H\n");
            A = opSBC(H);
            cycles = 4;
            break;
        }

        case 0x9D: { // SBC A, L
            PRINT_DBG_CPU("SBC A, L\n");
            A = opSBC(L);
            cycles = 4;
            break;
        }

        case 0x9E: { // SBC A, (HL)
            PRINT_DBG_CPU("SBC A, (HL)\n");
            A = opSBC(read(getHL()));
            cycles = 8;
            break;
        }

        case 0x9F: { // SBC A, A
            PRINT_DBG_CPU("SBC A, A\n");
            A = opSBC(A);
            cycles = 4;
            break;
        }

        case 0xA0: {
            PRINT_DBG_CPU("AND B\n");
            A = opAND(B);
            cycles = 4;
            break;
        }

        case 0xA1: {
            PRINT_DBG_CPU("AND C\n");
            A = opAND(C);
            cycles = 4;
            break;
        }

        case 0xA2: {
            PRINT_DBG_CPU("AND D\n");
            A = opAND(D);
            cycles = 4;
            break;
        }

        case 0xA3: {
            PRINT_DBG_CPU("AND E\n");
            A = opAND(E);
            cycles = 4;
            break;
        }

        case 0xA4: {
            PRINT_DBG_CPU("AND H\n");
            A = opAND(H);
            cycles = 4;
            break;
        }

        case 0xA5: {
            PRINT_DBG_CPU("AND L\n");
            A = opAND(L);
            cycles = 4;
            break;
        }

        case 0xA6: {
            PRINT_DBG_CPU("AND (HL)\n");
            A = opAND(read(getHL()));
            cycles = 8;
            break;
        }

        case 0xA7: {
            PRINT_DBG_CPU("AND A\n");
            A = opAND(A);
            cycles = 4;
            break;
        }

        case 0xA8: // XOR B
            PRINT_DBG_CPU("XOR B\n");
            A = opXOR(B);
            cycles = 4;
            break;

        case 0xA9: // XOR C
            PRINT_DBG_CPU("XOR C\n");
            A = opXOR(C);
            cycles = 4;
            break;

        case 0xAA: // XOR D
            PRINT_DBG_CPU("XOR D\n");
            A = opXOR(D);
            cycles = 4;
            break;

        case 0xAB: // XOR E
            PRINT_DBG_CPU("XOR E\n");
            A = opXOR(E);
            cycles = 4;
            break;

        case 0xAC: // XOR H
            PRINT_DBG_CPU("XOR H\n");
            A = opXOR(H);
            cycles = 4;
            break;

        case 0xAD: // XOR L
            PRINT_DBG_CPU("XOR L\n");
            A = opXOR(L);
            cycles = 4;
            break;

        case 0xAE: // XOR (HL)
            PRINT_DBG_CPU("XOR (HL)\n");
            A = opXOR(read(getHL()));
            cycles = 8;
            break;

        case 0xAF: // XOR A
            PRINT_DBG_CPU("XOR A\n");
            A = opXOR(A);
            cycles = 4;
            break;

        case 0xB0: {
            PRINT_DBG_CPU("OR B\n");
            A = opOR(B);
            cycles = 4;
            break;
        }

        case 0xB1: {
            PRINT_DBG_CPU("OR C\n");
            A = opOR(C);
            cycles = 4;
            break;
        }

        case 0xB2: {
            PRINT_DBG_CPU("OR D\n");
            A = opOR(D);
            cycles = 4;
            break;
        }

        case 0xB3: {
            PRINT_DBG_CPU("OR E\n");
            A = opOR(E);
            cycles = 4;
            break;
        }

        case 0xB4: {
            PRINT_DBG_CPU("OR H\n");
            A = opOR(H);
            cycles = 4;
            break;
        }

        case 0xB5: {
            PRINT_DBG_CPU("OR L\n");
            A = opOR(L);
            cycles = 4;
            break;
        }

        case 0xB6: {
            PRINT_DBG_CPU("OR (HL)\n");
            A = opOR(read(getHL()));
            cycles = 8;
            break;
        }

        case 0xB7: {
            PRINT_DBG_CPU("OR A\n");
            A = opOR(A);
            cycles = 4;
            break;
        }

        case 0xB8: {
            PRINT_DBG_CPU("CP B\n");
            opSUB(B);
            cycles = 4;
            break;
        }

        case 0xB9: {
            PRINT_DBG_CPU("CP C\n");
            opSUB(C);
            cycles = 4;
            break;
        }

        case 0xBA: { // CP D
            PRINT_DBG_CPU("CP D\n");
            opSUB(D);
            cycles = 4;
            break;
        }

        case 0xBC: { // CP H
            PRINT_DBG_CPU("CP H\n");
            opSUB(H);
            cycles = 4;
            break;
        }

        case 0xBD: { // CP L
            PRINT_DBG_CPU("CP L\n");
            opSUB(L);
            cycles = 4;
            break;
        }

        case 0xBF: { // CP A
            PRINT_DBG_CPU("CP A\n");
            opSUB(A);
            cycles = 4;
            break;
        }

        case 0xBB: { // CP E
            PRINT_DBG_CPU("CP E\n");
            uint8_t res = A - E;
            
            F = FlagN;
            if (res == 0) F |= FlagZ;
            if (A < E)    F |= FlagC;
            if ((A & 0x0F) < (E & 0x0F)) F |= FlagH;
            
            cycles = 4;
            break;
        }

        case 0xBE: {
            PRINT_DBG_CPU("CP (HL)\n");
            uint16_t hl = getHL();
            uint8_t value = read(hl);
            uint8_t result = A - value;

            F = 0;
            F |= FlagN;

            if (result == 0) F |= FlagZ;
            if ((A & 0xF) < (value & 0xF)) F |= FlagH;
            if (A < value) F |= FlagC;

            cycles = 8;
            break;
        }

        case 0xC0:
            PRINT_DBG_CPU("RET NZ\n");
            if (!(F & FlagZ)) {
                uint8_t low = pop();
                uint8_t high = pop();
                PC = (high << 8) | low;
                cycles = 20;
            } else {
                cycles = 8;
            }
            break;

        case 0xC1: // POP BC
            PRINT_DBG_CPU("POP BC\n");
            C = pop();
            B = pop();
            cycles = 12;
            break;

        case 0xC2: {
            uint8_t low = fetch();
            uint8_t high = fetch();
            uint16_t address = (high << 8) | low;
            PRINT_DBG_CPU("JP NZ, $0x%04X\n", address);
            
            if (!(F & FlagZ)) {
                PC = address;
                cycles = 16;
            } else {
                cycles = 12;
            }
            break;
        }

        case 0xC3: { // JP nn
            uint16_t dest = fetch16();
            PRINT_DBG_CPU("JP $0x%04X\n", dest);
            PC = dest;
            cycles = 16;
            break;
        }

        case 0xC4: { 
            uint16_t dest = fetch16();
            if (!(F & FlagZ)) {
                push(PC >> 8);
                push(PC & 0xFF);
                PC = dest;
                cycles = 24;
            } else {
                cycles = 12;
            }
            break;
        }

        case 0xC7:
            PRINT_DBG_CPU("RST 00H\n");
            push((PC >> 8) & 0xFF);
            push(PC & 0xFF);
            PC = 0x0000;
            cycles = 16;
            break;

        case 0xCD: { // CALL nn
            uint16_t dest = fetch16();
            PRINT_DBG_CPU("CALL $0x%04X\n", dest);
            push(PC >> 8);
            push(PC & 0xFF);
            PC = dest;
            cycles = 24;
            break;
        }

        case 0xCF:
            PRINT_DBG_CPU("RST 08H\n");
            push((PC >> 8) & 0xFF);
            push(PC & 0xFF);
            PC = 0x0008;
            cycles = 16;
            break;

        case 0xC5: // PUSH BC
            PRINT_DBG_CPU("PUSH BC\n");
            push(B);
            push(C);
            cycles = 16;
            break;

        case 0xC6: {
            uint8_t imm8 = fetch();
            PRINT_DBG_CPU("ADD A, $0x%02X\n", imm8);
            A = opADD(imm8);
            cycles = 8;
            break;
        }

        case 0xC8:
            PRINT_DBG_CPU("RET Z\n");
            if (F & FlagZ) {
                uint8_t low = pop();
                uint8_t high = pop();
                PC = (high << 8) | low;
                cycles = 20;
            } else {
                cycles = 8;
            }
            break;

        case 0xC9: { // RET
            PRINT_DBG_CPU("RET\n");
            uint8_t low = pop();
            uint8_t high = pop();
            PC = (high << 8) | low;
            cycles = 16;
            break;
        }

        case 0xCA: {
            uint8_t low = fetch();
            uint8_t high = fetch();
            uint16_t address = (high << 8) | low;
            PRINT_DBG_CPU("JP Z, $0x%04X\n", address);
            
            if (F & FlagZ) {
                PC = address;
                cycles = 16;
            } else {
                cycles = 12;
            }
            break;
        }

        case 0xCC: { // CALL Z, nn
            uint16_t dest = fetch16();
            if (F & FlagZ) {
                push(PC >> 8);
                push(PC & 0xFF);
                PC = dest;
                cycles = 24;
            } else {
                cycles = 12;
            }
            break;
        }

        case 0xCE: {
            uint8_t imm8 = fetch();
            PRINT_DBG_CPU("ADC A, $0x%02X\n", imm8);
            A = opADC(imm8);
            cycles = 8;
            break;
        }

        case 0xD0:
            PRINT_DBG_CPU("RET NC\n");
            if (!(F & FlagC)) {
                uint8_t low = pop();
                uint8_t high = pop();
                PC = (high << 8) | low;
                cycles = 20;
            } else {
                cycles = 8;
            }
            break;

        case 0xD1:
            PRINT_DBG_CPU("POP DE\n");
            E = pop();
            D = pop();
            cycles = 12;
            break;

        case 0xD2: {
            uint16_t address = fetch16();
            PRINT_DBG_CPU("JP NC, $0x%04X\n", address);
            
            if (!(F & FlagC)) {
                PC = address;
                cycles = 16;
            } else {
                cycles = 12;
            }
            break;
        }

        case 0xD4: {
            uint16_t dest = fetch16();
            PRINT_DBG_CPU("CALL NC, $0x%04X\n", dest);
            
            if (!(F & FlagC)) {
                push(PC >> 8);
                push(PC & 0xFF);
                PC = dest;
                cycles = 24;
            } else {
                cycles = 12;
            }
            break;
        }

        case 0xD5:
            PRINT_DBG_CPU("PUSH DE\n");
            push(D);
            push(E);
            cycles = 16;
            break;

        case 0xD6: {
            uint8_t imm8 = fetch();
            PRINT_DBG_CPU("SUB A, $0x%02X\n", imm8);
            A = opSUB(imm8);
            cycles = 8;
            break;
        }

        case 0xD7:
            PRINT_DBG_CPU("RST 10H\n");
            push((PC >> 8) & 0xFF);
            push(PC & 0xFF);
            PC = 0x0010;
            cycles = 16;
            break;

        case 0xD8:
            PRINT_DBG_CPU("RET C\n");
            if (F & FlagC) {
                uint8_t low = pop();
                uint8_t high = pop();
                PC = (high << 8) | low;
                cycles = 20;
            } else {
                cycles = 8;
            }
            break;

        case 0xD9: { // RETI
            PRINT_DBG_CPU("RETI\n");
            uint8_t low = pop();
            uint8_t high = pop();
            PC = (high << 8) | low;
            IME = true;
            cycles = 16;
            break;
        }

        case 0xDA: {
            uint16_t address = fetch16();
            PRINT_DBG_CPU("JP C, $0x%04X\n", address);
            
            if (F & FlagC) {
                PC = address;
                cycles = 16;
            } else {
                cycles = 12;
            }
            break;
        }

        case 0xDC: {
            uint16_t addr = fetch16();
            PRINT_DBG_CPU("CALL C, $0x%04X\n", addr);
            if (F & FlagC) {
                push((PC >> 8) & 0xFF);
                push(PC & 0xFF);
                PC = addr;
                cycles = 24;
            } else {
                cycles = 12;
            }
            break;
        }

        case 0xDE: {
            uint8_t imm8 = fetch();
            PRINT_DBG_CPU("SBC A, $0x%02X\n", imm8);
            A = opSBC(imm8);
            cycles = 8;
            break;
        }

        case 0xDF:
            PRINT_DBG_CPU("RST 18H\n");
            push((PC >> 8) & 0xFF);
            push(PC & 0xFF);
            PC = 0x0018;
            cycles = 16;
            break;

        case 0xE0: { // LDH (a8), A
            uint8_t offset = fetch();
            PRINT_DBG_CPU("LDH ($FF00 + $0x%02X), A\n", offset);
            write(0xFF00 + offset, A);
            cycles = 12;
            break;
        }

        case 0xE1:
            PRINT_DBG_CPU("POP HL\n");
            L = pop();
            H = pop();
            cycles = 12;
            break;

        case 0xE2: // LD ($FF00+C), A
            PRINT_DBG_CPU("LD ($FF00+C), A\n");
            write(0xFF00 + C, A);
            cycles = 8;
            break;

        case 0xE5: // PUSH HL
            PRINT_DBG_CPU("PUSH HL\n");
            push(H);
            push(L);
            cycles = 16;
            break;

        case 0xE6: {
            uint8_t imm8 = fetch();
            PRINT_DBG_CPU("AND $0x%02X\n", imm8);
            A = opAND(imm8);
            cycles = 8;
            break;
        }

        case 0xE7:
            PRINT_DBG_CPU("RST 20H\n");
            push((PC >> 8) & 0xFF);
            push(PC & 0xFF);
            PC = 0x0020;
            cycles = 16;
            break;

        case 0xE8: {
            int8_t offset = (int8_t)fetch();
            PRINT_DBG_CPU("ADD SP, %d\n", offset);

            uint16_t unsignedOff = (uint16_t)(uint8_t)offset;
            bool hFlag = ((SP & 0x0F) + (unsignedOff & 0x0F)) > 0x0F;
            bool cFlag = ((SP & 0xFF) + (unsignedOff & 0xFF)) > 0xFF;

            SP = (uint16_t)(SP + offset);

            F = 0;
            if (hFlag) F |= FlagH;
            if (cFlag) F |= FlagC;

            cycles = 16;
            break;
        }

        case 0xE9: {
            PRINT_DBG_CPU("JP (HL)\n");
            PC = getHL();
            cycles = 4;
            break;
        }

        case 0xEA: { // LD (nn), A
            uint16_t dest = fetch16();
            PRINT_DBG_CPU("LD ($0x%04X), A\n", dest);
            write(dest, A);
            cycles = 16;
            break;
        }

        case 0xEE: { // XOR d8
            uint8_t imm8 = fetch();
            PRINT_DBG_CPU("XOR $0x%02X\n", imm8);
            A = opXOR(imm8);
            cycles = 8;
            break;
        }

        case 0xEF: { // RST $28
            PRINT_DBG_CPU("RST $28\n");
            SP -= 2;
            write(SP, PC & 0xFF);
            write(SP + 1, (PC >> 8) & 0xFF);

            PC = 0x0028;
            cycles = 16;
            break;
        }

        case 0xF0: { // LDH A, (a8)
            uint8_t offset = fetch();
            PRINT_DBG_CPU("LDH A, ($FF00 + $0x%02X)\n", offset);
            A = read(0xFF00 + offset);
            cycles = 12;
            break;
        }

        case 0xF1: // POP AF
            PRINT_DBG_CPU("POP AF\n");
            F = pop() & 0xF0;
            A = pop();
            cycles = 12;
            break;

        case 0xF2: // LD A, ($FF00+C)
            PRINT_DBG_CPU("LD A, ($FF00+C)\n");
            A = read(0xFF00 + C);
            cycles = 8;
            break;

        case 0xF3: // DI
            PRINT_DBG_CPU("DI\n");
            IME = false;
            cycles = 4;
            break;

        case 0xF5: // PUSH AF
            PRINT_DBG_CPU("PUSH AF\n");
            push(A);
            push(F & 0xF0);
            cycles = 16;
            break;

        case 0xF6: {
            uint8_t imm8 = fetch();
            PRINT_DBG_CPU("OR $0x%02X\n", imm8);
            A = opOR(imm8);
            cycles = 8;
            break;
        }

        case 0xF7:
            PRINT_DBG_CPU("RST 30H\n");
            push((PC >> 8) & 0xFF);
            push(PC & 0xFF);
            PC = 0x0030;
            cycles = 16;
            break;

        case 0xF8: {
            int8_t offset = (int8_t)fetch();
            PRINT_DBG_CPU("LD HL, SP + %d\n", offset);

            uint16_t unsignedOff = (uint16_t)(uint8_t)offset;
            bool hFlag = ((SP & 0x0F) + (unsignedOff & 0x0F)) > 0x0F;
            bool cFlag = ((SP & 0xFF) + (unsignedOff & 0xFF)) > 0xFF;

            setHL((uint16_t)(SP + offset));

            F = 0;
            if (hFlag) F |= FlagH;
            if (cFlag) F |= FlagC;

            cycles = 12;
            break;
        }

        case 0xF9: {
            PRINT_DBG_CPU("LD SP, HL\n");
            SP = (uint16_t)((H << 8) | L);
            cycles = 8;
            break;
        }

        case 0xFA: { // LD A, (nn)
            uint16_t src = fetch16();
            PRINT_DBG_CPU("LD A, ($0x%04X)\n", src);
            A = read(src);
            cycles = 16;
            break;
        }

        case 0xFB: // EI
            PRINT_DBG_CPU("EI\n");
            EIPending = 2; 
            cycles = 4;
            break;

        case 0xFE: { // CP d8
            uint8_t imm8 = fetch();
            PRINT_DBG_CPU("CP $0x%02X\n", imm8);
            
            F = FlagN;
            if (A == imm8) F |= FlagZ;
            if ((A & 0x0F) < (imm8 & 0x0F)) F |= FlagH;
            if (A < imm8) F |= FlagC;
            
            cycles = 8;
            break;
        }

        case 0xFF:
            PRINT_DBG_CPU("RST 38H\n");
            push((PC >> 8) & 0xFF);
            push(PC & 0xFF);
            PC = 0x0038;
            cycles = 16;
            break;

        case 0xCB: { // prefix CB
            uint8_t cbOpcode = fetch();
            PRINT_DBG_CPU("CB %u\n", cbOpcode);
            executeCB(cbOpcode);
            break;
        }

        default: {
            char errorMsg[256];
            sprintf(errorMsg, "CPU crashed after reaching unimplemented opcode 0x%02X", opcode);
            romIsLoaded = false;
            reset();
            DebugPrintLog("CPU", "%s", errorMsg);
            QMessageBox::critical((QMainWindow*)globalQTWin, "Fatal error", errorMsg);
            break;
        }
    }
}

void GbCPU::executeCB(uint8_t opcode) {
    int mode = (opcode >> 6) & 3; 
    int param = (opcode >> 3) & 7;
    int reg = opcode & 7;

    uint8_t value = 0;
    uint16_t hlAddr = getHL();

    if (reg == 6) {
        value = read(hlAddr);
        cycles = 16;
    } else {
        switch (reg) {
            case 0: value = B; break;
            case 1: value = C; break;
            case 2: value = D; break;
            case 3: value = E; break;
            case 4: value = H; break;
            case 5: value = L; break;
            case 7: value = A; break;
        }
        cycles = 8;
    }

    switch (mode) {
        case 0:
            switch (param) {
                case 0: value = cbRLC(value); break;
                case 1: value = cbRRC(value); break;
                case 2: value = cbRL(value); break;
                case 3: value = cbRR(value); break;
                case 4: value = cbSLA(value); break;
                case 5: value = cbSRA(value); break;
                case 6: value = cbSWAP(value); break;
                case 7: value = cbSRL(value); break;
            }
            break;

        case 1:
            cbBIT(param, value);
            if (reg == 6) {
                cycles = 12;
            }
            return;

        case 2:
            value &= ~(1 << param);
            break;

        case 3:
            value |= (1 << param);
            break;
    }

    if (reg == 6) {
        write(hlAddr, value);
    } else {
        switch (reg) {
            case 0: B = value; break;
            case 1: C = value; break;
            case 2: D = value; break;
            case 3: E = value; break;
            case 4: H = value; break;
            case 5: L = value; break;
            case 7: A = value; break;
        }
    }
}

uint8_t GbCPU::read(uint16_t addr) {
    GbROM *rom = getGBRom();

    if (useBootROM) {
        if (rom->isCGB) {
            if (addr < 0x0100 || (addr >= 0x0200 && addr < 0x0900)) {
                return rom->bootROM[addr];
            }
        } else {
            if (addr < 0x0100) {
                return rom->bootROM[addr];
            }
        }
    }

    if (addr >= 0x8000 && addr <= 0x9FFF) {
        return ppu->readVRAM(addr);
    }

    if (addr >= 0xFE00 && addr <= 0xFE9F) {
        return ppu->readOAM(addr);
    }

    if (addr == 0xFF00) {
        uint8_t res = 0xC0 | (joypadReg & 0x30) | 0x0F; 
        uint8_t state = controllers[0].state; 

        if ((joypadReg & 0x10) == 0) {
            if (state & (1 << 7)) res &= ~0x01;
            if (state & (1 << 6)) res &= ~0x02;
            if (state & (1 << 4)) res &= ~0x04;
            if (state & (1 << 5)) res &= ~0x08;
        }
        if ((joypadReg & 0x20) == 0) {
            if (state & (1 << 0)) res &= ~0x01;
            if (state & (1 << 1)) res &= ~0x02;
            if (state & (1 << 2)) res &= ~0x04;
            if (state & (1 << 3)) res &= ~0x08;
        }

        return res;
    }

    if (addr == 0xFF0F) {
        return IF | 0xE0;
    }
    
    if ((addr >= 0xFF40 && addr <= 0xFF4B) || addr == 0xFF4F || (addr >= 0xFF68 && addr <= 0xFF6C)) {
        return ppu->readRegister(addr);
    }

    if (addr == 0xFF4C) {
        return KEY0;
    }
    if (addr == 0xFF4D) {
        return KEY1 | (doubleSpeed ? 0x80 : 0x00) | 0x7E;
    }

    if (addr >= 0xFF51 && addr <= 0xFF55) {
        switch (addr) {
            case 0xFF51: return (HDMASrc >> 8) & 0xFF;
            case 0xFF52: return HDMASrc & 0xFF;
            case 0xFF53: return (HDMADst >> 8) & 0xFF;
            case 0xFF54: return HDMADst & 0xFF;
            case 0xFF55: return HDMALength | (HDMAActive ? 0x00 : 0x80);
        }
    }
    
    if (addr == 0xFF04) {
        return DIV;
    }
    if (addr == 0xFF05) {
        return TIMA;
    }
    if (addr == 0xFF06) {
        return TMA;
    }
    if (addr == 0xFF07) {
        return TAC;
    }
    if (addr >= 0xFF10 && addr <= 0xFF3F) { // apu
        return apu->read(addr);
    }
    if (addr >= 0xC000 && addr <= 0xCFFF) {
        return rom->mbc->WRAM[addr - 0xC000];
    }
    if (addr >= 0xD000 && addr <= 0xDFFF) {
        if (rom->isCGB) {
            uint8_t bank = SVBK & 0x07;
            if (bank == 0) bank = 1;
            return rom->mbc->WRAM[(bank * 0x1000) + (addr - 0xD000)];
        } else {
            return rom->mbc->cpuRead(addr);
        }
    }
    if (addr >= 0xE000 && addr <= 0xFDFF) {
        return read(addr - 0x2000);
    }
    if (addr == 0xFF70) {
        return rom->isCGB ? (SVBK | 0xF8) : 0xFF;
    }
    if (addr >= 0xFF80 && addr <= 0xFFFE) {
        return rom->mbc->HRAM[addr - 0xFF80];
    }
    if (addr == 0xFFFF) {
        return IE;
    }
    return rom->mbc->cpuRead(addr);
}

void GbCPU::write(uint16_t addr, uint8_t value) {
    GbROM *rom = getGBRom();

    if (addr >= 0x8000 && addr <= 0x9FFF) {
        ppu->writeVRAM(addr, value);
        return;
    }

    if (addr >= 0xFE00 && addr <= 0xFE9F) {
        ppu->writeOAM(addr, value);
        return;
    }
    if (addr == 0xFF00) {
        joypadReg = (joypadReg & 0xCF) | (value & 0x30);
        return;
    }
    if (addr == 0xFF0F) {
        IF = value | 0xE0;
        return;
    }

    if ((addr >= 0xFF40 && addr <= 0xFF4B) || addr == 0xFF4F || (addr >= 0xFF68 && addr <= 0xFF6C)) {
        ppu->writeRegister(addr, value);
        return;
    }

    if (addr == 0xFF4C) {
        if (rom->isCGB) {
            KEY0 = value;
        }
        return;
    }

    if (addr == 0xFF4D) {
        if (rom->isCGB) {
            KEY1 = (KEY1 & 0x80) | (value & 0x01);
        }
        return;
    }

    if (addr >= 0xFF51 && addr <= 0xFF54) {
        switch (addr) {
            case 0xFF51: HDMASrc = (HDMASrc & 0x00FF) | (value << 8); break;
            case 0xFF52: HDMASrc = (HDMASrc & 0xFF00) | (value & 0xF0); break;
            case 0xFF53: HDMADst = (HDMADst & 0x00FF) | ((value & 0x1F) << 8); break;
            case 0xFF54: HDMADst = (HDMADst & 0xFF00) | (value & 0xF0); break;
        }
        return;
    }

    if (addr == 0xFF55) {
        bool isHBlank = (value & 0x80) != 0;
        uint16_t length = ((value & 0x7F) + 1) * 16;

        if (HDMAActive && isHBlank) {
            HDMAActive = false;
            HDMALength = value & 0x7F;
            return;
        }

        if (!isHBlank) {
            for (uint16_t i = 0; i < length; i++) {
                uint8_t dat = read(HDMASrc + i);
                write(0x8000 + ((HDMADst + i) & 0x1FFF), dat);
            }
            HDMAActive = false;
            HDMALength = 0xFF;
        } else {
            HDMAActive = true;
            HDMALength = value & 0x7F;
        }
        return;
    }

    if (addr == 0xFF04) {
        DIV = 0;
        DIVInternal = 0;
        return;
    }
    if (addr == 0xFF05) {
        TIMA = value;
        return;
    }
    if (addr == 0xFF06) {
        TMA = value;
        return;
    }
    if (addr == 0xFF07) {
        TAC = value & 0x07;
        return;
    }
    if (addr >= 0xFF10 && addr <= 0xFF3F) { // apu
        apu->write(addr, value);
        return;
    }
    if (addr >= 0xC000 && addr <= 0xCFFF) {
        rom->mbc->WRAM[addr - 0xC000] = value;
        return;
    }
    if (addr >= 0xD000 && addr <= 0xDFFF) {
        if (rom->isCGB) {
            uint8_t bank = SVBK & 0x07;
            if (bank == 0) bank = 1;
            rom->mbc->WRAM[(bank * 0x1000) + (addr - 0xD000)] = value;
        } else {
            rom->mbc->cpuWrite(addr, value);
        }
        return;
    }
    if (addr >= 0xE000 && addr <= 0xFDFF) {
        write(addr - 0x2000, value);
        return;
    }
    if (addr == 0xFF70) {
        if (rom->isCGB) {
            SVBK = value & 0x07;
        }
        return;
    }
    if (addr == 0xFF01) {
        serialData = value;
        return;
    }
    if (addr == 0xFF02) {
        if (value & 0x80) {
            serialControl = (value & 0x7F);
            IF |= 0x08;
        } else {
            serialControl = value;
        }
        if (value == 0x81) {
            if (serialData == 0x0A) {
                DebugPrintLog("GAMEBOY", "Serial output: %s", serialBuffer.c_str());
                serialBuffer.clear();
            } else {
                serialBuffer += (char)(serialData);
            }
        }
        return;
    }
    if (addr == 0xFF50) {
        if (value != 0) {
            useBootROM = false;
            DebugPrintLog("CPU", "Boot ROM done, jumping to game");
        }
        return;
    }
    if (addr >= 0xFF80 && addr <= 0xFFFE) {
        rom->mbc->HRAM[addr - 0xFF80] = value;
        return;
    }
    if (addr == 0xFFFF) {
        IE = value;
        return;
    }
    rom->mbc->cpuWrite(addr, value);
}

void GbCPU::push(uint8_t value) {
    SP--;
    write(SP, value);
}

uint8_t GbCPU::pop() {
    uint8_t value = read(SP);
    SP++;
    return value;
}