#include "nes_controller.h"

Controller controllers[2];

void update_controllers(void) {
    const uint8_t* keystate = SDL_GetKeyboardState(nullptr);

    for (unsigned char i=0;i<2;i++) {
        Controller *controller = &controllers[i];
        controller->state = 0;
        if (keystate[SDL_SCANCODE_Z]) controller->state |= 1 << 0;
        if (keystate[SDL_SCANCODE_X]) controller->state |= 1 << 1;
        if (keystate[SDL_SCANCODE_RSHIFT]) controller->state |= 1 << 2;
        if (keystate[SDL_SCANCODE_RETURN]) controller->state |= 1 << 3;
        if (keystate[SDL_SCANCODE_UP]) controller->state |= 1 << 4;
        if (keystate[SDL_SCANCODE_DOWN]) controller->state |= 1 << 5;
        if (keystate[SDL_SCANCODE_LEFT]) controller->state |= 1 << 6;
        if (keystate[SDL_SCANCODE_RIGHT]) controller->state |= 1 << 7;
    }
}