/*
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "common.h"

#if XASH_INPUT == INPUT_CTR

#include <3ds.h>

#include "touch.h"
#include "input.h"
#include "joyinput.h"

#include "keyboardOverlay_bin.h"
#include "touchOverlay_bin.h"

static uint16_t *framebuffer;

static bool text_input = false;
static bool keyboard_enabled = false;
static bool shift_pressed = false;
static int button_pressed = 0;

static circlePosition cstick;
static circlePosition circlepad;
static touchPosition  touch, old_touch;

typedef struct buttonmapping_s{
	uint32_t btn;
	int key;
} buttonmapping_t;

static char keymap[2][14 * 6] = 
{
	{
		K_ESCAPE , K_F1, K_F2, K_F3, K_F4, K_F5, K_F6, K_F7, K_F8, K_F9, K_F10, K_F11, K_F12, 0,
		'`' , '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', K_BACKSPACE,
		K_TAB, 'q' , 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '|',
		0, 'a' , 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', K_ENTER, K_ENTER,
		K_SHIFT, 'z' , 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, K_UPARROW, 0,
		0, 0 , 0, 0, K_SPACE, K_SPACE, K_SPACE, K_SPACE, K_SPACE, K_SPACE, 0, K_LEFTARROW, 	K_DOWNARROW, K_RIGHTARROW
	},
	{
		K_ESCAPE , K_F1, K_F2, K_F3, K_F4, K_F5, K_F6, K_F7, K_F8, K_F9, K_F10, K_F11, K_F12, 0,
		'~' , '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', K_BACKSPACE,
		K_TAB, 'Q' , 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '|',
		0, 'A' , 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', K_ENTER, K_ENTER,
		K_SHIFT, 'Z' , 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, K_UPARROW, 0,
		0, 0 , 0, 0, K_SPACE, K_SPACE, K_SPACE, K_SPACE, K_SPACE, K_SPACE, 0, K_LEFTARROW, 	K_DOWNARROW, K_RIGHTARROW
	}
};

static buttonmapping_t btnmap[14] =
{
	{ KEY_SELECT, (int)'~' },
	{ KEY_START, K_ESCAPE },
	{ KEY_DUP, K_UPARROW },
	{ KEY_DRIGHT, K_MWHEELUP },
	{ KEY_DDOWN, K_DOWNARROW },
	{ KEY_DLEFT, K_MWHEELDOWN },
	{ KEY_L, K_MOUSE2 },
	{ KEY_R, K_MOUSE1 },
	{ KEY_ZL, K_CTRL },
	{ KEY_ZR, 'r' },
	{ KEY_A, K_ENTER },
	{ KEY_B, K_SPACE },
	{ KEY_X, 'f' },
	{ KEY_Y, 'e' },
};

static void DrawSubscreen()
{
	int x, y;
	uint16_t* overlay;

	if(keyboard_enabled)
		overlay = keyboardOverlay_bin;
	else
		overlay = touchOverlay_bin;

	memcpy(framebuffer, overlay, 240*320*2);

	if(keyboard_enabled && shift_pressed)
	{
		for(x = 20; x < 23; x++)
      		for(y = 152; y < 155; y++)
				framebuffer[(x*240 + (239 - y))] = RGB8_to_565(39, 174, 96);
	}

}

static void RescaleAnalog( int *x, int *y, float deadZone )
{
	float analogX = (float)*x;
	float analogY = (float)*y;
	float maximum = 180.0f;
	float magnitude = sqrtf( analogX * analogX + analogY * analogY );

	if( magnitude >= deadZone )
	{
		float scalingFactor = maximum / magnitude * ( magnitude - deadZone ) / ( maximum - deadZone );
		*x = (int)( analogX * scalingFactor );
		*y = (int)( analogY * scalingFactor );
	}
	else
	{
		*x = 0;
		*y = 0;
	}
}

static inline void UpdateAxes( void )
{
	hidCircleRead(&circlepad);
	hidCstickRead(&cstick);

	int left_x = circlepad.dx;
	int left_y = circlepad.dy;
	int right_x = cstick.dx;
	int right_y = cstick.dy;

	if( abs( left_x ) < 15.0f ) left_x = 0;
	if( abs( left_y ) < 15.0f ) left_y = 0;

	RescaleAnalog( &right_x, &right_y, 25.0f );

	Joy_AxisMotionEvent( 0, 0, left_x * 180 );
	Joy_AxisMotionEvent( 0, 1, left_y * -180 );

	Joy_AxisMotionEvent( 0, 2, right_x * -180 );
	Joy_AxisMotionEvent( 0, 3, right_y *  180 );
}

static inline void UpdateButtons( void )
{
	for(int i = 0; i < 14; i++)
	{
		if( ( hidKeysDown() & btnmap[i].btn ) )
				Key_Event( btnmap[i].key, true );
		else if( ( hidKeysUp() & btnmap[i].btn ) )
				Key_Event( btnmap[i].key, false );
	}
}

static inline void UpdateTouch( void )
{
	if(hidKeysDown() & KEY_TOUCH)
	{
		if(touch.py < 42 && touch.py > 1)
		{
			button_pressed = K_AUX17 + (touch.px / 80);
		}

		if(keyboard_enabled && touch.py > 59 && touch.py < 193 && touch.px > 6 && touch.px < 314)
		{
			int key_num = ((touch.py - 59) / 22) * 14 + (touch.px - 6) / 22;

			if(text_input)
			{
				char character = keymap[shift_pressed][key_num];

				if(character == K_SHIFT)
				{
					shift_pressed = !shift_pressed;
					DrawSubscreen();
				}
				if(character < 32 || character > 126)
				{
					button_pressed = character;
				}
				else
				{
					CL_CharEvent( character );
				}
			}
			else
			{
				button_pressed = keymap[0][key_num];
			}
		}

		if(touch.py > 213 && touch.px > 135 && touch.px < 185)
		{
			keyboard_enabled = !keyboard_enabled;
			DrawSubscreen();
		}

		if(button_pressed)
		{
			Key_Event(button_pressed, true);
		}
	}
	else if(hidKeysHeld() & KEY_TOUCH)
	{
		if(!button_pressed)
		{
			IN_TouchEvent( event_motion, 0, touch.px / 400.0f, touch.py / 300.0f, (touch.px - old_touch.px) / 400.0f, (touch.py - old_touch.py) / 300.0f );
		}
	}
	else if(hidKeysUp() & KEY_TOUCH)
	{
		if(button_pressed)
		{
			Key_Event(button_pressed, false);
		}

		button_pressed = 0;
	}

	old_touch = touch;
}

void ctr_IN_Init( void )
{
	framebuffer = (uint16_t*)gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
	DrawSubscreen();
}

void ctr_IN_Frame( void )
{
	hidScanInput();
	hidTouchRead(&touch);
	UpdateAxes();
	UpdateButtons();
	UpdateTouch();
}

void ctr_EnableTextInput( int enable, qboolean force )
{
	text_input = enable;
}

#endif // INPUT_CTR
