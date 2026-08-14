#pragma once

#include <cstdint>

struct GbPulseChannel {
    bool enabled = false;

    uint8_t sweepPace = 0;
    bool sweepDirection = false;
    uint8_t sweepStep = 0;
    uint16_t sweepTimer = 0;
    uint16_t sweepShadowFreq = 0;
    bool sweepEnabled = false;

    uint8_t duty = 0;
    uint16_t lengthCounter = 0;
    bool lengthEnable = false;

    uint8_t initialVolume = 0;
    uint8_t currentVolume = 0;
    bool envDirection = false;
    uint8_t envPace = 0;
    uint8_t envTimer = 0;

    uint16_t frequency = 0;
    uint16_t timer = 0;
    uint8_t dutySequenceIdx = 0;

    void clockLength();
    void clockEnvelope();
    void clockSweep();
    void trigger(bool isP1);
    double getSample() const;
};

struct GbWaveChannel {
    bool enabled = false;
    bool dacEnable = false;

    uint16_t lengthCounter = 0;
    bool lengthEnable = false;

    uint8_t volumeShift = 0;
    uint16_t frequency = 0;
    uint16_t timer = 0;
    uint8_t wavePos = 0;
    uint8_t waveRAM[16] = {0};

    void clockLength();
    void trigger();
    double getSample() const;
};

struct GbNoiseChannel {
    bool enabled = false;

    uint16_t lengthCounter = 0;
    bool lengthEnable = false;

    uint8_t initialVolume = 0;
    uint8_t currentVolume = 0;
    bool envDirection = false;
    uint8_t envPace = 0;
    uint8_t envTimer = 0;

    uint8_t clockShift = 0;
    bool lfsrWidth = false;
    uint8_t clockDivider = 0;
    uint16_t timer = 0;
    uint16_t lfsr = 0x7FFF;

    void clockLength();
    void clockEnvelope();
    void trigger();
    double getSample() const;
};

class GbAPU {
public:
    GbAPU();
    ~GbAPU();

    void reset();
    void step(uint32_t cycles);
    
    uint8_t read(uint16_t addr);
    void write(uint16_t addr, uint8_t data);

    double getOutputSample();

    float pulse1Volume = 50.0f;
    float pulse2Volume = 50.0f;
    float waveVolume = 50.0f;
    float noiseVolume = 50.0f;
    float masterVolume = 50.0f;

    GbPulseChannel pulse1;
    GbPulseChannel pulse2;
    GbWaveChannel  wave;
    GbNoiseChannel noise;
private:
    bool masterPower = true;

    uint8_t nr50 = 0;
    uint8_t nr51 = 0;
    uint8_t nr52 = 0;

    uint16_t frameSequencerCounter = 0;
    uint8_t frameSequencerStep = 0;

    void clockFrameSequencer();
};

extern GbAPU gbApu;