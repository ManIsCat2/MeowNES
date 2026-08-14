#include "gb_apu.hpp"
#include "../audio.hpp"

GbAPU gbApu;

static const uint8_t DutyPatterns[4][8] = {
    {0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 1, 1, 1},
    {0, 1, 1, 1, 1, 1, 1, 0}
};

static const uint16_t NoiseDivisors[8] = { 8, 16, 32, 48, 64, 80, 96, 112 };

GbAPU::GbAPU() {
}
GbAPU::~GbAPU() {}

void GbAPU::reset() {
    masterPower = true;
    nr50 = 0x77;
    nr51 = 0xF3;
    nr52 = 0xF1;
    frameSequencerCounter = 0;
    frameSequencerStep = 0;

    pulse1 = GbPulseChannel();
    pulse2 = GbPulseChannel();
    wave = GbWaveChannel();
    noise = GbNoiseChannel();
}

void GbAPU::step(uint32_t cycles) {
    if (!masterPower) return;

    frameSequencerCounter += cycles;
    if (frameSequencerCounter >= 8192) {
        frameSequencerCounter -= 8192;
        clockFrameSequencer();
    }

    int remaining1 = cycles;
    while (remaining1 >= pulse1.timer) {
        if (pulse1.timer <= 0) pulse1.timer = 4;
        remaining1 -= pulse1.timer;
        pulse1.timer = (2048 - pulse1.frequency) * 4;
        pulse1.dutySequenceIdx = (pulse1.dutySequenceIdx + 1) & 7;
    }
    pulse1.timer -= remaining1;

    int remaining2 = cycles;
    while (remaining2 >= pulse2.timer) {
        if (pulse2.timer <= 0) pulse2.timer = 4;
        remaining2 -= pulse2.timer;
        pulse2.timer = (2048 - pulse2.frequency) * 4;
        pulse2.dutySequenceIdx = (pulse2.dutySequenceIdx + 1) & 7;
    }
    pulse2.timer -= remaining2;

    int remaining3 = cycles;
    while (remaining3 >= wave.timer) {
        if (wave.timer <= 0) wave.timer = 2;
        remaining3 -= wave.timer;
        wave.timer = (2048 - wave.frequency) * 2;
        wave.wavePos = (wave.wavePos + 1) & 31;
    }
    wave.timer -= remaining3;

    int remaining4 = cycles;
    while (remaining4 >= noise.timer) {
        if (noise.timer <= 0) noise.timer = 8;
        remaining4 -= noise.timer;
        noise.timer = (NoiseDivisors[noise.clockDivider] << noise.clockShift);
    }
    noise.timer -= remaining4;

    audioSystem.advance(cycles);
}

void GbAPU::clockFrameSequencer() {
    if (frameSequencerStep % 2 == 0) {
        pulse1.clockLength();
        pulse2.clockLength();
        wave.clockLength();
        noise.clockLength();
    }

    if (frameSequencerStep == 2 || frameSequencerStep == 6) {
        pulse1.clockSweep();
    }

    if (frameSequencerStep == 7) {
        pulse1.clockEnvelope();
        pulse2.clockEnvelope();
        noise.clockEnvelope();
    }

    frameSequencerStep = (frameSequencerStep + 1) & 7;
}

void GbPulseChannel::clockLength() {
    if (lengthEnable && lengthCounter > 0) {
        lengthCounter--;
        if (lengthCounter == 0) enabled = false;
    }
}

void GbPulseChannel::clockEnvelope() {
    if (envPace == 0) return;
    if (envTimer > 0) {
        envTimer--;
    } else {
        envTimer = envPace;
        if (envDirection && currentVolume < 15) currentVolume++;
        else if (!envDirection && currentVolume > 0) currentVolume--;
    }
}

void GbPulseChannel::clockSweep() {
    if (!sweepEnabled || sweepPace == 0) return;
    if (sweepTimer > 0) {
        sweepTimer--;
    } else {
        sweepTimer = sweepPace;
        uint16_t delta = sweepShadowFreq >> sweepStep;
        uint16_t newFreq = sweepDirection ? (sweepShadowFreq - delta) : (sweepShadowFreq + delta);

        if (newFreq <= 2047 && sweepStep > 0) {
            sweepShadowFreq = newFreq;
            frequency = newFreq;
        } else if (newFreq > 2047) {
            enabled = false;
        }
    }
}

void GbPulseChannel::trigger(bool isP1) {
    enabled = true;
    if (lengthCounter == 0) lengthCounter = 64;
    timer = (2048 - frequency) * 4;
    envTimer = envPace;
    currentVolume = initialVolume;

    if (isP1) {
        sweepShadowFreq = frequency;
        sweepTimer = sweepPace;
        sweepEnabled = (sweepPace > 0 || sweepStep > 0);
    }
}

double GbPulseChannel::getSample() const {
    if (!enabled || currentVolume == 0) return 0.0;
    uint8_t bit = DutyPatterns[duty][dutySequenceIdx];
    return bit ? (currentVolume / 15.0) : 0.0;
}

void GbWaveChannel::clockLength() {
    if (lengthEnable && lengthCounter > 0) {
        lengthCounter--;
        if (lengthCounter == 0) enabled = false;
    }
}

void GbWaveChannel::trigger() {
    enabled = dacEnable;
    if (lengthCounter == 0) lengthCounter = 256;
    timer = (2048 - frequency) * 2;
    wavePos = 0;
}

double GbWaveChannel::getSample() const {
    if (!enabled || !dacEnable || volumeShift == 0) return 0.0;
    uint8_t byte = waveRAM[wavePos / 2];
    uint8_t nibble = (wavePos % 2 == 0) ? (byte >> 4) : (byte & 0x0F);
    
    nibble >>= (volumeShift - 1);
    return (nibble / 15.0);
}

void GbNoiseChannel::clockLength() {
    if (lengthEnable && lengthCounter > 0) {
        lengthCounter--;
        if (lengthCounter == 0) enabled = false;
    }
}

void GbNoiseChannel::clockEnvelope() {
    if (envPace == 0) return;
    if (envTimer > 0) {
        envTimer--;
    } else {
        envTimer = envPace;
        if (envDirection && currentVolume < 15) currentVolume++;
        else if (!envDirection && currentVolume > 0) currentVolume--;
    }
}

void GbNoiseChannel::trigger() {
    enabled = true;
    if (lengthCounter == 0) lengthCounter = 64;
    timer = NoiseDivisors[clockDivider] << clockShift;
    lfsr = 0x7FFF;
    envTimer = envPace;
    currentVolume = initialVolume;
}

double GbNoiseChannel::getSample() const {
    if (!enabled || currentVolume == 0) return 0.0;
    return (~lfsr & 1) ? (currentVolume / 15.0) : 0.0;
}

uint8_t GbAPU::read(uint16_t addr) {
    if (addr >= 0xFF30 && addr <= 0xFF3F) {
        return wave.waveRAM[addr - 0xFF30];
    }

    switch (addr) {
        case 0xFF10: return (pulse1.sweepPace << 4) | (pulse1.sweepDirection << 3) | pulse1.sweepStep | 0x80;
        case 0xFF11: return (pulse1.duty << 6) | 0x3F;
        case 0xFF12: return (pulse1.initialVolume << 4) | (pulse1.envDirection << 3) | pulse1.envPace;
        case 0xFF14: return (pulse1.lengthEnable << 6) | 0xBF;
        
        case 0xFF16: return (pulse2.duty << 6) | 0x3F;
        case 0xFF17: return (pulse2.initialVolume << 4) | (pulse2.envDirection << 3) | pulse2.envPace;
        case 0xFF19: return (pulse2.lengthEnable << 6) | 0xBF;

        case 0xFF1A: return (wave.dacEnable << 7) | 0x7F;
        case 0xFF1C: return (wave.volumeShift << 5) | 0x9F;
        case 0xFF1E: return (wave.lengthEnable << 6) | 0xBF;

        case 0xFF21: return (noise.initialVolume << 4) | (noise.envDirection << 3) | noise.envPace;
        case 0xFF22: return (noise.clockShift << 4) | (noise.lfsrWidth << 3) | noise.clockDivider;
        case 0xFF23: return (noise.lengthEnable << 6) | 0xBF;

        case 0xFF24: return nr50;
        case 0xFF25: return nr51;
        case 0xFF26: {
            uint8_t val = (masterPower << 7) | 0x70;
            if (pulse1.enabled) val |= 0x01;
            if (pulse2.enabled) val |= 0x02;
            if (wave.enabled) val |= 0x04;
            if (noise.enabled) val |= 0x08;
            return val;
        }
        default: return 0xFF;
    }
}

void GbAPU::write(uint16_t addr, uint8_t data) {
    if (!masterPower && addr != 0xFF26 && !(addr >= 0xFF30 && addr <= 0xFF3F)) return;

    if (addr >= 0xFF30 && addr <= 0xFF3F) {
        wave.waveRAM[addr - 0xFF30] = data;
        return;
    }

    switch (addr) {
        case 0xFF10:
            pulse1.sweepPace = (data >> 4) & 0x07;
            pulse1.sweepDirection = (data & 0x08) != 0;
            pulse1.sweepStep = data & 0x07;
            break;
        case 0xFF11:
            pulse1.duty = (data >> 6) & 0x03;
            pulse1.lengthCounter = 64 - (data & 0x3F);
            break;
        case 0xFF12:
            pulse1.initialVolume = (data >> 4) & 0x0F;
            pulse1.envDirection = (data & 0x08) != 0;
            pulse1.envPace = data & 0x07;
            if ((data & 0xF8) == 0) pulse1.enabled = false;
            break;
        case 0xFF13:
            pulse1.frequency = (pulse1.frequency & 0x0700) | data;
            break;
        case 0xFF14:
            pulse1.frequency = (pulse1.frequency & 0x00FF) | ((data & 0x07) << 8);
            pulse1.lengthEnable = (data & 0x40) != 0;
            if (data & 0x80) pulse1.trigger(true);
            break;

        case 0xFF16:
            pulse2.duty = (data >> 6) & 0x03;
            pulse2.lengthCounter = 64 - (data & 0x3F);
            break;
        case 0xFF17:
            pulse2.initialVolume = (data >> 4) & 0x0F;
            pulse2.envDirection = (data & 0x08) != 0;
            pulse2.envPace = data & 0x07;
            if ((data & 0xF8) == 0) pulse2.enabled = false;
            break;
        case 0xFF18:
            pulse2.frequency = (pulse2.frequency & 0x0700) | data;
            break;
        case 0xFF19:
            pulse2.frequency = (pulse2.frequency & 0x00FF) | ((data & 0x07) << 8);
            pulse2.lengthEnable = (data & 0x40) != 0;
            if (data & 0x80) pulse2.trigger(false);
            break;

        case 0xFF1A:
            wave.dacEnable = (data & 0x80) != 0;
            if (!wave.dacEnable) wave.enabled = false;
            break;
        case 0xFF1B:
            wave.lengthCounter = 256 - data;
            break;
        case 0xFF1C:
            wave.volumeShift = (data >> 5) & 0x03;
            break;
        case 0xFF1D:
            wave.frequency = (wave.frequency & 0x0700) | data;
            break;
        case 0xFF1E:
            wave.frequency = (wave.frequency & 0x00FF) | ((data & 0x07) << 8);
            wave.lengthEnable = (data & 0x40) != 0;
            if (data & 0x80) wave.trigger();
            break;

        case 0xFF20:
            noise.lengthCounter = 64 - (data & 0x3F);
            break;
        case 0xFF21:
            noise.initialVolume = (data >> 4) & 0x0F;
            noise.envDirection = (data & 0x08) != 0;
            noise.envPace = data & 0x07;
            if ((data & 0xF8) == 0) noise.enabled = false;
            break;
        case 0xFF22:
            noise.clockShift = (data >> 4) & 0x0F;
            noise.lfsrWidth = (data & 0x08) != 0;
            noise.clockDivider = data & 0x07;
            break;
        case 0xFF23:
            noise.lengthEnable = (data & 0x40) != 0;
            if (data & 0x80) noise.trigger();
            break;

        case 0xFF24:
            nr50 = data;
            break;
        case 0xFF25:
            nr51 = data;
            break;
        case 0xFF26: {
            bool newPower = (data & 0x80) != 0;
            if (!newPower && masterPower) {
                for (uint16_t r = 0xFF10; r <= 0xFF25; ++r) write(r, 0x00);
            }
            masterPower = newPower;
            break;
        }
    }
}

double GbAPU::getOutputSample() {
    if (!masterPower) return 0.0;

    double s1 = pulse1.getSample() * (pulse1Volume / 50.0);
    double s2 = pulse2.getSample() * (pulse2Volume / 50.0);
    double s3 = wave.getSample() * (waveVolume / 50.0);
    double s4 = noise.getSample() * (noiseVolume / 50.0);
    double mixed = (s1 + s2 + s3 + s4) / 4.0;

    return mixed * (masterVolume / 50.0);
}