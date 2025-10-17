#include "nes_cpu.hpp"
#include "nes_controller.hpp"

//#define NES_DEBUG

#ifdef  NES_DEBUG
#define DEBUG_LOG(...) printf(__VA_ARGS__);
#define DEBUG_LOG2(...) printf(__VA_ARGS__); printf("\n");
#else
#define DEBUG_LOG(...) //printf(__VA_ARGS__);
#define DEBUG_LOG2(...) //printf(__VA_ARGS__); printf("\n");
#endif

CPU cpu;

void CPU::run(uint32_t maxCycles) {
    uint32_t cycles_run = 0;
    //uint32_t frame_ppu_counter = 0;

    while (cycles_run < maxCycles) {
        if (!romIsLoaded || CPUPaused) return;
        bool prevNMIDetect = NMIDetector;
        NMIDetector = ppu.Vblank && ppu.enableNMI;

        if (!prevNMIDetect && NMIDetector) HandleNMI();

        uint8_t opcode = fetch();
        execute(opcode);

        ppu.Step();
        ppu.Step();
        ppu.Step();

        cycles_run += cycles;
        cycles = 0;
    }
}

void CPU::execute(uint8_t opcode)
{
    auto readIndirect = [this](uint16_t addr) -> uint16_t
    {
        uint8_t lo = read(addr);
        uint8_t hi;
        if ((addr & 0x00FF) == 0xFF)
            hi = read(addr & 0xFF00);
        else
            hi = read(addr + 1);
        return (hi << 8) | lo;
    };

    auto branch = [this](bool condition, int8_t offset)
    {
        if (condition)
        {
            uint16_t oldPC = PC;
            PC += offset;
            cycles += 1;
            if ((oldPC & 0xFF00) != (PC & 0xFF00))
                cycles += 1;
        }
    };

    auto lda = [this](uint8_t value, int baseCycles) {
        A = value;
        SetZN(A);
        cycles += baseCycles;
    };

    auto lda_read = [this, &lda](uint16_t addr, int baseCycles, bool checkPage = false, uint16_t offset = 0) {
        uint16_t effective = addr + offset;
        lda(read(effective), baseCycles);
        if (checkPage && ((addr & 0xFF00) != (effective & 0xFF00)))
            cycles += 1;
    };

    auto ld_reg = [this](uint8_t &reg, uint8_t value, int baseCycles) {
        reg = value;
        SetZN(reg);
        cycles += baseCycles;
    };

    auto ld_reg_read = [this, &ld_reg](uint8_t &reg, uint16_t addr, int baseCycles, bool checkPage = false, uint16_t offset = 0) {
        uint16_t effective = addr + offset;
        ld_reg(reg, read(effective), baseCycles);
        if (checkPage && ((addr & 0xFF00) != (effective & 0xFF00)))
            cycles += 1;
    };

    auto st_reg = [this](uint16_t addr, uint8_t reg, int baseCycles) {
        write(addr, reg);
        cycles += baseCycles;
    };

    auto inc_reg = [this](uint16_t addr, int baseCycles) {
        uint8_t value = read(addr);
        value++;
        write(addr, value);
        SetZN(value);
        cycles += baseCycles;
    };

    auto dec_reg = [this](uint16_t addr, int baseCycles) {
        uint8_t value = read(addr);
        value--;
        write(addr, value);
        SetZN(value);
        cycles += baseCycles;
    };

    auto adc_op = [this](uint8_t value, int baseCycles) {
        uint16_t sum = A + value + (P & 0x01);
        SetZN(sum & 0xFF);
        if (sum > 0xFF) P |= 0x01; else P &= ~0x01;
        if (((A ^ sum) & (value ^ sum) & 0x80) != 0) P |= 0x40; else P &= ~0x40;
        A = sum & 0xFF;
        cycles += baseCycles;
    };

    auto sbc_op = [this](uint8_t value, int baseCycles) {
        value ^= 0xFF; // invert for SBC
        uint16_t sum = A + value + (P & 0x01);
        SetZN(sum & 0xFF);
        if (sum > 0xFF) P |= 0x01; else P &= ~0x01;
        if (((A ^ sum) & (value ^ sum) & 0x80) != 0) P |= 0x40; else P &= ~0x40;
        A = sum & 0xFF;
        cycles += baseCycles;
    };

    auto and_op = [this](uint8_t value, int baseCycles) {
        A &= value;
        SetZN(A);
        cycles += baseCycles;
    };

    auto ora_op = [this](uint8_t value, int baseCycles) {
        A |= value;
        SetZN(A);
        cycles += baseCycles;
    };

    auto lsr = [this](uint8_t &reg) {
        P = (reg & 0x01) ? (P | 0x01) : (P & ~0x01);
        reg >>= 1;
        SetZN(reg);
    };

    auto lsr_mem = [this](uint16_t addr, int baseCycles) {
        uint8_t value = read(addr);
        P = (value & 0x01) ? (P | 0x01) : (P & ~0x01);
        value >>= 1;
        write(addr, value);
        SetZN(value);
        cycles += baseCycles;
    };

    auto rol = [this](uint8_t &reg) {
        uint8_t old = reg;
        reg = (reg << 1) | (P & 0x01);
        P = (old & 0x80) ? (P | 0x01) : (P & ~0x01);
        SetZN(reg);
    };

    auto rol_mem = [this](uint16_t addr, int baseCycles) {
        uint8_t value = read(addr);
        uint8_t old = value;
        value = (value << 1) | (P & 0x01);
        P = (old & 0x80) ? (P | 0x01) : (P & ~0x01);
        write(addr, value);
        SetZN(value);
        cycles += baseCycles;
    };

    auto ror = [this](uint8_t &reg) {
        uint8_t old = reg;
        uint8_t carryIn = (P & 0x01) ? 0x80 : 0x00;
        P = (old & 0x01) ? (P | 0x01) : (P & ~0x01);
        reg = (old >> 1) | carryIn;
        SetZN(reg);
    };

    auto ror_mem = [this](uint16_t addr, int baseCycles) {
        uint8_t value = read(addr);
        uint8_t carryIn = (P & 0x01) ? 0x80 : 0x00;
        P = (value & 0x01) ? (P | 0x01) : (P & ~0x01);
        value = (value >> 1) | carryIn;
        write(addr, value);
        SetZN(value);
        cycles += baseCycles;
    };

    auto asl = [this](uint8_t &reg) {
        P = (reg & 0x80) ? (P | 0x01) : (P & ~0x01);
        reg <<= 1;
        SetZN(reg);
    };

    auto asl_mem = [this](uint16_t addr, int baseCycles) {
        uint8_t value = read(addr);
        P = (value & 0x80) ? (P | 0x01) : (P & ~0x01);
        value <<= 1;
        write(addr, value);
        SetZN(value);
        cycles += baseCycles;
    };

    auto cmp_reg = [this](uint8_t reg, uint8_t value, int baseCycles) {
        uint8_t r = reg - value;
        SetZN(r);
        P = (reg >= value ? P | 0x01 : P & ~0x01);
        cycles += baseCycles;
    };

    auto slo = [this](uint16_t addr) {
        uint8_t value = read(addr);
        P = (value & 0x80) ? (P | 0x01) : (P & ~0x01);

        value <<= 1;
        write(addr, value);

        A |= value;

        SetZN(A);
    };

    auto sre = [this](uint16_t addr) {
        uint8_t value = read(addr);
        uint8_t oldBit0 = value & 1;

        value >>= 1;
        write(addr, value);

        A ^= value;
        SetZN(A);
        if (oldBit0) P |= 0x01; else P &= ~0x01;
    };

    auto rra = [this](uint16_t addr) {
        uint8_t value = read(addr);
        uint8_t oldCarry = P & 0x01;

        value = (value >> 1) | (oldCarry << 7);
        write(addr, value);
        uint16_t sum = (uint16_t)A + value + oldCarry;

        if (sum > 0xFF) P |= 0x01; else P &= ~0x01;
        if ((~(A ^ value) & (A ^ sum)) & 0x80) P |= 0x40;
        else P &= ~0x40;

        A = sum & 0xFF;
        SetZN(A);
    };

    auto sax = [this](uint16_t addr) {
        write(addr, A & X);
    };

    auto lax = [this](uint8_t value) {
        A = value;
        X = value;
        SetZN(A);
    };

    auto dcp = [this](uint16_t addr) {
        uint8_t value = (read(addr) - 1) & 0xFF;
        write(addr, value);
        uint16_t result = (uint16_t)A - value;

        if (A >= value) P |= 0x01; else P &= ~0x01;

        SetZN(result & 0xFF);
    };

    auto isc = [this](uint16_t addr) {
        uint8_t value = (read(addr) + 1) & 0xFF;
        write(addr, value);
        uint16_t borrow = ((P & 0x01) ? 0 : 1);
        uint16_t result = (uint16_t)A - value - borrow;

        if (A >= value + borrow) P |= 0x01; else P &= ~0x01;
        if (((A ^ result) & 0x80) && ((A ^ value) & 0x80)) P |= 0x40; else P &= ~0x40;
        A = result & 0xFF;

        SetZN(A);
    };

    auto rla = [this](uint16_t addr) {
        uint8_t value = read(addr);

        bool oldCarry = P & 0x01;
        bool newCarry = value & 0x80;

        value = (value << 1) | (oldCarry ? 1 : 0);
        write(addr, value);
        P = newCarry ? (P | 0x01) : (P & ~0x01);

        A &= value;

        SetZN(A);
    };


    switch (opcode)
    {
    // lda
    case 0xA9: // LDA immediate
        lda(fetch(), 2);
        DEBUG_LOG("LDA imm 0x%x\n", A);
        break;
    case 0xA5: { // LDA zero-page
        uint8_t addr = fetch();
        lda_read(addr, 3);
        DEBUG_LOG("LDA zp 0x%02X\n", addr);
        break;
    }
    case 0xB1: { // LDA (Indirect),Y
        uint8_t zp = fetch();
        uint16_t base = read(zp) | (read((zp + 1) & 0xFF) << 8);
        lda_read(base, 5, true, Y);
        DEBUG_LOG("LDA Y ind 0x%x\n", zp);
        break;
    }
    case 0xB9: { // LDA absolute,Y
        uint16_t addr = fetch16();
        lda_read(addr, 4, true, Y);
        DEBUG_LOG("LDA Y abs 0x%x\n", addr);
        break;
    }
    case 0xAD: { // LDA absolute
        uint16_t addr = fetch16();
        lda_read(addr, 4);
        DEBUG_LOG("LDA abs 0x%04X\n", addr);
        break;
    }
    case 0xBD: { // LDA absolute,X
        uint16_t addr = fetch16();
        lda_read(addr, 4, true, X);
        DEBUG_LOG("LDA X abs 0x%x\n", addr);
        break;
    }
    case 0xB5: { // LDA zero-page,X
        uint8_t zp = fetch();
        lda_read((zp + X) & 0xFF, 4);
        DEBUG_LOG("LDA X zp 0x%02X\n", zp);
        break;
    }
    case 0xA1: { // LDA (Indirect,X)
        uint8_t zp = fetch();
        uint8_t ptr = (zp + X);
        uint16_t addr = read(ptr) | (read((ptr + 1) & 0xFF) << 8);
        lda_read(addr, 6);
        DEBUG_LOG("LDA X ind 0x%02X\n", zp);
        break;
    }

    case 0xA2: // LDX immediate
        ld_reg(X, fetch(), 2);
        DEBUG_LOG("LDX imm 0x%x\n", X);
        break;
    case 0xAE: { // LDX absolute
        uint16_t addr = fetch16();
        ld_reg_read(X, addr, 4);
        DEBUG_LOG("LDX abs 0x%04X\n", addr);
        break;
    }
    case 0xA6: { // LDX zero-page
        uint8_t addr = fetch();
        ld_reg_read(X, addr, 3);
        DEBUG_LOG("LDX zp 0x%02X\n", addr);
        break;
    }
    case 0xBE: { // LDX absolute,Y
        uint16_t addr = fetch16();
        ld_reg_read(X, addr, 4, true, Y);
        DEBUG_LOG("LDX Y abs 0x%04X\n", addr);
        break;
    }
    case 0xB6: { // LDX zero-page,Y
        uint8_t zp = fetch();
        ld_reg_read(X, (zp + Y) & 0xFF, 4);
        DEBUG_LOG("LDX Y zp 0x%02X\n", zp);
        break;
    }

    case 0xA0: // LDY immediate
        ld_reg(Y, fetch(), 2);
        DEBUG_LOG("LDY imm 0x%x\n", Y);
        break;
    case 0xAC: { // LDY absolute
        uint16_t addr = fetch16();
        ld_reg_read(Y, addr, 4);
        DEBUG_LOG("LDY abs 0x%04X\n", addr);
        break;
    }
    case 0xA4: { // LDY zero-page
        uint8_t addr = fetch();
        ld_reg_read(Y, addr, 3);
        DEBUG_LOG("LDY zp 0x%02X\n", addr);
        break;
    }
    case 0xB4: { // LDY zero-page,X
        uint8_t zp = fetch();
        ld_reg_read(Y, (zp + X) & 0xFF, 4);
        DEBUG_LOG("LDY X zp 0x%02X\n", zp);
        break;
    }
    case 0xBC: { // LDY absolute,X
        uint16_t addr = fetch16();
        ld_reg_read(Y, addr, 4, true, X);
        DEBUG_LOG("LDY X abs 0x%04X\n", addr);
        break;
    }

    //sta
    case 0x8D: { // STA absolute
        uint16_t addr = fetch16();
        st_reg(addr, A, 4);
        DEBUG_LOG("STA abs 0x%x\n", addr);
        break;
    }
    case 0x9D: { // STA absolute,X
        uint16_t addr = fetch16();
        st_reg(addr + X, A, 5);
        DEBUG_LOG("STA X abs 0x%04X\n", addr);
        break;
    }
    case 0x95: { // STA zero-page,X
        uint8_t zp = fetch();
        st_reg((zp + X) & 0xFF, A, 4);
        DEBUG_LOG("STA X zp 0x%02X\n", zp);
        break;
    }
    case 0x85: { // STA zero page
        uint8_t addr = fetch();
        st_reg(addr, A, 3);
        DEBUG_LOG("STA zp 0x%x\n", addr);
        break;
    }
    case 0x91: { // STA (Indirect),Y
        uint8_t zp = fetch();
        uint16_t base = read(zp) | (read((zp + 1) & 0xFF) << 8);
        st_reg(base + Y, A, 6);
        DEBUG_LOG("STA Y ind 0x%x\n", zp);
        break;
    }
    case 0x99: { // STA absolute,Y
        uint16_t addr = fetch16();
        st_reg(addr + Y, A, 5);
        DEBUG_LOG("STA Y abs 0x%x\n", addr);
        break;
    }
    case 0x81: { // STA (Indirect,X)
        uint8_t zp = fetch();
        uint8_t zp_indexed = (zp + X) & 0xFF;
        uint16_t addr = read(zp_indexed) | (read((zp_indexed + 1) & 0xFF) << 8);
        st_reg(addr, A, 6);
        DEBUG_LOG("STA X ind 0x%02X\n", zp);
        break;
    }

    case 0x86: { // STX zero page
        uint8_t addr = fetch();
        st_reg(addr, X, 3);
        DEBUG_LOG("STX zp 0x%x\n", addr);
        break;
    }
    case 0x8E: { // STX absolute
        uint16_t addr = fetch16();
        st_reg(addr, X, 4);
        DEBUG_LOG("STX abs 0x%x\n", addr);
        break;
    }

    case 0x8C: { // STY absolute
        uint16_t addr = fetch16();
        st_reg(addr, Y, 4);
        DEBUG_LOG("STY abs 0x%x\n", addr);
        break;
    }
    case 0x84: { // STY zero-page
        uint8_t addr = fetch();
        st_reg(addr, Y, 3);
        DEBUG_LOG("STY zp 0x%x\n", addr);
        break;
    }
    case 0x94: { // STY zero-page,X
        uint8_t zp = fetch();
        st_reg((zp + X) & 0xFF, Y, 4);
        DEBUG_LOG("STY X zp 0x%02X\n", zp);
        break;
    }

    //lsr, rol, asl
    case 0x4A: lsr(A); cycles += 2; DEBUG_LOG2("LSR acc"); break;
    case 0x4E: {                                                                          
        uint16_t addr = fetch16();
        lsr_mem(addr, 6);
        DEBUG_LOG("LSR abs 0x%04X\n", addr);
        break;
    }
    case 0x46: {                                                                          
        uint8_t addr = fetch();
        lsr_mem(addr, 5);
        DEBUG_LOG("LSR zp 0x%02X\n", addr);
        break;
    }

    case 0x2A: rol(A); cycles += 2; DEBUG_LOG2("ROL acc"); break;
    case 0x2E: {                                                                          
        uint16_t addr = fetch16();
        rol_mem(addr, 6);
        DEBUG_LOG("ROL abs 0x%04X\n", addr);
        break;
    }
    case 0x26: {                                                                          
        uint8_t addr = fetch();
        rol_mem(addr, 5);
        DEBUG_LOG("ROL zp 0x%02X\n", addr);
        break;
    }
    case 0x36: { // ROL zeropage,X
        uint8_t zp = fetch();
        uint8_t addr = (zp + X);
        rol_mem(addr, 6);
        DEBUG_LOG("ROL X zp 0x%02X\n", zp);
        break;
    }

    case 0x6E: { // ROR absolute
        uint16_t addr = fetch16();
        ror_mem(addr, 6);
        DEBUG_LOG("ROR abs 0x%04X\n", addr);
        break;
    }
    case 0x6A: ror(A); cycles += 2; DEBUG_LOG2("ROR A"); break;
    case 0x66: {                                                                          
        uint8_t addr = fetch();
        ror_mem(addr, 5);
        DEBUG_LOG("ROR zp 0x%02X\n", addr);
        break;
    }
    case 0x76: { // ROR zeropage,X
        uint8_t zp = fetch();
        uint8_t addr = (zp + X);
        ror_mem(addr, 6);
        DEBUG_LOG("ROR X zp 0x%02X\n", zp);
        break;
    }

    case 0x7E: {
        uint16_t addr = fetch16();
        ror_mem(addr + X, 6);
        if ((addr & 0xFF00) != ((addr + X) & 0xFF00)) cycles += 1;
        DEBUG_LOG("ROR X abs 0x%04X\n", addr);
        break;
    }

    case 0x0A: asl(A); cycles += 2; DEBUG_LOG2("ASL acc"); break;
    case 0x0E: {                                                                          
        uint16_t addr = fetch16();
        asl_mem(addr, 6);
        DEBUG_LOG("ASL abs 0x%04X\n", addr);
        break;
    }
    case 0x1E: { // ASL absolute,X
        uint16_t addr = fetch16();
        uint16_t effective = addr + X;
        asl_mem(effective, 7);
        if ((addr & 0xFF00) != (effective & 0xFF00)) cycles++;
        DEBUG_LOG("ASL X abs 0x%04X\n", addr);
        break;
    }

    case 0x06: {
        uint8_t addr = fetch();
        asl_mem(addr, 5);
        DEBUG_LOG("ASL zp 0x%02X\n", addr);
        break;
    }

    // transfer?
    case 0xAA:
        X = A;
        SetZN(X);
        cycles += 2;
        DEBUG_LOG2("TAX");
        break; // TAX
    case 0xA8:
        Y = A;
        SetZN(Y);
        cycles += 2;
        DEBUG_LOG2("TAY");
        break; // TAY
    case 0xBA:
        X = SP;
        SetZN(X);
        cycles += 2;
        DEBUG_LOG2("TSX");
        break; // TSX
    case 0x8A:
        A = X;
        SetZN(A);
        cycles += 2;
        DEBUG_LOG2("TXA");
        break; // TXA
    case 0x98:
        A = Y;
        SetZN(A);
        cycles += 2;
        DEBUG_LOG2("TYA");
        break; // TYA
    case 0x9A:
        SP = X;
        cycles += 2;
        DEBUG_LOG2("TXS");
        break; // TXS

    // increase/decrease
    case 0xEE: { // INC absolute
        uint16_t addr = fetch16();
        inc_reg(addr, 6);
        DEBUG_LOG("INC abs 0x%04X\n", addr);
        break;
    }
    case 0xE6: { // INC zp
        uint8_t addr = fetch();
        inc_reg(addr, 5);
        DEBUG_LOG("INC zp 0x%02X\n", addr);
        break;
    }
    case 0xF6: { // INC zp,X
        uint8_t zp = fetch();
        inc_reg((zp + X) & 0xFF, 6);
        DEBUG_LOG("INC X zp 0x%02X\n", zp);
        break;
    }
    case 0xFE: { // INC absolute,X
        uint16_t addr = fetch16();
        uint16_t effective = addr + X;
        inc_reg(effective, 7);
        DEBUG_LOG("INC X abs 0x%04X\n", addr);
        break;
    }

    case 0xCE: { // DEC absolute
        uint16_t addr = fetch16();
        dec_reg(addr, 6);
        DEBUG_LOG("DEC abs 0x%04X\n", addr);
        break;
    }
    case 0xDE: { // DEC abs,X
        uint16_t addr = fetch16();
        dec_reg(addr + X, 7);
        DEBUG_LOG("DEC X abs 0x%04X\n", addr);
        break;
    }
    case 0xC6: { // DEC zp
        uint8_t addr = fetch();
        dec_reg(addr, 5);
        DEBUG_LOG("DEC zp 0x%02X\n", addr);
        break;
    }
    case 0xD6: { // DEC zp,X
        uint8_t zp = fetch();
        dec_reg((zp + X) & 0xFF, 6);
        DEBUG_LOG("DEC X zp 0x%02X\n", zp);
        break;
    }

    case 0xE8:
        X++;
        SetZN(X);
        cycles += 2;
        DEBUG_LOG2("INX");
        break; // INX
    case 0xC8:
        Y++;
        SetZN(Y);
        cycles += 2;
        DEBUG_LOG2("INY");
        break; // INY
    case 0xCA:
        X--;
        SetZN(X);
        cycles += 2;
        DEBUG_LOG2("DEX");
        break; // DEX
    case 0x88:
        Y--;
        SetZN(Y);
        cycles += 2;
        DEBUG_LOG2("DEY");
        break; // DEY

    // jump
    case 0x4C:
        PC = fetch16();
        cycles += 3;
        DEBUG_LOG("JMP abs 0x%x\n", PC);
        break; // JMP abs
    case 0x6C:
        PC = readIndirect(fetch16());
        cycles += 5;
        DEBUG_LOG("JMP ind 0x%x\n", PC);
        break; // JMP ind
    case 0x20: { // JSR
        uint16_t addr = fetch16();
        uint16_t ret = PC - 1;
        push(ret >> 8);
        push(ret & 0xFF);
        PC = addr;
        cycles += 6;
        DEBUG_LOG("JSR abs 0x%04X\n", addr);
        break;
    }
    case 0x60: { // RTS
        uint8_t lo = pop();
        uint8_t hi = pop();
        PC = (hi << 8) | lo;
        PC += 1;
        cycles += 6;
        DEBUG_LOG("RTS -> PC=0x%04X, SP=0x%02X\n", PC, SP);
        break;
    }
    // branch
    case 0xF0: branch(P & 0x02, fetch()); cycles+=2; break; // BEQ
    case 0xD0: branch(!(P & 0x02), fetch()); cycles+=2; break; // BNE
    case 0x10: branch(!(P & 0x80), fetch()); cycles+=2; break; // BPL
    case 0x30: branch(P & 0x80, fetch()); cycles+=2; break; // BMI
    case 0xB0: branch(P & 0x01, fetch()); cycles+=2; break; // BCS
    case 0x90: branch(!(P & 0x01), fetch()); cycles+=2; break; // BCC
    case 0x50: branch(!(P & 0x40), fetch()); cycles+=2; break; // BVC
    case 0x70: branch((P & 0x40), fetch()); cycles+=2; break; // BVS

    // arithmatic
    case 0x69: { // ADC imm
        adc_op(fetch(), 2);
        DEBUG_LOG("ADC imm\n");
        break;
    }
    case 0x6D: { // ADC abs
        uint16_t addr = fetch16();
        adc_op(read(addr), 4);
        DEBUG_LOG("ADC abs 0x%04X\n", addr);
        break;
    }
    case 0x7D: { // ADC abs,X
        uint16_t addr = fetch16();
        uint16_t effective = addr + X;
        adc_op(read(effective), 4);
        if ((addr & 0xFF00) != (effective & 0xFF00)) cycles++;
        DEBUG_LOG("ADC X abs 0x%04X\n", addr);
        break;
    }
    case 0x79: { // ADC abs,Y
        uint16_t addr = fetch16();
        uint16_t effective = addr + Y;
        adc_op(read(effective), 4);
        if ((addr & 0xFF00) != (effective & 0xFF00)) cycles++;
        DEBUG_LOG("ADC Y abs 0x%04X\n", addr);
        break;
    }
    case 0x65: { // ADC zp
        adc_op(read(fetch()), 3);
        DEBUG_LOG("ADC zp\n");
        break;
    }
    case 0x75: { // ADC zp,X
        uint8_t zp = fetch();
        adc_op(read((zp + X) & 0xFF), 4);
        DEBUG_LOG("ADC X zp 0x%02X\n", zp);
        break;
    }
    case 0x71: { // ADC (ind),Y
        uint8_t zp = fetch();
        uint16_t base = read(zp) | (read((zp + 1) & 0xFF) << 8);
        uint16_t effective = base + Y;
        adc_op(read(effective), 5);
        if ((base & 0xFF00) != (effective & 0xFF00)) cycles++;
        DEBUG_LOG("ADC Y ind 0x%02X\n", zp);
        break;
    }

    case 0xE9: { // SBC imm
        uint8_t value = fetch();
        sbc_op(value, 2);
        DEBUG_LOG("SBC imm 0x%02X\n", value);
        break;
    }
    case 0xED: { // SBC abs
        uint16_t addr = fetch16();
        sbc_op(read(addr), 4);
        DEBUG_LOG("SBC abs 0x%04X\n", addr);
        break;
    }
    case 0xF9: { // SBC abs,Y
        uint16_t addr = fetch16();
        uint16_t effective = addr + Y;
        sbc_op(read(effective), 4);
        if ((addr & 0xFF00) != (effective & 0xFF00)) cycles++;
        DEBUG_LOG("SBC Y abs 0x%04X\n", addr);
        break;
    }
    case 0xE5: { // SBC zp
        sbc_op(read(fetch()), 3);
        DEBUG_LOG("SBC zp\n");
        break;
    }
    case 0xFD: { // SBC absolute,X
        uint16_t addr = fetch16();
        uint16_t effective = addr + X;
        sbc_op(read(effective), 4);
        if ((addr & 0xFF00) != (effective & 0xFF00)) cycles++;
        DEBUG_LOG("SBC X abs 0x%04X\n", addr);
        break;
    }
    case 0xF5: { // SBC zp,X
        uint8_t zp = fetch();
        sbc_op(read((zp + X) & 0xFF), 4);
        DEBUG_LOG("SBC X zp 0x%02X\n", zp);
        break;
    }
    case 0xF1: { // SBC (ind),Y
        uint8_t zp = fetch();
        uint16_t base = read16(zp);
        uint16_t effective = base + Y;
        sbc_op(read(effective), 5);
        if ((base & 0xFF00) != (effective & 0xFF00)) cycles++;
        DEBUG_LOG("SBC Y ind 0x%02X\n", zp);
        break;
    }


    //and or
    case 0x29: { // AND imm
        uint8_t addr = fetch();
        and_op(addr, 2);
        DEBUG_LOG("AND imm 0x%x\n", addr);
        break;
    }
    case 0x2D: { // AND abs
        uint16_t addr = fetch16();
        and_op(read(addr), 4);
        DEBUG_LOG("AND abs 0x%04X\n", addr);
        break;
    }
    case 0x25: { // AND zp
        uint8_t addr = fetch();
        and_op(read(addr), 3);
        DEBUG_LOG("AND zp 0x%x\n", addr);
        break;
    }
    case 0x35: { // AND zeropage,X
        uint8_t zp = fetch();
        uint8_t addr = (zp + X);
        uint8_t value = read(addr);
        and_op(value, 4); // 4 cycles
        DEBUG_LOG("AND X zp 0x%02X\n", zp);
        break;
    }

    case 0x3D: { // AND abs,X
        uint16_t addr = fetch16();
        uint16_t effective = addr + X;
        and_op(read(effective), 4);
        if ((addr & 0xFF00) != (effective & 0xFF00)) cycles++;
        DEBUG_LOG("AND X abs 0x%04X\n", addr);
        break;
    }
    case 0x39: { // AND abs,Y
        uint16_t addr = fetch16();
        uint16_t effective = addr + Y;
        and_op(read(effective), 4);
        if ((addr & 0xFF00) != (effective & 0xFF00)) cycles++;
        DEBUG_LOG("AND Y abs 0x%04X\n", addr);
        break;
    }
    case 0x31: { // AND (indirect),Y
        uint8_t zp = fetch();
        uint16_t base = read(zp) | (read((zp + 1) & 0xFF) << 8);
        uint16_t effective = base + Y;
        uint8_t value = read(effective);
        and_op(value, 5);
        if ((base & 0xFF00) != (effective & 0xFF00)) cycles++;
        DEBUG_LOG("AND Y ind 0x%02X\n", zp);
        break;
    }


    case 0x09: { // ORA imm
        uint8_t addr = fetch();
        ora_op(addr, 2);
        DEBUG_LOG("ORA imm 0x%x\n", addr);
        break;
    }
    case 0x0D: { // ORA abs
        uint16_t addr = fetch16();
        ora_op(read(addr), 4);
        DEBUG_LOG("ORA abs 0x%04X\n", addr);
        break;
    }
    case 0x1D: { // ORA abs,X
        uint16_t addr = fetch16();
        uint16_t effective = addr + X;
        ora_op(read(effective), 4);
        if ((addr & 0xFF00) != (effective & 0xFF00)) cycles++;
        DEBUG_LOG("ORA X abs 0x%04X\n", addr);
        break;
    }
    case 0x19: { // ORA abs,Y
        uint16_t addr = fetch16();
        uint16_t effective = addr + Y;
        ora_op(read(effective), 4);
        if ((addr & 0xFF00) != (effective & 0xFF00)) cycles++;
        DEBUG_LOG("ORA Y abs 0x%04X\n", addr);
        break;
    }
    case 0x05: { // ORA zp
        uint8_t addr = fetch();
        ora_op(read(addr), 3);
        DEBUG_LOG("ORA zp 0x%x\n", addr);
        break;
    }
    case 0x15: { // ORA zp,X
        uint8_t zp = fetch();
        uint8_t effective = (zp + X);
        ora_op(read(effective), 4);
        DEBUG_LOG("ORA X zp 0x%02X\n", zp);
        break;
    }
    case 0x11: { // ORA (indirect),Y
        uint8_t zp = fetch();
        uint16_t base = read(zp) | (read((zp + 1) & 0xFF) << 8);
        uint16_t effective = base + Y;
        uint8_t value = read(effective);
        ora_op(value, 5);
        if ((base & 0xFF00) != (effective & 0xFF00)) cycles++;
        DEBUG_LOG("ORA Y ind 0x%02X\n", zp);
        break;
    }

    case 0x49: { // EOR imm
        uint8_t value = fetch();
        A ^= value;
        SetZN(A);
        cycles += 2;
        DEBUG_LOG("EOR imm 0x%x\n", value);
        break;
    }
    case 0x4D: { // EOR absolute
        uint16_t addr = fetch16();
        uint8_t value = read(addr); 
        A ^= value;
        SetZN(A);
        cycles += 4; 
        DEBUG_LOG("EOR abs 0x%04X\n", addr);
        break;
    }
    case 0x5D: { // EOR absolute,X
        uint16_t addr = fetch16();
        uint16_t effective = addr + X;
        uint8_t value = read(effective);
        A ^= value;
        SetZN(A);
        cycles += 4;
        if ((addr & 0xFF00) != (effective & 0xFF00)) cycles++;
        DEBUG_LOG("EOR X abs 0x%04X\n", addr);
        break;
    }
    case 0x45: { // EOR zero-page
        uint8_t addr = fetch();
        A ^= read(addr);
        SetZN(A);
        cycles += 3;
        DEBUG_LOG("EOR zp 0x%x\n", addr);
        break;
    }
    case 0x55: { // EOR zeropage,X
        uint8_t zp = fetch();
        uint8_t addr = (zp + X) & 0xFF;
        uint8_t value = read(addr);
        A ^= value;
        SetZN(A);
        cycles += 4;
        DEBUG_LOG("EOR X zp 0x%02X\n", zp);
        break;
    }
    case 0x51: { // EOR (ind),Y
        uint8_t zp = fetch();
        uint16_t base = read16(zp);
        uint16_t effective = base + Y;
        uint8_t value = read(effective);
        A ^= value;
        SetZN(A);
        cycles += 5;
        if ((base & 0xFF00) != (effective & 0xFF00)) cycles++;
        DEBUG_LOG("EOR Y ind 0x%02X -> 0x%04X\n", zp, effective);
        break;
    }

    case 0x2C:
    { // BIT absolute
        uint16_t addr = fetch16();
        uint8_t value = read(addr);
        P = (P & 0x3D) | (value & 0xC0);
        P = (A & value) ? (P & ~0x02) : (P | 0x02);
        cycles += 4;
        DEBUG_LOG("BIT abs 0x%04X\n", addr);
        break;
    }
    case 0x24: { // BIT zero-page
        uint8_t addr = fetch();
        uint8_t value = read(addr);
        P = (P & 0x3D) | (value & 0xC0);
        P = (A & value) ? (P & ~0x02) : (P | 0x02);
        cycles += 3;
        DEBUG_LOG("BIT zp 0x%02X\n", addr);
        break;
    }

    case 0xC9:
    { // CMP imm
        uint8_t addr = fetch();
        cmp_reg(A, addr, 2);
        DEBUG_LOG("CMP imm 0x%02X\n", addr);
        break;
    }
    case 0xC5: 
    { // CMP zp
        uint8_t addr = fetch();
        cmp_reg(A, read(addr), 3);
        DEBUG_LOG("CMP zp 0x%02X\n", addr);
        break;
    }
    case 0xD5: // CMP zp,X
    {
        uint8_t addr = fetch();
        cmp_reg(A, read(addr + X), 4);
        DEBUG_LOG("CMP X zp 0x%02X\n", addr);
        break;
    }
    case 0xCD: // CMP absolute
    {
        uint16_t addr = fetch16();
        cmp_reg(A, read(addr), 4);
        DEBUG_LOG("CMP abs 0x%04X\n", addr);
        break;
    }
    case 0xDD: // CMP absolute,X
    {
        uint16_t addr = fetch16();
        uint16_t effective = addr + X;
        cmp_reg(A, read(effective), 4);
        if ((addr & 0xFF00) != (effective & 0xFF00)) cycles += 1;
        DEBUG_LOG("CMP X abs 0x%04X\n", addr);
        break;
    }
    case 0xD9: // CMP absolute,Y
    {
        uint16_t addr = fetch16();
        uint16_t effective = addr + Y;
        cmp_reg(A, read(effective), 4);
        if ((addr & 0xFF00) != (effective & 0xFF00)) cycles += 1;
        DEBUG_LOG("CMP Y abs 0x%04X\n", addr);
        break;
    }
    case 0xD1: { // CMP (ind),Y
        uint8_t zp = fetch();
        uint16_t base = read(zp) | (read((zp + 1) & 0xFF) << 8);
        uint16_t effective = base + Y;

        cmp_reg(A, read(effective), 5);
        if ((base & 0xFF00) != (effective & 0xFF00)) cycles += 1;

        DEBUG_LOG("CMP Y ind 0x%02X", zp);
        break;
    }

    // CPX
    case 0xE0: { // CPX immediate
        uint8_t value = fetch();
        cmp_reg(X, value, 2);
        DEBUG_LOG("CPX imm 0x%02X\n", value);
        break;
    }
    case 0xE4: { // CPX zero-page
        uint8_t addr = fetch();
        uint8_t value = read(addr);
        cmp_reg(X, value, 3);
        DEBUG_LOG("CPX zp 0x%02X\n", addr);
        break;
    }
    case 0xEC: { // CPX absolute
        uint16_t addr = fetch16();
        uint8_t value = read(addr);
        cmp_reg(X, value, 4);
        DEBUG_LOG("CPX abs 0x%04X\n", addr);
        break;
    }

    // CPY
    case 0xC0: { // CPY immediate
        uint8_t value = fetch();
        cmp_reg(Y, value, 2);
        DEBUG_LOG("CPY imm 0x%02X\n", value);
        break;
    }
    case 0xC4: { // CPY zero-page
        uint8_t addr = fetch();
        uint8_t value = read(addr);
        cmp_reg(Y, value, 3);
        DEBUG_LOG("CPY zp 0x%02X\n", addr);
        break;
    }
    case 0xCC: { // CPY absolute
        uint16_t addr = fetch16();
        uint8_t value = read(addr);
        cmp_reg(Y, value, 4);
        DEBUG_LOG("CPY abs 0x%04X\n", addr);
        break;
    }

    // stack
    case 0x48: // PHA
        push(A);
        cycles += 3;
        DEBUG_LOG2("PHA");
        break;

    case 0x68: // PLA
        A = pop();
        SetZN(A);
        cycles += 4;
        DEBUG_LOG2("PLA");
        break;

    case 0x08: // PHP
        push(P | 0x10);
        cycles += 3;
        DEBUG_LOG2("PHP");
        break;

    case 0x28: // PLP
        P = pop() | 0x20;
        cycles += 4;
        DEBUG_LOG2("PLP");
        break;

    // SEI
    case 0x78:
        P |= 0x04;
        cycles += 2;
        DEBUG_LOG2("SEI");
        break;
    // SEC
    case 0x38:
        P |= 0x01;
        cycles += 2;
        DEBUG_LOG2("SEC");
        break;
    case 0xF8: // SED
        P |= 0x08;
        DEBUG_LOG2("SED");
        cycles += 2;
        break;

    // CLD
    case 0xD8:
        P &= ~0x08;
        cycles += 2;
        DEBUG_LOG2("CLD");
        break;
    case 0x18: // CLC
        P &= ~0x01;
        cycles += 2;
        DEBUG_LOG2("CLC");
        break;
    case 0x40: { // RTI
        P = pop() | 0x20;
        P &= ~0x10;
        uint8_t lo = pop();
        uint8_t hi = pop();
        PC = (hi << 8) | lo;
        cycles += 6;
        DEBUG_LOG("RTI -> PC=0x%04X, SP=0x%02X\n", PC, SP);
        break;
    }
    
    case 0xEA: // NOP
        cycles += 2;
        DEBUG_LOG2("NOP");
        break;
    case 0xB8: // CLV
        P &= ~0x40;
        cycles += 2;
        DEBUG_LOG2("CLV");
        break;
    case 0x58: // CLI
        P &= ~0x04;
        cycles += 2;
        DEBUG_LOG2("CLI");
        break;

    // unoffical opcodes
    //slo
    case 0x07: { // SLO zeropage
        uint8_t addr = fetch();
        slo(addr);
        DEBUG_LOG("SLO zp 0x%02X\n", addr);
        cycles += 5;
        break;
    }
    case 0x17: { // SLO zeropage,X
        uint8_t addr = (fetch() + X) & 0xFF;
        slo(addr);
        DEBUG_LOG("SLO X zp 0x%02X\n", addr);
        cycles += 6;
        break;
    }
    case 0x0F: { // SLO absolute
        uint16_t addr = fetch16();
        slo(addr);
        DEBUG_LOG("SLO abs 0x%04X\n", addr);
        cycles += 6;
        break;
    }
    case 0x1F: { // SLO absolute,X
        uint16_t addr = fetch16();
        slo(addr + X);
        DEBUG_LOG("SLO X abs 0x%04X\n", addr);
        cycles += 7;
        break;
    }
    case 0x1B: { // SLO absolute,Y
        uint16_t addr = fetch16();
        slo(addr + Y);
        DEBUG_LOG("SLO Y abs 0x%04X\n", addr);
        cycles += 7;
        break;
    }
    case 0x03: { // SLO (Indirect,X)
        uint8_t zp = (fetch() + X) & 0xFF;
        uint16_t addr = read(zp) | (read((zp + 1) & 0xFF) << 8);
        slo(addr);
        DEBUG_LOG("SLO X ind 0x%02X\n", zp);
        cycles += 8;
        break;
    }
    case 0x13: { // SLO (Indirect),Y
        uint8_t zp = fetch();
        uint16_t base = read(zp) | (read((zp + 1) & 0xFF) << 8);
        slo(base + Y);
        DEBUG_LOG("SLO Y zp 0x%02X\n", zp);
        cycles += 8;
        break;
    }
    //rla
    case 0x27: { // RLA zeropage
        uint8_t addr = fetch();
        rla(addr);
        DEBUG_LOG("RLA zp 0x%02X\n", addr);
        cycles += 5;
        break;
    }
    case 0x37: { // RLA zeropage,X
        uint8_t addr = (fetch() + X) & 0xFF;
        rla(addr);
        DEBUG_LOG("RLA X zp 0x%02X\n", addr);
        cycles += 6;
        break;
    }
    case 0x2F: { // RLA absolute
        uint16_t addr = fetch16();
        rla(addr);
        DEBUG_LOG("RLA abs 0x%04X\n", addr);
        cycles += 6;
        break;
    }
    case 0x3F: { // RLA absolute,X
        uint16_t addr = fetch16();
        rla(addr + X);
        DEBUG_LOG("RLA X abs 0x%04X\n", addr);
        cycles += 7;
        break;
    }
    case 0x3B: { // RLA absolute,Y
        uint16_t addr = fetch16();
        rla(addr + Y);
        DEBUG_LOG("RLA Y abs 0x%04X\n", addr);
        cycles += 7;
        break;
    }
    case 0x23: { // RLA (Indirect,X)
        uint8_t zp = (fetch() + X) & 0xFF;
        uint16_t addr = read(zp) | (read((zp + 1) & 0xFF) << 8);
        rla(addr);
        DEBUG_LOG("RLA X ind 0x%02X\n", zp);
        cycles += 8;
        break;
    }
    case 0x33: { // RLA (Indirect),Y
        uint8_t zp = fetch();
        uint16_t base = read(zp) | (read((zp + 1) & 0xFF) << 8);
        rla(base + Y);
        DEBUG_LOG("RLA Y ind 0x%02X\n", zp);
        cycles += 8;
        break;
    }

    //rra
    case 0x67: { // RRA zeropage
        uint8_t addr = fetch();
        rra(addr);
        DEBUG_LOG("RRA zp 0x%02X\n", addr);
        cycles += 5;
        break;
    }
    case 0x77: { // RRA zeropage,X
        uint8_t addr = (fetch() + X) & 0xFF;
        rra(addr);
        DEBUG_LOG("RRA X zp 0x%02X\n", addr);
        cycles += 6;
        break;
    }
    case 0x6F: { // RRA absolute
        uint16_t addr = fetch16();
        rra(addr);
        DEBUG_LOG("RRA abs 0x%04X\n", addr);
        cycles += 6;
        break;
    }
    case 0x7F: { // RRA absolute,X
        uint16_t addr = fetch16();
        rra(addr + X);
        DEBUG_LOG("RRA X abs 0x%04X\n", addr);
        cycles += 7;
        break;
    }
    case 0x7B: { // RRA absolute,Y
        uint16_t addr = fetch16();
        rra(addr + Y);
        DEBUG_LOG("RRA Y abs 0x%04X\n", addr);
        cycles += 7;
        break;
    }
    case 0x63: { // RRA (Indirect,X)
        uint8_t zp = (fetch() + X) & 0xFF;
        uint16_t addr = read16(zp);
        rra(addr);
        DEBUG_LOG("RRA X ind 0x%02X\n", zp);
        cycles += 8;
        break;
    }
    case 0x73: { // RRA (Indirect),Y
        uint8_t zp = fetch();
        uint16_t base = read16(zp);
        rra(base + Y);
        DEBUG_LOG("RRA Y ind 0x%02X\n", zp);
        cycles += 8;
        break;
    }

    //sre
    case 0x47: { // SRE zeropage
        uint8_t addr = fetch();
        sre(addr);
        DEBUG_LOG("SRE zp 0x%02X\n", addr);
        cycles += 5;
        break;
    }
    case 0x57: { // SRE zeropage,X
        uint8_t addr = (fetch() + X) & 0xFF;
        sre(addr);
        DEBUG_LOG("SRE X zp 0x%02X\n", addr);
        cycles += 6;
        break;
    }
    case 0x4F: { // SRE absolute
        uint16_t addr = fetch16();
        sre(addr);
        DEBUG_LOG("SRE abs 0x%04X\n", addr);
        cycles += 6;
        break;
    }
    case 0x5F: { // SRE absolute,X
        uint16_t addr = fetch16();
        sre(addr + X);
        DEBUG_LOG("SRE X abs 0x%04X\n", addr);
        cycles += 7;
        break;
    }
    case 0x5B: { // SRE absolute,Y
        uint16_t addr = fetch16();
        sre(addr + Y);
        DEBUG_LOG("SRE Y abs 0x%04X\n", addr);
        cycles += 7;
        break;
    }
    case 0x43: { // SRE (Indirect,X)
        uint8_t zp = (fetch() + X) & 0xFF;
        uint16_t addr = read(zp) | (read((zp + 1) & 0xFF) << 8);
        sre(addr);
        DEBUG_LOG("SRE X ind 0x%02X\n", zp);
        cycles += 8;
        break;
    }
    case 0x53: { // SRE (Indirect),Y
        uint8_t zp = fetch();
        uint16_t base = read(zp) | (read((zp + 1) & 0xFF) << 8);
        sre(base + Y);
        DEBUG_LOG("SRE Y ind 0x%02X\n", zp);
        cycles += 8;
        break;
    }

    //sax
    case 0x83: { // SAX (Indirect,X)
        uint8_t zp = (fetch() + X) & 0xFF;
        uint16_t addr = read16(zp);
        sax(addr);
        cycles += 6;
        DEBUG_LOG("SAX X ind 0x%02X\n", zp);
        break;
    }
    case 0x87: { // SAX zero-page
        uint8_t zp = fetch();
        sax(zp);
        cycles += 3;
        DEBUG_LOG("SAX zp 0x%02X\n", zp);
        break;
    }
    case 0x8F: { // SAX absolute
        uint16_t addr = fetch16();
        sax(addr);
        cycles += 4;
        DEBUG_LOG("SAX abs 0x%04X\n", addr);
        break;
    }
    case 0x97: { // SAX zero-page,Y
        uint8_t zp = fetch();
        uint16_t addr = (zp + Y) & 0xFF;
        sax(addr);
        cycles += 4;
        DEBUG_LOG("SAX Y zp 0x%02X\n", zp);
        break;
    }

    //lax
    case 0xA7: { // LAX zero-page
        uint8_t zp = fetch();
        lax(read(zp));
        cycles += 3;
        DEBUG_LOG("LAX zp 0x%02X\n", zp);
        break;
    }
    case 0xB7: { // LAX zero-page,Y
        uint8_t zp = fetch();
        uint16_t addr = (zp + Y) & 0xFF;
        lax(read(addr));
        cycles += 4;
        DEBUG_LOG("LAX Y zp 0x%02X\n", zp);
        break;
    }
    case 0xAF: { // LAX absolute
        uint16_t addr = fetch16();
        lax(read(addr));
        cycles += 4;
        DEBUG_LOG("LAX abs 0x%04X\n", addr);
        break;
    }
    case 0xBF: { // LAX absolute,Y
        uint16_t addr = fetch16() + Y;
        lax(read(addr));
        cycles += 4;
        if ((addr & 0xFF00) != ((addr - Y) & 0xFF00)) cycles++;
        DEBUG_LOG("LAX Y abs 0x%04X\n", addr);
        break;
    }
    case 0xA3: { // LAX (Indirect,X)
        uint8_t zp = (fetch() + X) & 0xFF;
        uint16_t addr = read16(zp);
        lax(read(addr));
        cycles += 6;
        DEBUG_LOG("LAX X ind 0x%02X\n", zp);
        break;
    }
    case 0xB3: { // LAX (Indirect),Y
        uint8_t zp = fetch();
        uint16_t base = read16(zp);
        uint16_t addr = base + Y;
        lax(read(addr));
        cycles += 5;
        if ((addr & 0xFF00) != (base & 0xFF00)) cycles++;
        DEBUG_LOG("LAX Y ind 0x%02X\n", zp);
        break;
    }

    //sha, incomplete
    case 0x9F: { // SHA absolute,Y
        uint16_t addr = fetch16() + Y;
        cycles += 5;
        DEBUG_LOG("SHA Y abs 0x%04X\n", addr);
        break;
    }
    case 0x93: { // SHA (ind),Y
        uint8_t zp = fetch();
        cycles += 6;
        DEBUG_LOG("SHA Y ind 0x%02X\n", zp);
        break;
    }
    case 0x9B: { // SHS absolute,Y
        fetch16();
        cycles += 6;
        DEBUG_LOG("SHS Y abs 0x%02X\n", zp);
        break;
    }
    case 0x9C: { // SHY absolute,X
        fetch16();
        cycles += 6;
        DEBUG_LOG("SHY X abs 0x%02X\n", zp);
        break;
    }
    case 0x9E: { // SHX absolute,Y
        fetch16();
        cycles += 6;
        DEBUG_LOG("SHY Y abs 0x%02X\n", zp);
        break;
    }
    case 0xBB: { // LAE absolute,Y
        fetch16();
        cycles += 6;
        DEBUG_LOG("LAE Y abs 0x%02X\n", zp);
        break;
    }

    //dcp
    case 0xC7: { // DCP zero-page
        uint8_t zp = fetch();
        dcp(zp);
        cycles += 5;
        DEBUG_LOG("DCP zp 0x%02X\n", zp);
        break;
    }
    case 0xD7: { // DCP zero-page,X
        uint8_t zp = (fetch() + X);
        dcp(zp);
        cycles += 6;
        DEBUG_LOG("DCP X zp 0x%02X\n", zp);
        break;
    }
    case 0xCF: { // DCP absolute
        uint16_t addr = fetch16();
        dcp(addr);
        cycles += 6;
        DEBUG_LOG("DCP abs 0x%04X\n", addr);
        break;
    }
    case 0xDF: { // DCP absolute,X
        uint16_t addr = fetch16() + X;
        dcp(addr);
        cycles += 7;
        DEBUG_LOG("DCP X abs 0x%04X\n", addr);
        break;
    }
    case 0xDB: { // DCP absolute,Y
        uint16_t addr = fetch16() + Y;
        dcp(addr);
        cycles += 7;
        DEBUG_LOG("DCP Y abs 0x%04X\n", addr);
        break;
    }
    case 0xC3: { // DCP (Indirect,X)
        uint8_t zp = (fetch() + X);
        uint16_t addr = read16(zp);
        dcp(addr);
        cycles += 8;
        DEBUG_LOG("DCP X ind 0x%02X\n", zp);
        break;
    }
    case 0xD3: { // DCP (Indirect),Y
        uint8_t zp = fetch();
        uint16_t base = read16(zp);
        uint16_t addr = base + Y;
        dcp(addr);
        cycles += 8;
        DEBUG_LOG("DCP Y ind 0x%02X\n", zp);
        break;
    }

    //isc
    case 0xE7: { // ISC zero-page
        uint8_t zp = fetch();
        isc(zp);
        cycles += 5;
        DEBUG_LOG("ISC zp 0x%02X\n", zp);
        break;
    }
    case 0xF7: { // ISC zero-page,X
        uint8_t zp = (fetch() + X);
        isc(zp);
        cycles += 6;
        DEBUG_LOG("ISC X zp 0x%02X\n", zp);
        break;
    }
    case 0xEF: { // ISC absolute
        uint16_t addr = fetch16();
        isc(addr);
        cycles += 6;
        DEBUG_LOG("ISC abs 0x%04X\n", addr);
        break;
    }
    case 0xFF: { // ISC absolute,X
        uint16_t addr = fetch16() + X;
        isc(addr);
        cycles += 7;
        DEBUG_LOG("ISC X abs 0x%04X\n", addr);
        break;
    }
    case 0xFB: { // ISC absolute,Y
        uint16_t addr = fetch16() + Y;
        isc(addr);
        cycles += 7;
        DEBUG_LOG("ISC Y abs 0x%04X\n", addr);
        break;
    }
    case 0xE3: { // ISC (Indirect,X)
        uint8_t zp = (fetch() + X);
        uint16_t addr = read16(zp);
        isc(addr);
        cycles += 8;
        DEBUG_LOG("ISC X ind 0x%02X\n", zp);
        break;
    }
    case 0xF3: { // ISC (Indirect),Y
        uint8_t zp = fetch();
        uint16_t base = read16(zp);
        uint16_t addr = base + Y;
        isc(addr);
        cycles += 8;
        DEBUG_LOG("ISC Y ind 0x%02X\n", zp);
        break;
    }

    //anc, alr, arr, ...
    case 0x0B: case 0x2B: { // ANC imm
        uint8_t value = fetch();
        A &= value;
        SetZN(A);
        if (A & 0x80) P |= 0x01; else P &= ~0x01;
        cycles += 2;
        DEBUG_LOG("ANC imm 0x%02X\n", value);
        break;
    }
    case 0x4B: { // ALR imm
        uint8_t value = fetch();
        A &= value;
        uint8_t carryOut = A & 1;
        A >>= 1;
        if (carryOut) P |= 0x01; else P &= ~0x01;
        SetZN(A);
        cycles += 2;
        DEBUG_LOG("ALR imm 0x%02X\n", value);
        break;
    }
    case 0x6B: { // ARR imm
        uint8_t value = fetch();
        A &= value;
        A = (A >> 1) | ((P & 0x01) ? 0x80 : 0x00);
        SetZN(A);
        uint8_t bit5 = (A >> 5) & 1;
        uint8_t bit6 = (A >> 6) & 1;
        if (bit6) P |= 0x01; else P &= ~0x01;
        if (bit5 ^ bit6) P |= 0x40; else P &= ~0x40;

        cycles += 2;
        DEBUG_LOG("ARR imm 0x%02X\n", value);
        break;
    }
    case 0x8B: { // XAA imm
        uint8_t value = fetch();
        A = X & value;
        SetZN(A);
        cycles += 2;
        DEBUG_LOG("XAA imm 0x%02X\n", value);
        break;
    }
    case 0xAB: { // LXA imm
        uint8_t value = fetch();
        A = (A | 0xEE) & X & value;
        SetZN(A);
        cycles += 2;
        DEBUG_LOG("LXA imm 0x%02X\n", value);
        break;
    }
    case 0xCB: { // AXS imm
        uint8_t value = fetch();
        uint8_t result = (A & X) - value;
        if ((A & X) >= value) P |= 0x01; else P &= ~0x01;
        X = result;
        SetZN(X);
        cycles += 2;
        DEBUG_LOG("AXS imm 0x%02X\n", value);
        break;
    }

    case 0xEB: { // SBC imm
        uint8_t value = fetch();
        sbc_op(value, 2);
        DEBUG_LOG("SBC imm 0x%02X\n", value);
        break;
    }

    case 0x04: case 0x44: case 0x64: { // NOP zp
        fetch();
        cycles += 3;
        DEBUG_LOG2("NOP zp");
        break;
    }
    case 0x14: case 0x34: case 0x54: case 0x74: case 0xD4: case 0xF4: { // NOP zp,X
        fetch();
        cycles += 4;
        DEBUG_LOG2("NOP X zp");
        break;
    }
    case 0x0C: case 0x1C: case 0x3C: case 0x5C: case 0x7C: case 0xDC: case 0xFC: { // NOP abs
        fetch16();
        cycles += 4;
        if ((opcode & 0x1C) != 0x0C) cycles++;
        DEBUG_LOG2("NOP abs");
        break;
    }
    case 0x1A: case 0x3A: case 0x5A: case 0x7A: case 0xDA: case 0xFA: { // NOP implied
        cycles += 2;
        DEBUG_LOG2("NOP implied");
        break;
    }
    case 0x80: case 0x82: case 0x89: case 0xC2: case 0xE2: { // NOP imm
        fetch();
        cycles += 2;
        DEBUG_LOG2("NOP imm");
        break;
    }
    
    case 0x00: { // BRK
        PC++;
        push((PC >> 8) & 0xFF);
        push(PC & 0xFF);
        push(P | 0x30);
        P |= 0x04;
        PC = read16(0xFFFE);
        cycles += 7;
        break;
    }

    default:
        char errorMsg[64];
        sprintf(errorMsg, "Unimplemented Opcode: 0x%02X\n", opcode);
        romIsLoaded = false;
        reset();
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR , "Fatal error", errorMsg, NULL);
        break;
    }
    //  DEBUG_LOG("Proccessed 0x%x\n", opcode);
}

void CPU::SetZN(uint8_t value)
{
    P = (value == 0 ? P | 0x02 : P & ~0x02);
    P = (value & 0x80 ? P | 0x80 : P & ~0x80);
}

uint8_t CPU::read(uint16_t addr)
{
    if (addr < 0x2000) {
        return memory[addr & 0x07FF];
    }

    if (addr >= 0x2000 && addr < 0x4000) {
        switch (addr & 7) {
            case 2: { // PPUSTATUS
                uint8_t status = 0;
                status |= (ppu.Vblank ? 0x80 : 0);
                if (ppu.ScanLine < 240) status |= 0x40;

                ppu.Vblank = false;
                ppu.WriteLatch = false;
                return status;
            }

            case 4: // OAMDATA
                return ppu.OAM[ppu.OAMAddr];

            case 7: { // PPUDATA
                uint16_t vaddr = ppu.VRAMAddr & 0x3FFF;
                uint8_t ret;

                if (vaddr < 0x3F00) {
                    ret = ppu.ReadBuffer;
                    uint16_t nt = vaddr & 0x0FFF;
                    if (vaddr < 0x2000)
                        ppu.ReadBuffer = ppu.ChrROM[vaddr];
                    else {
                        if (globalROM.Header[6] & 1) nt &= 0x7FF;
                        else nt = (nt & 0x800) ? (nt - 0x400) : nt;
                        ppu.ReadBuffer = ppu.VRAM[nt];
                    }
                } else {
                    uint16_t pal = vaddr & 0x1F;
                    if ((pal & 0x13) == 0x10) pal &= ~0x10;
                    ret = ppu.paletteRAM[pal];
                }

                ppu.VRAMAddr += ppu.VRAMInc32Mode ? 32 : 1;
                ppu.VRAMAddr &= 0x3FFF;
                return ret;
            }

            default:
                return 0;
        }
    }


    switch (addr) {
        case 0x4015: // apu
            return 0;
        case 0x4016: {
            uint8_t ret = controllers[0].shift & 1;
            if (!controllers[0].strobe) {
                controllers[0].shift >>= 1;
            }
            return ret | 0x40;
        }
        case 0x4017: {
            uint8_t ret = controllers[1].shift & 1;
            if (!controllers[1].strobe) {
                controllers[1].shift >>= 1;
            }
            return ret | 0x40;
        }
    }

    return memory[addr];
}

void CPU::write(uint16_t addr, uint8_t value)
{
    if (addr < 0x2000) {
        memory[addr & 0x07FF] = value;
        return;
    }

    if (addr >= 0x2000 && addr < 0x4000) {
        switch (addr & 7) {
            case 0: // PPUCTRL
                ppu.nametableSelect      = value & 0x03;
                ppu.VRAMInc32Mode        = (value & 0x04) != 0;
                ppu.spritePatternTable   = (value & 0x08) != 0;
                ppu.BGPatternTable       = (value & 0x10) != 0;
                ppu.use8x16Sprites       = (value & 0x20) != 0;
                ppu.enableNMI            = (value & 0x80) != 0;

                ppu.TempVRAMAddr = (ppu.TempVRAMAddr & 0x73FF) | ((value & 0x03) << 10);
                break;

            case 1: // PPUMASK
                ppu.mask8pxMaskBG        = (value & 0x02) != 0;
                ppu.mask8pxMaskSprites   = (value & 0x04) != 0;
                ppu.maskRenderBG         = (value & 0x08) != 0;
                ppu.maskRenderSprites    = (value & 0x10) != 0;
                break;

            case 2: // PPUSTATUS
                break;

            case 3: // OAMADDR
                ppu.OAMAddr = value;
                break;

            case 4: // OAMDATA
                ppu.OAM[ppu.OAMAddr++] = value;
                break;

            case 5: // PPUSCROLL
               if (!ppu.WriteLatch) {
                    ppu.scrollX = value; // coarseX + fineX
                    ppu.scrollFineX = value & 7; // fine pixel inside tile
                } else {
                    ppu.scrollY = value;  // coarseY + fineY
                }
                ppu.WriteLatch = !ppu.WriteLatch;
                break;

            case 6: // PPUADDR
                if (!ppu.WriteLatch) {
                    ppu.TempVRAMAddr = (ppu.TempVRAMAddr & 0x00FF) | ((value & 0x3F) << 8);
                } else {
                    ppu.TempVRAMAddr = (ppu.TempVRAMAddr & 0x7F00) | value;
                    ppu.VRAMAddr = ppu.TempVRAMAddr;
                }
                ppu.WriteLatch = !ppu.WriteLatch;
                break;

            case 7: { // PPUDATA
                uint16_t vaddr = ppu.VRAMAddr & 0x3FFF;

                if (vaddr < 0x2000) {
                    if (globalROM.Header[5] == 0)
                        ppu.ChrROM[vaddr] = value;
                }
                else if (vaddr < 0x3F00) {
                    uint16_t nt = vaddr & 0x0FFF;
                    if (globalROM.Header[6] & 1) { // vertical mirroring
                        nt &= 0x7FF;
                    } else { // horizontal
                        nt = (nt & 0x800) ? (nt - 0x400) : nt;
                    }
                    ppu.VRAM[nt] = value;
                }
                else {
                    uint16_t pal = vaddr & 0x1F;
                    if ((pal & 0x13) == 0x10) pal &= ~0x10;
                    ppu.paletteRAM[pal] = value;
                }

                ppu.VRAMAddr += ppu.VRAMInc32Mode ? 32 : 1;
                ppu.VRAMAddr &= 0x3FFF;
                break;
            }
        }
        return;
    }

    if (addr < 0x4020) {
        switch (addr) {
            case 0x4014: {
                uint16_t base = value << 8;
                for (int i = 0; i < 256; i++)
                    ppu.OAM[i] = read(base + i);
                break;
            }
            case 0x4015: break; // apu status
            case 0x4016: {
                controllers[0].strobe = value & 1;
                if (controllers[0].strobe) {
                    controllers[0].shift = controllers[0].state;
                    controllers[1].shift = controllers[1].state;
                }
                break;
            }
            case 0x4017: break; // apu framecnt
        }
        return;
    }

    if (addr >= 0x6000 && addr < 0x8000) {
        memory[addr] = value;
        return;
    }
}

uint16_t CPU::read16(uint16_t addr)
{
    uint8_t lo = read(addr);
    uint8_t hi = read(addr + 1);
    return (hi << 8) | lo;
}

void CPU::push(uint8_t value) {
    memory[0x100 + SP] = value;
    SP--;
}

uint8_t CPU::pop() {
    SP++;
    return memory[0x100 + SP];
}