
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

