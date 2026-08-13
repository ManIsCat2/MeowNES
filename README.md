# EMeowlator

EMeowlator is a multi-system emulator made by a Cat.

## NES

Can play most of the NES library.

Supported mappers:
- NROM (Mapper 0)
- MMC1 (Mapper 1)
- UxROM (Mapper 2)
- CNROM (Mapper 3)
- MMC3 (Mapper 4)
- MMC5 (Mapper 5) (WIP)
- MMC2 (Mapper 9)
- Namco163 / Namco129 (Mapper 19)
- Nina001 (Mapper 34, SubMapper 1)
- BNROM (Mapper 34, SubMapper 2)
- X in 1 (Mapper 62)
- SunSoft FME-7 (Mapper 69)
- SL12 (?) (Mapper 116)
- J.Y. Company ASIC (Mapper 90, 209 and 211) (WIP)

## Game Boy
The whole GB support right now is kinda bad, so everything is WIP.

**Credits:**
- [SameBoy](https://github.com/LIJI32/SameBoy) For the BootROM used in DMG and CGB.

# Building

## Step 1: Install Dependencies

### Ubuntu / Debian
```bash
sudo apt install qt6-base-dev libsdl2-dev build-essential clang
```

### Arch Linux
```bash
sudo pacman -S qt6-base sdl2 base-devel clang
```

### Fedora
```bash
sudo dnf install qt6-qtbase-devel SDL2-devel make clang
```

### Windows (MinGW)
```bash
pacman -S mingw-w64-x86_64-SDL2 mingw-w64-x86_64-qt6 mingw-w64-x86_64-toolchain mingw-w64-x86_64-ntldd mingw-w64-x86_64-pkg-config
```

## Step 2: Clone the Repository
```bash
git clone https://github.com/ManIsCat2/EMeowlator
```

## Step 3: Compile
```bash
cd path/to/EMeowlator
make -j$(nproc)
```

The built executable will be located in the `build/` folder inside EMeowlator.