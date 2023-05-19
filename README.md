## Overview 
### What?
 `micropython-wolfssl` is a [user module](https://docs.micropython.org/en/v1.19.1/develop/cmodules.html) for [micropython](https://micropython.org/) that enables [wolfSSL](https://www.wolfssl.com) to be used as backend provider for TLS and cryptographic operations. It is an API-compatible replacement for the built-in implementations provided by the `ussl`, `ucryptolib` and `uhashlib` modules.

### Why?
The wolfSSL embedded TLS library is a lightweight, portable, C-language-based SSL/TLS library targeted at IoT, embedded, and RTOS environments primarily because of its size, speed, and feature set. It works seamlessly in desktop, enterprise, and cloud environments as well. wolfSSL supports industry standards up to the current TLS 1.3 and DTLS 1.3, is up to 20 times smaller than OpenSSL, offers a simple API, an OpenSSL compatibility layer, OCSP and CRL support, and is backed by the robust wolfCrypt cryptography library. wolfCrypt has obtained two FIPS 140-2 Certificates, and is certified in a wide variety of operating environments. wolfCrypt is also listed on the CMVP Modules in Process List for FIPS 140-3. 

### Licensing 
wolfSSL  is dual licensed under both the GPLv2 as well as standard commercial licensing. [More info on licensing here](https://www.wolfssl.com/license/).

## Using micropython-wolfssl
`micropython-wolfssl` replaces the built-in module implementations of SSL/TLS (`ussl`), encryption (`ucryptolib`), and hashing (`uhashlib`), and implements its own API-compatible version. This means that python code utilizing these modules should not need to be changed when `micropython-wolfssl` is used, and all micropython documentation for these modules can be followed. The only changes in behavior are different SSL error codes returned on failure. WolfSSL has much more comprehensive error codes than most other SSL libraries, and are detailed in [appendix F of the wolfSSL manual](https://www.wolfssl.com/documentation/manuals/wolfssl/appendix06.html).

## Module Structure

```
micropython-wolfssl/
├── micropython.mk          <-- makefile fragment parsed by micropython
├── moducryptolib_wolfssl.c <-- implementation of ucryptolib
├── moduhashlib_wolfssl.c   <-- implemenation of uhashlib
├── modussl_wolfssl.c       <-- implementation of ussl
├── ports                   <-- directory containing port-specific configuration and code
├── README.md
├── tests                   
└── wolfssl                 <-- wolfSSL submodule
```
## Port-specific configuration
`micropython-wolfssl` supports a subset of the official ports supported by micropython. The names of the ports are the same, and you must specify the port you are targeting when building micropython using the makefile variable `WOLFSSL_PORT`. Valid values are any supported micropython port, who's names correspond to the name of the various port directories at `micropython/ports/` in the micropython repository. If left unspecified, the default port is the `unix` port. Each port supported by
micropython can be found in its own directory under `micropython-wolfssl/ports`. Each port contains a `user_settings.h` file, and optionally, a `wolfssl_port.c` file.

### User Settings
wolfSSL can be customized for a specific target application or platform using a `user_settings.h` file. This module provides a default `user_settings.h` file for each port located in the `ports` directory. If you wish you further customise wolfSSL, you can create your own `user_settings.h` and pass it to the build system through the `WOLFSSL_USER_SETTINGS_FILE` variable. This will override the default one located in the `ports/$(WOLFSSL_PORT)` directory. The directory containing this file will be added to the compiler include path, so it is recommended you create a new directory holding this file with nothing else in it.

### Port-specific code
Some ports also contain a `wolfssl_port.c` file containing implementations of certain functions required to enable wolfSSL to work on a specific hardware platform. These port files can be found in `ports/` directory, where each subdirectory corresponds to the specific micropython port. 

## Building micropython with `micropython-wolfssl`
`micropython-wolfssl` needs to override the default implementations of a few micropython modules in order to work. This requires setting a few important C macros in your port's build configuration to prevent the default implementations from being compiled. These macros are:
 
 ```
#define MICROPY_PY_USSL       0
#define MICROPY_PY_UCRYPTOLIB 0
#define MICROPY_PY_UHASHLIB   0
```
 Unfortunately, there is little consistency between the makefiles/cmake files for each each micropython port with regards to these macros. As such, each port might define these macros in a different location, might optionally define them based on a different macro, or might not define them at all, relying on a default value instead. The steps
 
 Examples below are for the Unix port but should generalize across all ports, though macros may be defined in different locations.

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


