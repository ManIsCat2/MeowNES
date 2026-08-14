#include "nes_apu.hpp"
#include "nes_cpu.hpp"
#include "../audio.hpp"

NesAPU nesApu;

const uint8_t DutyTable[4][8] = {
    {0, 1, 0, 0, 0, 0, 0, 0}, 
    {0, 1, 1, 0, 0, 0, 0, 0}, 
    {0, 1, 1, 1, 1, 0, 0, 0}, 
    {1, 0, 0, 1, 1, 1, 1, 1}  
};
    
const uint8_t LengthTable[32] = {
    10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10, 14, 12, 26, 14,
    12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
};
    
const uint8_t TriTable[32] = {
    15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
};
    
const uint16_t NoiseTimerTableNTSC[16] = {
    4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068
};
    
const uint16_t NoiseTimerTablePAL[16] = {
    4, 8, 14, 30, 60, 88, 118, 148, 188, 236, 354, 472, 708, 944, 1890, 3778
};
    
const uint16_t DMCRateTableNTSC[16] = {
    428, 380, 340, 320, 286, 254, 226, 214, 190, 160, 142, 128, 106, 84, 72, 54
};
    
const uint16_t DMCRateTablePAL[16] = {
    398, 354, 316, 298, 276, 236, 210, 198, 176, 148, 132, 118, 98, 78, 66, 50
};

void PulseChannel::writeRegister(uint16_t addr, uint8_t data) {
    uint8_t offset = addr & 0x0003;
    switch (offset) {
        case 0:
            duty = (data & 0xC0) >> 6;
            lengthHalt = (data & 0x20) != 0;
            constantVolume = (data & 0x10) != 0;
            volume = (data & 0x0F);
            break;
        case 1:
            sweepEnable = (data & 0x80) != 0;
            sweepPeriod = (data >> 4) & 0x07;
            sweepNegate = (data & 0x08) != 0;
            sweepShift  = data & 0x07;
            sweepReload = true;
            break;
        case 2:
            timerReload = (timerReload & 0xFF00) | data;
            break;
        case 3:
            timerReload = (timerReload & 0x00FF) | ((data & 0x07) << 8);
            timer = timerReload + 1;
            if (enable) lengthCounter = LengthTable[(data & 0xF8) >> 3];
            dutySeq = 0;
            envStart = true;
            break;
    }
}

void PulseChannel::clockTimer() {
    if (timer > 0) {
        timer--;
    } else {
        timer = timerReload;
        dutySeq = (dutySeq + 1) & 7;
    }
}

void PulseChannel::clockQuarterFrame() {
    clockEnvelope();
}

void PulseChannel::clockHalfFrame() {
    if (lengthCounter > 0 && !lengthHalt) lengthCounter--;
    clockSweep();
}

void PulseChannel::clockEnvelope() {
    if (envStart) {
        envStart = false;
        envVol = 15;
        envDivider = volume;
    } else {
        if (envDivider > 0) {
            envDivider--;
        } else {
            envDivider = volume;
            if (envVol > 0) envVol--;
            else if (lengthHalt) envVol = 15;
        }
    }
}

void PulseChannel::clockSweep() {
    if (sweepReload) {
        sweepReload = false;
        sweepDivider = sweepPeriod == 0 ? 8 : sweepPeriod;
        return;
    }

    if (sweepDivider > 0) {
        sweepDivider--;
        return;
    }

    sweepDivider = sweepPeriod == 0 ? 8 : sweepPeriod;

    if (!sweepEnable || sweepShift == 0 || timerReload < 8) return;

    uint16_t change = timerReload >> sweepShift;
    uint16_t target = timerReload;

    if (sweepNegate) {
        target -= change;
        if (isPulse1) target--;
    } else {
        target += change;
    }

    if (target <= 0x7FF) {
        timerReload = target;
    }
}

bool PulseChannel::isMuted() const {
    if (timerReload < 8) return true;
    if (!sweepEnable || sweepShift == 0) return false;

    uint16_t change = timerReload >> sweepShift;
    uint16_t target = timerReload;

    if (sweepNegate) {
        target -= change;
        if (isPulse1) target--;
    } else {
        target += change;
    }

    return target > 0x7FF;
}

double PulseChannel::getSample() const {
    if (!enable || lengthCounter == 0 || isMuted()) return 0.0;
    return DutyTable[duty][dutySeq] ? (constantVolume ? volume : envVol) : 0.0;
}


void TriangleChannel::writeRegister(uint16_t addr, uint8_t data) {
    uint8_t offset = addr & 0x0003;
    switch (offset) {
        case 0:
            lengthHalt = (data & 0x80) != 0;
            linearReload = data & 0x7F;
            break;
        case 2:
            timerReload = (timerReload & 0xFF00) | data;
            break;
        case 3:
            timerReload = (timerReload & 0x00FF) | ((data & 0x07) << 8);
            timer = timerReload + 1;
            if (enable) lengthCounter = LengthTable[(data & 0xF8) >> 3];
            linearReloadFlag = true;
            break;
    }
}

void TriangleChannel::clockTimer() {
    if (timer > 0) {
        timer--;
    } else {
        timer = timerReload;
        if (linearCounter > 0 && lengthCounter > 0 && timerReload >= 2) {
            dutySeq = (dutySeq + 1) % 32;
        }
    }
}

void TriangleChannel::clockQuarterFrame() {
    if (linearReloadFlag) {
        linearCounter = linearReload;
    } else if (linearCounter > 0) {
        linearCounter--;
    }
    if (!lengthHalt) {
        linearReloadFlag = false;
    }
}

void TriangleChannel::clockHalfFrame() {
    if (lengthCounter > 0 && !lengthHalt) {
        lengthCounter--;
    }
}

double TriangleChannel::getSample() const {
    if (!enable || lengthCounter == 0 || linearCounter == 0) return 0.0;
    return TriTable[dutySeq];
}


void NoiseChannel::writeRegister(uint16_t addr, uint8_t data, bool isNTSC) {
    uint8_t offset = addr & 0x0003;
    switch (offset) {
        case 0:
            lengthHalt = (data & 0x20) != 0;
            constantVolume = (data & 0x10) != 0;
            volume = (data & 0x0F);
            break;
        case 2:
            mode = (data & 0x80) != 0;
            timerReload = isNTSC ? NoiseTimerTableNTSC[data & 0x0F] : NoiseTimerTablePAL[data & 0x0F];
            break;
        case 3:
            if (enable) lengthCounter = LengthTable[(data & 0xF8) >> 3];
            envStart = true;
            break;
    }
}

void NoiseChannel::clockTimer() {
    if (timer > 0) {
        timer--;
    } else {
        timer = timerReload;
        uint16_t tap = mode ? 6 : 1;
        uint16_t feedback = (shiftRegister & 1) ^ ((shiftRegister >> tap) & 1);
        shiftRegister >>= 1;
        shiftRegister |= (feedback << 14);
    }
}

void NoiseChannel::clockQuarterFrame() {
    clockEnvelope();
}

void NoiseChannel::clockHalfFrame() {
    if (lengthCounter > 0 && !lengthHalt) {
        lengthCounter--;
    }
}

void NoiseChannel::clockEnvelope() {
    if (envStart) {
        envStart = false;
        envVol = 15;
        envDivider = volume;
    } else {
        if (envDivider > 0) {
            envDivider--;
        } else {
            envDivider = volume;
            if (envVol > 0) envVol--;
            else if (lengthHalt) envVol = 15;
        }
    }
}

double NoiseChannel::getSample() const {
    if (!enable || lengthCounter == 0 || (shiftRegister & 0x0001) != 0) return 0.0;
    return constantVolume ? volume : envVol;
}


void DMCChannel::writeRegister(uint16_t addr, uint8_t data, bool isNTSC) {
    uint8_t offset = addr & 0x0003;
    switch (offset) {
        case 0:
            DMCIrqEnable = (data & 0x80) != 0;
            loop = (data & 0x40) != 0;
            timerReload = isNTSC ? DMCRateTableNTSC[data & 0x0F] : DMCRateTablePAL[data & 0x0F];
            if (!DMCIrqEnable) DMCIrqPending = false;
            break;
        case 1:
            outputLevel = data & 0x7F;
            break;
        case 2:
            sampleAddress = 0xC000 + (data * 64);
            break;
        case 3:
            sampleLength = (data * 16) + 1;
            break;
    }
}

void DMCChannel::setEnabled(bool state) {
    enable = state;
    if (!enable) {
        currentLength = 0;
    } else {
        if (currentLength == 0) {
            currentAddress = sampleAddress;
            currentLength = sampleLength;
            sampleBufferEmpty = true;
            bitsRemaining = 8;
        }
    }
}

void DMCChannel::clockTimer(NesCPU* cpu) {
    if (sampleBufferEmpty && currentLength > 0) {
        sampleBuffer = cpu->read(currentAddress);
        sampleBufferEmpty = false;

        currentAddress++;
        if (currentAddress == 0) currentAddress = 0x8000;

        currentLength--;
        if (currentLength == 0) {
            if (loop) {
                currentAddress = sampleAddress;
                currentLength = sampleLength;
            } else if (DMCIrqEnable) {
                DMCIrqPending = true;
            }
        }
    }

    if (timer > 0) {
        timer--;
        return;
    }

    timer = timerReload;

    if (!silence) {
        if (shiftRegister & 1) {
            if (outputLevel <= 125) outputLevel += 2;
        } else {
            if (outputLevel >= 2) outputLevel -= 2;
        }
    }

    shiftRegister >>= 1;
    bitsRemaining--;

    if (bitsRemaining == 0) {
        bitsRemaining = 8;

        if (sampleBufferEmpty) {
            silence = true;
        } else {
            silence = false;
            shiftRegister = sampleBuffer;
            sampleBufferEmpty = true;
        }
    }
}

double DMCChannel::getSample() const {
    if (!enable) return 0.0;
    return outputLevel;
}


NesAPU::NesAPU() {}
NesAPU::~NesAPU() {}

void NesAPU::reset() {
    connectBus(&nesCpu, nullptr, nullptr);
    pulse1 = PulseChannel(true); 
    pulse2 = PulseChannel(false);
    triangle = TriangleChannel();
    noise = NoiseChannel();
    dmc = DMCChannel();
    
    clockCounter = 0;
    frameCounter = 0;
    frameMode = 0;
    IRQInhibit = false;
    IRQPending = false;
    dmc.DMCIrqPending = false;
    dmc.DMCIrqEnable = false;
    frameCounterResetDelay = 0;
    delayedFrameMode = 0;
}

void NesAPU::write(uint16_t addr, uint8_t data) {
    bool isNTSC = (getRom()->Region == ConsoleRegion::NTSC);

    if (addr >= 0x4000 && addr <= 0x4003) {
        pulse1.writeRegister(addr, data);
    } else if (addr >= 0x4004 && addr <= 0x4007) {
        pulse2.writeRegister(addr, data);
    } else if (addr >= 0x4008 && addr <= 0x400B) {
        triangle.writeRegister(addr, data);
    } else if (addr >= 0x400C && addr <= 0x400F) {
        noise.writeRegister(addr, data, isNTSC);
    } else if (addr >= 0x4010 && addr <= 0x4013) {
        dmc.writeRegister(addr, data, isNTSC);
    } else if (addr == 0x4015) {
        pulse1.enable = (data & 0x01) != 0;
        if (!pulse1.enable) pulse1.lengthCounter = 0;

        pulse2.enable = (data & 0x02) != 0;
        if (!pulse2.enable) pulse2.lengthCounter = 0;

        triangle.enable = (data & 0x04) != 0;
        if (!triangle.enable) triangle.lengthCounter = 0;

        noise.enable = (data & 0x08) != 0;
        if (!noise.enable) noise.lengthCounter = 0;

        dmc.setEnabled((data & 0x10) != 0);
        dmc.DMCIrqPending = false;
    } else if (addr == 0x4017) {
        delayedFrameMode = (data & 0x80) >> 7;
        IRQInhibit = (data & 0x40) >> 6;
        IRQPending = false;
        frameCounterResetDelay = (clockCounter & 1) ? 4 : 3;
    }
}

uint8_t NesAPU::read(uint16_t addr) {
    uint8_t data = 0;
    if (addr == 0x4015) {
        data = (cpu->dataBus & 0x20);
        if (pulse1.lengthCounter > 0) data |= 0x01;
        if (pulse2.lengthCounter > 0) data |= 0x02;
        if (triangle.lengthCounter > 0) data |= 0x04;
        if (noise.lengthCounter > 0) data |= 0x08;
        if (dmc.currentLength > 0) data |= 0x10;
        if (IRQPending) data |= 0x40;
        if (dmc.DMCIrqPending) data |= 0x80;
        
        IRQPending = false;
    }
    return data;
}

void NesAPU::clockQuarterFrame() {
    pulse1.clockQuarterFrame();
    pulse2.clockQuarterFrame();
    triangle.clockQuarterFrame();
    noise.clockQuarterFrame();
}

void NesAPU::clockHalfFrame() {
    pulse1.clockHalfFrame();
    pulse2.clockHalfFrame();
    triangle.clockHalfFrame();
    noise.clockHalfFrame();
}

void NesAPU::step() {
    if (frameCounterResetDelay > 0) {
        frameCounterResetDelay--;
        if (frameCounterResetDelay == 0) {
            frameCounter = 0;
            frameMode = delayedFrameMode;
            if (frameMode == 1) { 
                clockQuarterFrame();
                clockHalfFrame();
            }
        }
    }

    triangle.clockTimer();

    if (clockCounter % 2 == 0) {
        pulse1.clockTimer();
        pulse2.clockTimer();
        noise.clockTimer();
    }
    
    dmc.clockTimer(cpu);

    frameCounter++;
    if (getRom()->Region == ConsoleRegion::NTSC) {
        if (frameMode == 0) {
            if (frameCounter == 7457)  { clockQuarterFrame(); }
            if (frameCounter == 14913) { clockQuarterFrame(); clockHalfFrame(); }
            if (frameCounter == 22371) { clockQuarterFrame(); }

            if (frameCounter == 29828 && !IRQInhibit) IRQPending = true;

            if (frameCounter == 29829) {
                if (!IRQInhibit) IRQPending = true;
                clockQuarterFrame();
                clockHalfFrame();
            }

            if (frameCounter == 29830) {
                if (!IRQInhibit) IRQPending = true;
                frameCounter = 0;
            }
        } else { 
            if (frameCounter == 7457)  { clockQuarterFrame(); }
            if (frameCounter == 14913) { clockQuarterFrame(); clockHalfFrame(); }
            if (frameCounter == 22371) { clockQuarterFrame(); }
            if (frameCounter == 37281) { clockQuarterFrame(); clockHalfFrame(); }
            if (frameCounter == 37282) { frameCounter = 0; }
        }
    } else {
        if (frameMode == 0) {
            if (frameCounter == 8313)  { clockQuarterFrame(); }
            if (frameCounter == 16627) { clockQuarterFrame(); clockHalfFrame(); }
            if (frameCounter == 24939) { clockQuarterFrame(); }

            if (frameCounter == 33252 && !IRQInhibit) IRQPending = true;

            if (frameCounter == 33253) {
                if (!IRQInhibit) IRQPending = true;
                clockQuarterFrame();
                clockHalfFrame();
            }

            if (frameCounter == 33254) {
                if (!IRQInhibit) IRQPending = true;
                frameCounter = 0;
            }
        } else {
            if (frameCounter == 8313)  { clockQuarterFrame(); }
            if (frameCounter == 16627) { clockQuarterFrame(); clockHalfFrame(); }
            if (frameCounter == 24939) { clockQuarterFrame(); }
            if (frameCounter == 41565) { clockQuarterFrame(); clockHalfFrame(); }
            if (frameCounter == 41566) { frameCounter = 0; }
        }
    }

    clockCounter++;
    audioSystem.advance();
}

double NesAPU::getOutputSample() {
    double p1 = pulse1.getSample() * (pulse1Volume / 50.0);
    double p2 = pulse2.getSample() * (pulse2Volume / 50.0);
    double t = triangle.getSample() * (triangleVolume / 50.0);
    double n = noise.getSample() * (noiseVolume / 50.0);
    double d = dmc.getSample() * (dmcVolume / 50.0);

    double pulseOut = 0.0;
    double pulseSum = p1 + p2;
    if (pulseSum > 0.0) {
        pulseOut = 95.52 / ((8128.0 / pulseSum) + 100.0);
    }

    double tndIndex = (3.0 * t) + (2.0 * n) + d;
    double tndOut = 0.0;
    if (tndIndex > 0.0) {
        tndOut = 163.67 / ((24329.0 / tndIndex) + 100.0);
    }

    return (pulseOut + tndOut) * (masterVolume / 50.0);
}