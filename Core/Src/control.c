/*
 * control.c
 *
 *  Created on: Nov 27, 2024
 *      Author: ZS
 */

#include "main.h"

enum state_t { INIT = 0, IDLE, CHECKCMD, BLINKLED, MEASOUT };

static enum state_t act_state;
static enum state_t next_state = INIT;

uint32_t tick_BLINKLED;
uint32_t tick_MEASOUT;

bool en_blinkled = false;
bool en_meas_out = false;

char char_txtout = '.';

void statemachine(void){
  uint32_t tick = HAL_GetTick();
  act_state = next_state;
  switch (act_state) {
      case INIT:
        start_rx_uart_it();
        HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, 1);
        HAL_TIM_OC_Start(&htim4, TIM_CHANNEL_1);
        HAL_TIM_IC_Start_IT(&htim3, TIM_CHANNEL_1); // Primary channel - rising edge
        HAL_TIM_IC_Start(&htim3, TIM_CHANNEL_2);    // Secondary channel - falling edge
        tick_BLINKLED = tick +250;
        tick_MEASOUT = tick +500;
        next_state = CHECKCMD;
        break;

      case IDLE:
        break;

      case CHECKCMD:
        Check_Command();
        next_state = BLINKLED;
        break;

      case BLINKLED:
        if ( true == en_blinkled ) {
          if ( tick > tick_BLINKLED) {
            tick_BLINKLED += 250;
            HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
          }
        }
        next_state = MEASOUT;
        break;

      case MEASOUT:
        if ( true == en_meas_out) {
          if (tick > tick_MEASOUT) {
            tick_MEASOUT += 2000;
            __HAL_TIM_ENABLE_IT(&htim3, TIM_IT_CC1);
            HAL_Delay(10);

            meas_ic_freq = (double) TIMER_CLOCK_FREQ / (cnt_period );
            meas_ic_duty = (double) 100 * ( cnt_on ) / ( cnt_period );

            printf("%.3f freq=%3.2E duty=%.2f\r\n",(float)tick/1000, meas_ic_freq, meas_ic_duty);
          }
        }
        next_state = CHECKCMD;
        break;

      default:
        next_state = CHECKCMD;
        break;
  }
}
