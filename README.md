 `micropython-wolfssl` is a [user module](https://docs.micropython.org/en/v1.19.1/develop/cmodules.html) for [micropython]() that enables [wolfSSL](https://www.wolfssl.com) to be used as backend provider for TLS and cryptographic operations. It provides an API-compatible replacement for the built-in implementations provided by the `ussl`, `ucryptolib` and `uhashlib` modules.

## Why? 

## Port-specific configuration 
This module provides various `user_settings.h` files for each port to configure wolfSSL differently. These can be found in the `config` directory, where each subdirectory corresponds to a specific micropython port and contains the appropriate `user_settings.h` file. If you wish you further customise wolfSSL, you can provide your own `user_settings.h` in a location of your choosing through the `WOLFSSL_USER_SETTINGS_FILE` makefile variable when building the port. 

## Building micropython with `micropython-wolfssl`
`micropython-wolfssl` needs to override the default implementations of a few micropython modules in order to work. This requires setting a few important C macros in your port's build configuration that prevent the default implementations from being compiled. Examples below are for the Unix port but should generalize across all ports, though macros may be defined in different locations.

### Steps

1. Familiarize yourself with the micropython [documentation for user modules](https://docs.micropython.org/en/v1.19.1/develop/cmodules.html), specifically for how user modules are included in the build
2. Create a directory for user modules, if you haven't already (in this example, we use `micropython-modules` as the directory name
3. Clone this repository in the user module directory 
4. Disable support for the `ussl`, `ucryptolib` and `uhashlib` by ensuring the following make variable is set to zero in `mpconfigport.mk` 
```
MICROPY_PY_USSL=0
```
NOTE: This should automatically prevent the C macros `MICROPY_PY_UCRYPTOLIB` and `MICROPY_PY_UHASHLIB` from being defined to 1 in `port/unix/variants/mpconfigvariant_common.h`, which disables compilation of the built-in `uhashlib` and `ucryptolib` modules. If this doesn not work (you get errors about multiple definitions for `uhashlib` and `ucryptolib` fucntions or types) then it is possible the build system has changed and you need to find another way to ensure the following three C macros are all defined to zero at build time. 
```
# this is the configuration that prevents the three modules from being compiled in, thus allowing the wolfSSL user module to re-implement them 
#define MICROPY_PY_USSL       0
#define MICROPY_PY_UCRYPTOLIB 0
#define MICROPY_PY_UHASHLIB   0
```
5. (clean) build your micropython port, providing `make` with two important command line variables: The path to your user module directory in the `USER_C_MODULES` variable, and the full path to the `user_settings.h` file that corresponds to your target port in `WOLFSSL_USER_SETTINGS_FILE`
```
make USER_C_MODULES=/path/to/micropython-modules \
     WOLFSSL_USER_SETTINGS_FILE=/path/to/micropython-modules/micropython-wolfssl/user-settings/unix/user_settings.h
```


