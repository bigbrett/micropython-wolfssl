 `micropython-wolfssl` is a [user module](https://docs.micropython.org/en/v1.19.1/develop/cmodules.html) for [micropython]() that enables [wolfSSL](https://www.wolfssl.com) to be used as backend provider for TLS and cryptographic operations. It provides an API-compatible replacement for the built-in implementations provided by the `ussl`, `ucryptolib` and `uhashlib` modules.

## Why? 

## Port-specific configuration 
This module provides various `user_settings.h` files for each port to configure wolfSSL differently. These can be found in the `user-settings` directory, where each subdirectory corresponds to a specific micropython port and contains the appropriate `user_settings.h` file, which should be passed to your port's build through the `WOLFSSL_USER_SETTINGS_FILE` makefile variable (more on this in he steps below). If you wish you further customise wolfSSL, you can provide your own `user_settings.h` in a location of your choosing. Some ports also contain a `wolfssl_port.c` file containing implementations of certain functions required to enable wolfSSL to work on a specific hardware platform. These port files can be found in `ports/` directory, where each subdirectory corresponds to the specific micropython port. 

## Building micropython with `micropython-wolfssl`
`micropython-wolfssl` needs to override the default implementations of a few micropython modules in order to work. This requires setting a few important C macros in your port's build configuration that prevent the default implementations from being compiled. Examples below are for the Unix port but should generalize across all ports, though macros may be defined in different locations.

### Steps

1. Familiarize yourself with the micropython [documentation for user modules](https://docs.micropython.org/en/v1.19.1/develop/cmodules.html), specifically for how user modules are included in the build
2. Create a directory for user modules, if you haven't already (in this example, we use `micropython-modules` as the directory name
3. Clone this repository in the user module directory, and initialize submodules
```
cd /path/to/micropython-modules
git clone https://github.com/bigbrett/micropython-wolfssl.git
git submodule update --init --recursive
```
4. In the micropython repository, disable support for the `ussl`, `ucryptolib` and `uhashlib` modules in your port by ensuring the following C macros are defined to 0 
```
#define MICROPY_PY_USSL       0
#define MICROPY_PY_UCRYPTOLIB 0
#define MICROPY_PY_UHASHLIB   0
```

**NOTE**: For the unix port, this can be easily achieved by setting the `MICROPY_PY_USSL=0` makefile variable in `ports/unix/mpconfigport.mk`. This should automatically ensure the `MICROPY_PY_USSL C` is not set, as well as prevent the `MICROPY_PY_UCRYPTOLIB` and `MICROPY_PY_UHASHLIB` C macros  from later being defined to 1 in `port/unix/variants/mpconfigvariant_common.h`, preventing compilation of the built-in `uhashlib` and `ucryptolib` modules. If this doesn not work (you get errors about multiple definitions for `uhashlib` and `ucryptolib` fucntions or types) then it is possible the build system has changed and you need to find another way to ensure the `MICROPY_PY_USSL`, `MICROPY_PY_UCRYPTOLIB` and `MICROPY_PY_UHASHLIB` C macros are all defined to zero at build time. 

5. (clean) build your micropython port, providing `make` with two important command line variables: The path to your user module directory in the `USER_C_MODULES` variable, and the full path to the `user_settings.h` file that corresponds to your target port in `WOLFSSL_USER_SETTINGS_FILE`
```
make USER_C_MODULES=/path/to/micropython-modules \
     WOLFSSL_USER_SETTINGS_FILE=/path/to/micropython-modules/micropython-wolfssl/user-settings/unix/user_settings.h
```


