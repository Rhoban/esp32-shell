#include "rhoban_shell.h"
#include "driver/uart.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Uart used
static uart_port_t uart_num = UART_NUM_0;

/**
 * Global variables shell
 */

static char shell_buffer[SHELL_BUFFER_SIZE];

static bool shell_last_ok = false;
static unsigned int shell_last_pos = 0;
static unsigned int shell_pos = 0;

static const struct shell_command *shell_commands[SHELL_MAX_COMMANDS];

static unsigned int shell_command_count = 0;

static bool shell_echo_mode = true;

/**
 * Registers a command
 */
void shell_register(const struct shell_command *command) {
  shell_commands[shell_command_count++] = command;
}

static void displayHelp(bool parameter) {
  char buffer[256];
  unsigned int i;

  if (parameter) {
    printf("Available parameters:\n");
  } else {
    printf("Available commands:\n");
  }

  for (i = 0; i < shell_command_count; i++) {
    const struct shell_command *command = shell_commands[i];

    if (command->parameter != parameter) {
      continue;
    }

    int namesize = strlen(command->name);
    int descsize = strlen(command->description);
    int typesize =
        (command->parameter_type == NULL) ? 0 : strlen(command->parameter_type);

    memcpy(buffer, command->name, namesize);
    buffer[namesize++] = ':';
    buffer[namesize++] = '\r';
    buffer[namesize++] = '\n';
    buffer[namesize++] = '\t';
    memcpy(buffer + namesize, command->description, descsize);
    if (typesize) {
      buffer[namesize + descsize++] = ' ';
      buffer[namesize + descsize++] = '(';
      memcpy(buffer + namesize + descsize, command->parameter_type, typesize);
      buffer[namesize + descsize + typesize++] = ')';
    }
    buffer[namesize + descsize + typesize++] = '\r';
    buffer[namesize + descsize + typesize++] = '\n';
    uart_write_bytes(uart_num, buffer, namesize + descsize + typesize);
  }
}

/**
 * Internal helping command
 */
SHELL_COMMAND(help, "Displays the help about commands") { displayHelp(false); }

void shell_params_show() {
  unsigned int i;

  for (i = 0; i < shell_command_count; i++) {
    const struct shell_command *command = shell_commands[i];

    if (command->parameter) {
      command->command(0, NULL);
    }
  }
}

/**
 * Display available parameters
 */
SHELL_COMMAND(params,
              "Displays the available parameters. Usage: params [show]") {
  if (argc && strcmp(argv[0], "show") == 0) {
    shell_params_show();
  } else {
    displayHelp(true);
  }
}

/**
 * Switch echo mode
 */
SHELL_COMMAND(echo, "Switch echo mode. Usage echo [on|off]") {
  if ((argc == 1 && strcmp("on", argv[0])) || shell_echo_mode == false) {
    shell_echo_mode = true;
    printf("Echo enabled\n");
  } else {
    shell_echo_mode = false;
    printf("Echo disabled\n");
  }
}

/**
 * Write the shell prompt
 */
void shell_prompt() { uart_write_bytes(uart_num, SHELL_PROMPT, 2); }

const struct shell_command *
shell_find_command(char *command_name, unsigned int command_name_length) {
  unsigned int i;

  for (i = 0; i < shell_command_count; i++) {
    const struct shell_command *command = shell_commands[i];

    if (strlen(command->name) == command_name_length &&
        strncmp(shell_buffer, command->name, command_name_length) == 0) {
      return command;
    }
  }

  return NULL;
}

/***
 * Executes the given command with given parameters
 */
bool shell_execute(char *command_name, unsigned int command_name_length,
                   unsigned int argc, char **argv) {
  unsigned int i;
  const struct shell_command *command;

  // Try to find and execute the command
  command = shell_find_command(command_name, command_name_length);
  if (command != NULL) {
    command->command(argc, argv);
  }

  // If it fails, try to parse the command as an allocation (a=b)
  if (command == NULL) {
    for (i = 0; i < command_name_length; i++) {
      if (command_name[i] == '=') {
        command_name[i] = '\0';
        command_name_length = strlen(command_name);
        command = shell_find_command(command_name, command_name_length);

        if (command && command->parameter) {
          argv[0] = command_name + i + 1;
          argv[1] = NULL;
          argc = 1;
          command->command(argc, argv);
        } else {
          command = NULL;
        }

        if (!command) {
          printf("Unknown parameter: \n");
          uart_write_bytes(uart_num, command_name, command_name_length);
          uart_write_bytes(uart_num, "\r\n", 2);
          return false;
        }
      }
    }
  }

  // If it fails again, display the "unknown command" message
  if (command == NULL) {
    printf("Unknown command: \n");
    uart_write_bytes(uart_num, command_name, command_name_length);
    uart_write_bytes(uart_num, "\r\n", 2);
    return false;
  }

  return true;
}

/***
 * Process the receive buffer to parse the command and executes it
 */
void shell_process() {
  char *saveptr;
  unsigned int command_name_length;

  unsigned int argc = 0;
  char *argv[SHELL_MAX_ARGUMENTS + 1];

  printf("\n");

  strtok_r(shell_buffer, " ", &saveptr);
  while ((argv[argc] = strtok_r(NULL, " ", &saveptr)) != NULL &&
         argc < SHELL_MAX_ARGUMENTS) {
    *(argv[argc] - 1) = '\0';
    argc++;
  }

  if (saveptr != NULL) {
    *(saveptr - 1) = ' ';
  }

  command_name_length = strlen(shell_buffer);

  if (command_name_length > 0) {
    shell_last_ok =
        shell_execute(shell_buffer, command_name_length, argc, argv);
  } else {
    shell_last_ok = false;
  }

  shell_last_pos = shell_pos;
  shell_pos = 0;
  shell_prompt();
}

static const int uart_buffer_size = (1024 * 2);
static QueueHandle_t uart_queue;

/**
 * Save the Serial object globaly
 */
void shell_init(uint32_t baudrate) {
  ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, uart_buffer_size,
                                      uart_buffer_size, 10, &uart_queue, 0));
}

TaskHandle_t shell_task_handle = NULL;

void shell_task(void *) {
  while (true) {
    shell_tick();
    vTaskDelay(10);
  }
}

void shell_reset() {
  shell_pos = 0;
  shell_last_pos = 0;
  shell_buffer[0] = '\0';
  shell_last_ok = false;
  shell_prompt();
}

/**
 * Stops the shell
 */
void shell_enable() { shell_last_ok = false; }

static bool has_available_bytes() {
  int available_bytes = 0;
  ESP_ERROR_CHECK(
      uart_get_buffered_data_len(uart_num, (size_t *)&available_bytes));

  return available_bytes > 0;
}

/**
 * Ticking the shell, this will cause lookup for characters
 * and eventually a call to the process function on new lines
 */
void shell_tick() {
  char c;

  while (has_available_bytes()) {
    if (uart_read_bytes(uart_num, &c, 1, 1) != 1) {
      continue;
    }

    if (c == '\0' || c == 0xff) {
      continue;
    }

    // Return key
    if (c == '\r' || c == '\n') {
      if (shell_pos == 0 && shell_last_ok) {
        // If the user pressed no keys, restore the last
        // command and run it again
        unsigned int i;
        for (i = 0; i < shell_last_pos; i++) {
          if (shell_buffer[i] == '\0') {
            shell_buffer[i] = ' ';
          }
        }
        shell_pos = shell_last_pos;
      }
      shell_buffer[shell_pos] = '\0';
      shell_process();
      // Back key
    } else if (c == '\x7f') {
      if (shell_pos > 0) {
        shell_pos--;
        uart_write_bytes(uart_num, "\x8 \x8", 3);
      }
      // Special key
    } else if (c == '\x1b') {
      uart_read_bytes(uart_num, &c, 1, 1);
      uart_read_bytes(uart_num, &c, 1, 1);
      // Others
    } else {
      shell_buffer[shell_pos] = c;
      if (shell_echo_mode) {
        uart_write_bytes(uart_num, &c, 1);
      }

      if (shell_pos < SHELL_BUFFER_SIZE - 1) {
        shell_pos++;
      }
    }
  }
}
