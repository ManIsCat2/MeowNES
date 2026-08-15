#include "namco163.hpp"
#include "../nes_cpu.hpp"
#include "../nes_ppu.hpp"

N163::N163() {

}

void N163::reset() {
    variant = NAMCO_163;
    writeProtect = 0;
    irqCounter = 0;

    n163Addr = 0;
    n163AutoInc = false;
    audioCycleCount = 0;
    currentChannel = 0;
    currentAudioSample = 0.0;
    for (int i = 0; i < 128; i++) {
        n163Ram[i] = 0;
    }
    for (int i = 0; i < 8; i++) {
        channelOut[i] = 0.0;
    }

    setPRGBank(3, -1);
    updateWorkRam();
}

const char *N163::getName(void) {
    //if (variant == Variant::NAMCO_175) return "Namco175";
    //if (variant == Variant::NAMCO_340) return "Namco340";
    return "N163";
}

void N163::updateWorkRam() {
    uint8_t *memory = getNESRom()->hasBattery ? SRAM : PRGRam;
    if (variant == Variant::NAMCO_163) {
		bool WriteEnable = (writeProtect & 0x40) == 0x40;
		mapCPUMemory(0x6000, 0x67FF, memory, 0, WriteEnable && (writeProtect & 0x01) == 0x00);
		mapCPUMemory(0x6800, 0x6FFF, memory, 0, WriteEnable && (writeProtect & 0x02) == 0x00);
		mapCPUMemory(0x7000, 0x77FF, memory, 0, WriteEnable && (writeProtect & 0x04) == 0x00);
		mapCPUMemory(0x7800, 0x7FFF, memory, 0, WriteEnable && (writeProtect & 0x08) == 0x00);
	} else if (variant == Variant::NAMCO_175) {
		mapCPUMemory(0x6000, 0x7FFF, memory, 0, (writeProtect & 0x01) == 0x01);
	} else {
        unmapCPUMemory(0x6000, 0x7FFF);
    }
}

void N163::clockCPU(void) {
    if (irqCounter & 0x8000) {
        if ((irqCounter & 0x7FFF) != 0x7FFF) {
            irqCounter++;

            if ((irqCounter & 0x7FFF) == 0x7FFF) {
                cpu->setExternalIRQ(true);
            }
        }
    }

    audioCycleCount++;
    if (audioCycleCount >= 15) {
        audioCycleCount = 0;
        
        uint8_t numChannels = ((n163Ram[0x7F] & 0x70) >> 4) + 1;
        
        uint8_t ch = 7 - currentChannel;
        uint8_t base = ch * 8 + 0x40;
        
        uint32_t freq = n163Ram[base + 0] | (n163Ram[base + 2] << 8) | ((n163Ram[base + 4] & 0x03) << 16);
        uint32_t phase = n163Ram[base + 1] | (n163Ram[base + 3] << 8) | (n163Ram[base + 5] << 16);
        
        uint32_t length = 256 - (n163Ram[base + 4] & 0xFC);
        
        uint32_t waveAddr = n163Ram[base + 6];
        uint8_t volume = n163Ram[base + 7] & 0x0F;
        
        phase = (phase + freq) % (length << 16);
        
        n163Ram[base + 1] = phase & 0xFF;
        n163Ram[base + 3] = (phase >> 8) & 0xFF;
        n163Ram[base + 5] = (phase >> 16) & 0xFF;
        
        uint32_t sampleIndex = phase >> 16;
        uint32_t absoluteSampleIndex = (waveAddr + sampleIndex) & 0xFF;
        
        uint8_t waveByte = n163Ram[absoluteSampleIndex / 2];
        
        int8_t sample = (absoluteSampleIndex & 1) ? (waveByte >> 4) : (waveByte & 0x0F);
        
        sample -= 8;
        
        channelOut[ch] = (sample * volume); 
        
        currentChannel++;
        if (currentChannel >= numChannels) {
            currentChannel = 0;
        }

        double mixSum = 0.0;
        for (int i = 8 - numChannels; i < 8; i++) {
            mixSum += channelOut[i];
        }
        
        currentAudioSample = (mixSum / numChannels) / 150.0;
    }
}

uint8_t N163::cpuRead(uint16_t addr) {
    if ((addr & 0xF800) == 0x4800) {
        uint8_t data = n163Ram[n163Addr];
        if (n163AutoInc) n163Addr = (n163Addr + 1) & 0x7F;
        return data;
    }
    return MapperBase::cpuRead(addr);
}

void N163::cpuWrite(uint16_t addr, uint8_t value) {
    switch (addr & 0xF800) {
        case 0x4800:
            n163Ram[n163Addr] = value;
            if (n163AutoInc) n163Addr = (n163Addr + 1) & 0x7F;
            break;

        case 0x5000:
            irqCounter = (irqCounter & 0xFF00) | value;
            cpu->setExternalIRQ(false);
            break;

        case 0x5800:
            irqCounter = (irqCounter & 0x00FF) | (value << 8);
            cpu->setExternalIRQ(false);
            break;
        case 0x8000:
        case 0x8800:
        case 0x9000:
        case 0x9800: {
            uint8_t bank = (addr - 0x8000) >> 11;
            if (value >= 0xE0) {
                setCHRBank(bank, value & 1);
            } else {
                setCHRBank(bank, value);
            }
            break;
        }

        case 0xA000:
        case 0xA800:
        case 0xB000:
        case 0xB800: {
            uint8_t bank = ((addr - 0xA000) >> 11) + 4;
            if (value >= 0xE0) {
                setCHRBank(bank, value & 1);
            } else {
                setCHRBank(bank, value);
            }
            break;
        }
        case 0xC000:
        case 0xC800:
        case 0xD000:
        case 0xD800: {
            if (variant == NAMCO_175) {
                writeProtect = value;
                updateWorkRam();
            } else {
                uint8_t bank = ((addr - 0xC000) >> 11) + 8;

                if (value >= 0xE0)
                    setCHRBank(bank, value & 1);
                else
                    setCHRBank(bank, value);
            }
            break;
        }
        case 0xE000:
            setPRGBank(0, value & 0x3F);
            if (variant == NAMCO_340) {
                switch((value >> 6) & 3) {
                    case 0: ppu->Mirroring = MirrorMode::SCREEN_A; break;
                    case 1: ppu->Mirroring = MirrorMode::VERTICAL; break;
                    case 2: ppu->Mirroring = MirrorMode::HORIZONTAL; break;
                    case 3: ppu->Mirroring = MirrorMode::SCREEN_B; break;
                }
            }
            break;

        case 0xE800:
            setPRGBank(1, value & 0x3F);
            break;

        case 0xF000:
            setPRGBank(2, value & 0x3F);
            break;

        case 0xF800:
            writeProtect = value;
            updateWorkRam();
            n163Addr = value & 0x7F;
            n163AutoInc = (value & 0x80) != 0;
            break;
    }
    MapperBase::cpuWrite(addr, value);
}

double N163::getExpansionAudioSample() {
    return currentAudioSample;
}