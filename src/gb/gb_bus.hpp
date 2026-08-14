#pragma once

class GbCPU;
class GbPPU;
class GbAPU;

class HasGBBus {
public:
    GbCPU *cpu = nullptr;
    GbPPU *ppu = nullptr;
    GbAPU *apu = nullptr;
    void connectBus(GbCPU *sysCPU, GbPPU *sysPPU, GbAPU *sysAPU) {
        cpu = sysCPU;
        ppu = sysPPU;
        apu = sysAPU;
    }
};
