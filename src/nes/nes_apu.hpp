#pragma once

#include <cstdint>
#include "nes_bus.hpp"

class PulseChannel {
public:
    PulseChannel(bool isPulse1 = true) : isPulse1(isPulse1) {}

    void writeRegister(uint16_t addr, uint8_t data);
    void clockTimer();
    void clockQuarterFrame();
    void clockHalfFrame();
    double getSample() const;

    bool enable = false;
    uint8_t lengthCounter = 0;
private:
    bool isPulse1;
    uint8_t duty = 0;
    uint8_t dutySeq = 0;
    uint16_t timer = 0;
    uint16_t timerReload = 0;
    bool lengthHalt = false;
    
    bool constantVolume = false;
    uint8_t volume = 0;
    bool envStart = false;
    uint8_t envVol = 0;
    uint8_t envDivider = 0;
    
    bool sweepEnable = false;
    uint8_t sweepPeriod = 0;
    bool sweepNegate = false;
    uint8_t sweepShift = 0;
    bool sweepReload = false;
    uint8_t sweepDivider = 0;

    void clockEnvelope();
    void clockSweep();
    bool isMuted() const;
};

class TriangleChannel {
public:
    void writeRegister(uint16_t addr, uint8_t data);
    void clockTimer();
    void clockQuarterFrame();
    void clockHalfFrame();
    double getSample() const;

    bool enable = false;
    uint8_t lengthCounter = 0;

private:
    bool lengthHalt = false;
    uint8_t linearCounter = 0;
    uint8_t linearReload = 0;
    bool linearReloadFlag = false;
    uint16_t timer = 0;
    uint16_t timerReload = 0;
    uint8_t dutySeq = 0;
};

class NoiseChannel {
public:
    void writeRegister(uint16_t addr, uint8_t data, bool isNTSC);
    void clockTimer();
    void clockQuarterFrame();
    void clockHalfFrame();
    double getSample() const;

    bool enable = false;
    uint8_t lengthCounter = 0;

private:
    bool lengthHalt = false;
    bool constantVolume = false;
    uint8_t volume = 0;
    uint16_t timer = 0;
    uint16_t timerReload = 0;
    uint16_t shiftRegister = 1;
    bool mode = false;
    bool envStart = false;
    uint8_t envVol = 0;
    uint8_t envDivider = 0;

    void clockEnvelope();
};

class DMCChannel {
public:
    void writeRegister(uint16_t addr, uint8_t data, bool isNTSC);
    void clockTimer(NesCPU* cpu);
    double getSample() const;

    bool enable = false;
    uint16_t currentLength = 0;
    bool DMCIrqPending = false;
    bool DMCIrqEnable = false;

    void setEnabled(bool state); 
private:
    bool loop = false;
    uint16_t timer = 0;
    uint16_t timerReload = 428;
    uint16_t currentAddress = 0;
    uint16_t sampleAddress = 0xC000;
    uint16_t sampleLength = 1;
    uint8_t shiftRegister = 0;
    uint8_t bitsRemaining = 8;
    uint8_t sampleBuffer = 0;
    bool sampleBufferEmpty = true;
    uint8_t outputLevel = 0;
    bool silence = true;
};

class NesAPU : public HasNESBus {
public:
    NesAPU();
    ~NesAPU();

    void write(uint16_t addr, uint8_t data);
    uint8_t read(uint16_t addr);
    
    void step();
    void reset();

    bool IRQPending = false;

    float pulse1Volume = 50.0f;
    float pulse2Volume = 50.0f;
    float triangleVolume = 50.0f;
    float noiseVolume = 50.0f;
    float dmcVolume = 50.0f;
    float expVolume = 50.0f;
    float masterVolume = 50.0f;
    
    PulseChannel pulse1{true};
    PulseChannel pulse2{false};
    TriangleChannel triangle;
    NoiseChannel noise;
    DMCChannel dmc;
    double getOutputSample();
private:
    uint32_t clockCounter = 0;
    uint32_t frameCounter = 0;
    uint8_t frameMode = 0;
    bool IRQInhibit = false;
    int frameCounterResetDelay = 0;
    uint8_t delayedFrameMode = 0;

    void clockQuarterFrame();
    void clockHalfFrame();
};

extern NesAPU nesApu;