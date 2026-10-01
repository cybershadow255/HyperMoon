/*
 * commands.h
 *
 *  Created on: Nov 27, 2024
 *      Author: ZS
 */

#ifndef INC_COMMANDS_H_
#define INC_COMMANDS_H_

// command prototypes
void cmd_psc(char **Args, int argCount);
void cmd_arr(char **Args, int argCount);
void cmd_ccr1(char **Args, int argCount);

void cmd_LED(char** Args, int argCount);
void cmd_PC3(char** Args, int argCount);
void cmd_blink_LED(char **Args, int argCount);
void cmd_meas_out(char **Args, int argCount);
void cmd_Help(char **Args, int argCount);
void cmd_Print_Version(char** Args, int argCount);
void cmd_test_args(char** Args, int argCount);

// CommandMap type
typedef struct CommandMap_t {
  char *cmdName;
  void (*funcPtr)(char**, int );
} CommandMap_t;

extern const CommandMap_t g_cmdMap[];
extern const int NUM_COMMANDS;
extern const int MAX_NUM_ARGS;

#endif /* INC_COMMANDS_H_ */
