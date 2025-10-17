#define PFD_USING_IMPLEMENTATION
#include "portable-file-dialogs.h"

#include <SDL2/SDL.h>
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_sdl2.h"
#include "imgui/backends/imgui_impl_sdlrenderer2.h"

#include "main.hpp"

NesROM globalROM;

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

    if (!ppu.InitSDL(renderer)) return 1;

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
        if (globalROM.LoadNES(argv[1], mem)) {
            cpu.LoadMem(mem);
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
                        if (globalROM.LoadNES(romPath, mem)) {
                            cpu.LoadMem(mem);
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
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("CPU")) {
                if (ImGui::MenuItem("Pause")) {
                    cpu.CPUPaused = true;
                }
                if (ImGui::MenuItem("Continue")) {
                    cpu.CPUPaused = false;
                }
                if (ImGui::MenuItem("Reset")) {
                    cpu.reset();
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

        SDL_SetRenderDrawColor(renderer, 0x20, 0x20, 0x20, 0xff);
        SDL_RenderClear(renderer);

        if (romIsLoaded) {
            UpdateControllers();
            ppu.Render(renderer);
            cpu.run(89342);
        }

        ImGui::Render();
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    ppu.ShutdownSDL();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
