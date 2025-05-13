# ESP32 Shell (IDF)

## Installation

Firstly, clone this repository to `components/esp32-shell`:

```
mkdir -p components
git clone git@github.com:rhoban/esp32-shell.git components/
```

In your `CMakeLists.txt`, add `esp32-shell` as a `REQUIRES` for your component:

```
# Example of app using esp32-shell
idf_component_register(SRCS "my_main_app.cpp"
                    PRIV_REQUIRES spi_flash
                    REQUIRES esp32-shell
                    INCLUDE_DIRS "")
```

## Starting

Inclue the following:

```c
#include "rhoban_shell.h"
```

In your setup process, call:

```
shell_init(115200);
shell_start_task();
```

## Use

You can declare parameters like this:

```c
SHELL_PARAMETER_INT(gain, "Gain", 123);
```

And commands with arguments:

```c
SHELL_COMMAND(hello, "Hello command")
{
  if (argc == 0) {
    printf("Usage: hello [name]\n");
  } else {
    printf("Hello %s\n", argv[0]);
  }
}
```
