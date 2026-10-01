/*
 * control.h
 *
 *  Created on: Sep 17, 2024
 *      Author: ZS
 */

#ifndef INC_CONTROL_H_
#define INC_CONTROL_H_

void statemachine(void);

extern TIM_HandleTypeDef htim4; // on PB6 pwm generation on
extern TIM_HandleTypeDef htim3; // on PA6 pwm input capture measurement

extern bool en_blinkled;
extern bool en_meas_out;
extern char char_txtout;

extern uint32_t tick_BLINKLED;
extern uint32_t tick_MEASOUT;

#endif /* INC_CONTROL_H_ */
