/*
 * buildinfo.h
 *
 *  Created on: Nov 27, 2024
 *      Author: ZS
 */

#ifndef INC_BUILDINFO_H_
#define INC_BUILDINFO_H_

#define PRJ_VERSION (const char * ) ( "Project f446_pwm_ic, v26.09" )
#define BUILD_DATE  (const char []) { (__DATE__[4] == ' ' ?  '0' : __DATE__[4]), __DATE__[5], ' ', __DATE__[0], __DATE__[1], __DATE__[2], ' ', __DATE__[7], __DATE__[8], __DATE__[9], __DATE__[10], ' ', '-', ' ', __TIME__[0], __TIME__[1], __TIME__[2], __TIME__[3], __TIME__[4], __TIME__[5], __TIME__[6], __TIME__[7], '\0' }

#endif /* INC_BUILDINFO_H_ */
