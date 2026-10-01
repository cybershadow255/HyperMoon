/*
 * tim_it.c
 *
 *  Created on: Oct 22, 2024
 *      Author: ZS
 *
 */

#include "main.h"

const uint32_t TIMER_CLOCK_FREQ = 90000000; // APB1 timer clock
volatile uint32_t cnt_period;
volatile uint32_t cnt_on;
double meas_ic_freq = 0;
double meas_ic_duty = 0;

// TIM3 configured as combined PWM input on CH1
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
  if (htim->Instance == TIM3) {
    cnt_period = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
    cnt_on = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);
    __HAL_TIM_DISABLE_IT(&htim3, TIM_IT_CC1);
  }
}
