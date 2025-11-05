#include "modfunc.h"
#include "../modules.h"
#include "../graphics/gui_utility.h"
#include <stdint.h>

static struct GUIButton* play;

void timeline_draw(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {

    // testing buttons!
    if (!play) {play = createButton(); 
        play->weldToWindow = POSITION_BOTTOM; 
        play->localX = 20;
        play->localY = 30;
        play->width = 40;
        play->height = 20;
        play->text = "Play";
        play->buttonID = BUTTON_TIMELINE_PLAYSTOP;
        addButton(play);
    }
    drawButton(play);
}
