
# Requirements
- must turn off mbedTLS and axTLS mpconfigport.mk

- `MICROPY_SSL_MBEDTLS=1`
- `MICROPY_SSL_AXTLS=1`

- must provide full path to `user_settings.h` as argument to `make` in `WOLFSSL_USER_SETTINGS_FILE` 

- must define `MICROPY_PY_UHASHLIB` and `MICROPY_PY_UCRYPTOLIB` to zero (for unix port this is in `variants/mpconfigvariant_common.h`)

