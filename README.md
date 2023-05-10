# Overview
 `micropython-wolfssl` is a [user module](https://docs.micropython.org/en/v1.19.1/develop/cmodules.html) for [micropython]() that enables [wolfSSL](https://www.wolfssl.com) to be used as backend provider for TLS and cryptographic operations. It provides an API-compatible replacement for the built-in implementations provided by the `ussl`, `ucryptolib` and `uhashlib` modules.

# Why? 

# Files

# Building `micropython` to use `micropython-wolfssl`
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


# Requirements
- must turn off mbedTLS and axTLS mpconfigport.mk

- `MICROPY_SSL_MBEDTLS=1`
- `MICROPY_SSL_AXTLS=1`

- must provide full path to `user_settings.h` as argument to `make` in `WOLFSSL_USER_SETTINGS_FILE` 

- must define `MICROPY_PY_UHASHLIB` and `MICROPY_PY_UCRYPTOLIB` to zero (for unix port this is in `variants/mpconfigvariant_common.h`)

```
diff --git a/ports/unix/mpconfigport.mk b/ports/unix/mpconfigport.mk
index ce6183c13..c7797ade4 100644
--- a/ports/unix/mpconfigport.mk
+++ b/ports/unix/mpconfigport.mk
@@ -30,7 +30,7 @@ MICROPY_PY_USSL = 1
 MICROPY_SSL_AXTLS = 0
 # mbedTLS is more up to date and complete implementation, but also
 # more bloated.
-MICROPY_SSL_MBEDTLS = 1
+MICROPY_SSL_MBEDTLS = 0
 
 # jni module requires JVM/JNI
 MICROPY_PY_JNI = 0
diff --git a/ports/unix/variants/mpconfigvariant_common.h b/ports/unix/variants/mpconfigvariant_common.h
index e4e0c0c5d..93e9201f6 100644
--- a/ports/unix/variants/mpconfigvariant_common.h
+++ b/ports/unix/variants/mpconfigvariant_common.h
@@ -106,7 +106,8 @@
 #if MICROPY_PY_USSL
 #define MICROPY_PY_UHASHLIB_MD5        (1)
 #define MICROPY_PY_UHASHLIB_SHA1       (1)
-#define MICROPY_PY_UCRYPTOLIB          (1)
+#define MICROPY_PY_UCRYPTOLIB          (0)
+#define MICROPY_PY_UHASHLIB            (0)
 #endif
 
 // Use the posix implementation of the "select" module (unless the variant
```

