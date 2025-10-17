#include "nes_controller.hpp"

Controller controllers[2];

void UpdateControllers(void) {
    const uint8_t* Keystate = SDL_GetKeyboardState(nullptr);

    for (unsigned char i=0;i<2;i++) {
        Controller *ControllerP = &controllers[i];
        ControllerP->state = 0;
        if (Keystate[SDL_SCANCODE_Z]) ControllerP->state |= A_BUTTON;
        if (Keystate[SDL_SCANCODE_X]) ControllerP->state |= B_BUTTON;
        if (Keystate[SDL_SCANCODE_RSHIFT]) ControllerP->state |= SELECT_BUTTON;
        if (Keystate[SDL_SCANCODE_RETURN]) ControllerP->state |= START_BUTTON;
        if (Keystate[SDL_SCANCODE_UP]) ControllerP->state |= STICK_UP;
        if (Keystate[SDL_SCANCODE_DOWN]) ControllerP->state |= STICK_DOWN;
        if (Keystate[SDL_SCANCODE_LEFT]) ControllerP->state |= STICK_LEFT;
        if (Keystate[SDL_SCANCODE_RIGHT]) ControllerP->state |= STICK_RIGHT;
    }
}