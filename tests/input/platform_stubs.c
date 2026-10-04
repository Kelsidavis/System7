#include "Platform/PS2Input.h"
#include "EventManager/EventGlobals.h"
#include "check.h"

int event_post_mouse(int16_t x_delta, int16_t y_delta, uint8_t buttons);

int main(void)
{
    Point position;
    KeyMap keys;
    CHECK(!PS2_IsInitialized(), 1);
    GetMouse(&position);
    CHECK(position.h == 400 && position.v == 300, 2);
    SetMouseButtons(3);
    CHECK(GetMouseButtons() == 3 && gCurrentButtons == 3, 3);
    CHECK(InitPS2Controller() && PS2_IsInitialized(), 4);
    CHECK(GetMouseButtons() == 0 && GetMouseButtonsLatched() == 0, 5);
    SetMousePosition(123, 234);
    GetMouse(&position);
    CHECK(position.h == 123 && position.v == 234, 6);
    CHECK(event_post_mouse(4, -5, 1) == 0, 7);
    GetMouse(&position);
    CHECK(position.h == 127 && position.v == 229, 8);
    CHECK(GetMouseButtons() == 1 && GetMouseButtonsLatched() == 1 && gCurrentButtons == 1, 9);
    SetMouseButtons(0);
    CHECK(GetMouseButtons() == 0 && gCurrentButtons == 0, 10);
    memset(keys, 0xff, sizeof(keys));
    CHECK(GetPS2KeyboardState(keys), 11);
    for (unsigned i = 0; i < sizeof(keys); ++i) CHECK(keys[i] == 0, 12);
    GetMouse(NULL);
    return 0;
}
