 `micropython-wolfssl` is a [user module](https://docs.micropython.org/en/v1.19.1/develop/cmodules.html) for [micropython]() that enables [wolfSSL](https://www.wolfssl.com) to be used as backend provider for TLS and cryptographic operations. It provides an API-compatible replacement for the built-in implementations provided by the `ussl`, `ucryptolib` and `uhashlib` modules.

## Why? 

## Files

## Building `micropython` to use `micropython-wolfssl`
`micropython-wolfssl` needs to override the default implementations of a few micropython modules in order to work. This requires setting a few important C macros in your port's build configuration that prevent the default implementations from being compiled. Examples below are for the Unix port but should generalize across all ports, though macros may be defined in different locations.

1. Familiarize yourself with the micropython [documentation for user modules](https://docs.micropython.org/en/v1.19.1/develop/cmodules.html), specifically for how user modules are included in the build
2. Create a directory for user modules, if you haven't already (in this example, we use `micropython-modules` as the directory name
3. Clone this repository in the user module directory 
4. Disable support for mbedTLS and axTLS as TLS providers for the `ussl` module by ensuring the two make variables in `mpconfigport.mk` are set to zero
```
MICROPY_SSL_MBEDTLS=0
MICROPY_SSL_AXTLS=0
```
5. Disable compilation of the built-in `uhashlib` and `ucryptolib` modules by defining the following two C macros to zero in `port/unix/variants/mpconfigvariant_common.h`
```
#define MICROPY_PY_UCRYPTOLIB 0
#define MICROPY_PY_UHASHLIB   0
```
5. (clean) build your micropython port, providing `make` with two important command line variables: The path to your user module directory in the `USER_C_MODULES` variable, and the full path to the `user_settings.h` file that corresponds to your target port in `WOLFSSL_USER_SETTINGS_FILE`
```
make USER_C_MODULES=/path/to/micropython-modules \
     WOLFSSL_USER_SETTINGS_FILE=/path/to/micropython-modules/micropython-wolfssl/user-settings/unix/user_settings.h
```


