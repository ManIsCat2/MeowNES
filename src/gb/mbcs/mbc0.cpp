#include "mbc0.hpp"
#include "../gb_cpu.hpp"

MBC0::MBC0() {

}

void MBC0::reset() {
    mapCPUMemory(0x0000, 0x7FFF, getGBRom()->ROM, 0, false);
}

const char *MBC0::getName(void) {
    return "MBC0";
}