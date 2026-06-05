/* input_joystick.h */
#ifndef INPUT_JOYSTICK_H
#define INPUT_JOYSTICK_H

#include "platform.h"

void joystick_init(void);
void joystick_read(input_state_t *in);

#endif
