#ifndef _RHOBAN_SHELL_H
#define _RHOBAN_SHELL_H

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Maximum length of a command line
 * and its argument
 */
#define SHELL_BUFFER_SIZE 64

/**
 * Maximum number of command arguments
 */
#define SHELL_MAX_ARGUMENTS 8

/**
 * Maximum number of commands
 * which ca be registered
 */
#define SHELL_MAX_COMMANDS 100

/**
 * shell prompt
 */
#define SHELL_PROMPT "$ "

/**
 * Initializing shell
 * This function have to be called before using
 * shell_tick()
 *
 * @param com : you have to provide to shell_init
 * an initialized instance of USBSerial or HardwareSerial
 */
void shell_init(uint32_t baudrate);

/**
 * Starts a background task
 */
void shell_start_task();

/**
 * Resets the shell
 */
void shell_reset();

/**
 * Activate or desactivate the shell
 */
void shell_disable();
void shell_enable();

/**
 * shell ticking
 * Call this function in your main loop 
 * to fetch serial port and handle shell
 */
void shell_tick();

/**
 * Prototype of a shell command
 */
typedef void shell_command_fn(unsigned int argc, char *argv[]);

/**
 * A command definition for the shell
 */
struct shell_command
{
    char *name;
    char *description;
    shell_command_fn *command;
    char parameter;
    char *parameter_type;
};

/**
 * Registers a command
 */
void shell_register(const struct shell_command *command);

#define SHELL_COMMAND_INTERNAL(name, description, parameter, parameterType) shell_command_fn shell_command_ ## name; \
    \
    char shell_command_name_ ## name [] = #name; \
    char shell_command_description_ ## name [] = description; \
    \
    struct shell_command shell_command_definition_ ## name = { \
        shell_command_name_ ## name , \
        shell_command_description_ ## name , \
        shell_command_ ## name, \
        parameter, \
        parameterType \
    }; \
    \
    __attribute__((constructor)) \
    void shell_command_init_ ## name () {  \
        shell_register(&shell_command_definition_ ## name ); \
    } \
    \
    void shell_command_ ## name (unsigned int argc, char *argv[])

#define SHELL_COMMAND(name, description) \
    SHELL_COMMAND_INTERNAL(name, description, false, NULL)

#define SHELL_PARAMETER(name, description, startValue, type, conversion, format) \
    volatile static type name = startValue; \
    char shell_parameter_type_ ## name [] = #type; \
    \
    SHELL_COMMAND_INTERNAL(name, description, true, shell_parameter_type_ ## name) \
    { \
        type g; \
        if (argc) { \
            g = conversion(argv[0]); \
            name = g; \
        } \
        printf("%s=" format "\n", #name, name); \
    }

float shell_atof(char *str);

#define SHELL_PARAMETER_FLOAT(name, description, startValue) \
    SHELL_PARAMETER(name, description, startValue, float, atof, "%g")

#define SHELL_PARAMETER_DOUBLE(name, description, startValue) \
    SHELL_PARAMETER(name, description, startValue, double, atof, "%g")

#define SHELL_PARAMETER_INT(name, description, startValue) \
    SHELL_PARAMETER(name, description, startValue, int, atoi, "%d")

#define SHELL_PARAMETER_BOOL(name, description, startValue) \
    SHELL_PARAMETER(name, description, startValue, char, (char)atoi, "%d")

#ifdef __cplusplus
} // extern C
#endif

#endif // _shell_H