#define PFD_USING_IMPLEMENTATION
#include "portable-file-dialogs.h"

#include <cstdint>
#include <iostream>
#include <array>
#include <string>
#include <fstream>
#include <vector>
#include <cstring>
#include <filesystem>

#include <SDL2/SDL.h>
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_sdl2.h"
#include "imgui/backends/imgui_impl_sdlrenderer2.h"

#include "nes_cpu.h"
#include "nes_controller.h"

NesROM globalROM;

bool loadNES(const std::string& filename, std::array<uint8_t, MEMORY_SIZE>& mem) {
    std::ifstream rom(filename, std::ios::binary | std::ios::ate);
    if (!rom) {
        std::cerr << "Failed to open ROM: " << filename << "\n";
        return false;
    }

    std::streamsize fsize = rom.tellg();
    if (fsize < 16) {
        std::cerr << "ROM too small\n";
        return false;
    }
    rom.seekg(0, std::ios::beg);

    std::vector<uint8_t> data((size_t)fsize);
    if (!rom.read(reinterpret_cast<char*>(data.data()), fsize)) {
        std::cerr << "Failed to read ROM\n";
        return false;
    }

    // header
    if (data.size() < 16 || data[0] != 'N' || data[1] != 'E' || data[2] != 'S' || data[3] != 0x1A) {
        std::cerr << "Invalid iNES header\n";
        return false;
    }

    std::memcpy(globalROM.header, data.data(), 8);

    uint8_t prgPages = data[4];
    uint8_t chrPages = data[5];
    uint8_t flags6 = data[6];
    uint8_t flags7 = data[7];

    bool hasTrainer = (flags6 & 0x04) != 0;
    bool verticalMirroring = (flags6 & 0x01) != 0;
    bool fourScreen = (flags6 & 0x08) != 0;

    uint8_t mapper = (flags7 & 0xF0) | (flags6 >> 4);
    if (mapper != 0) {
        std::cerr << "Warning: mapper " << int(mapper) << " detected. only mapper 0 (NROM) is supported by MeowNES.\n";
    }

    size_t offset = 16;
    if (hasTrainer) {
        if (data.size() < offset + 512) {
            std::cerr << "ROM too small\n";
            return false;
        }
        offset += 512;
    }

    size_t totalPrgSize = size_t(prgPages) * 16 * 1024;

    if (prgPages == 0) {
        std::cerr << "ROM has zero PRG pages.\n";
        return false;
    } else if (prgPages == 1) {
        std::memcpy(&mem[0x8000], &data[offset], 0x4000);
        std::memcpy(&mem[0xC000], &data[offset], 0x4000);
    } else {
        std::memcpy(&mem[0x8000], &data[offset], 0x4000);
        std::memcpy(&mem[0xC000], &data[offset + 0x4000], 0x4000);
    }
    offset += totalPrgSize;

    size_t totalChrSize = size_t(chrPages) * 8 * 1024;
    if (chrPages == 0) {
        uint8_t zeros[0x2000] = {};
        ppu.loadCHR(zeros, 0x2000);
    } else {
        ppu.loadCHR(&data[offset], totalChrSize);
    }
    offset += totalChrSize;

    std::cerr << "Loaded ROM:\nPRG pages = " << int(prgPages) << "\n"
              << "CHR pages = " << int(chrPages) << "\n"
              << "CHR size = " << int(totalChrSize) << "\n"
              << "mapper = " << int(mapper) << "\n\n";
    return true;
}

bool romIsLoaded = false;

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        std::cerr << "SDL init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("MeowNES",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        800, 600, SDL_WINDOW_SHOWN);

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    SDL_Surface* icon = IMG_Load("gui/ico.png");
    if (!icon) {
        std::cout << "Failed to set window icon\n";
    } else {
       SDL_SetWindowIcon(window, icon);
       SDL_FreeSurface(icon);
    }

    if (!ppu.initSDL(renderer)) return 1;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    std::array<uint8_t, MEMORY_SIZE> mem{};
    bool running = true;
    std::string romPath;
    SDL_Event event;

    if (argc > 1) {
        if (loadNES(argv[1], mem)) {
            cpu.loadMemory(mem);
            cpu.reset();
            romIsLoaded = true;
        }
    }

    while (running) {
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) running = false;
        }
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        if (ImGui::BeginMainMenuBar()) {
            
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Open ROM")) {
                    auto selection = pfd::open_file(
                        "Select NES ROM",
                        "",
                        { "NES ROMs", "*.nes" },
                        pfd::opt::none
                    ).result();

                    if (!selection.empty()) {
                        romPath = selection.front();
                        if (loadNES(romPath, mem)) {
                            cpu.loadMemory(mem);
                            cpu.reset();
                            romIsLoaded = true;
                        } else {
                            std::cerr << "Failed to load ROM: " << romPath << "\n";
                        }
                    }
                }

                if (romIsLoaded) {
                    if (ImGui::MenuItem("Close ROM")) {
                        romIsLoaded = false;
                        cpu.reset();
                    }

                     if (ImGui::MenuItem("Reset")) {
                        cpu.reset();
                    }
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Settings")) {
                if (ImGui::BeginMenu("Graphics")) {
                    static bool fullscreen = false;
                    if (ImGui::Checkbox("Fullscreen", &fullscreen)) {
                        if (fullscreen) SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
                        else SDL_SetWindowFullscreen(window, 0);
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Misc")) {
                if (ImGui::MenuItem("Exit")) {
                    running = false;
                }
                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        SDL_SetRenderDrawColor(renderer, 0x20, 0x20, 0x20, 255);
        SDL_RenderClear(renderer);

        if (romIsLoaded) {
            update_controllers();
            ppu.renderPPU(renderer);
            cpu.run(89342);
        }

        ImGui::Render();
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    ppu.shutdownSDL();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
