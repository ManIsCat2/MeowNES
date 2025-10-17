#pragma once

extern bool romIsLoaded;

class NesROM {
public:
    uint8_t header[8];
};

extern NesROM globalROM;