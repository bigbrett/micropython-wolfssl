WOLFSSL_MOD_DIR := $(USERMOD_DIR)

# Add required compile options for micropython
CFLAGS_USERMOD += -DWOLFSSL_USER_SETTINGS -DMICROPY_PY_WOLFSSL_UHASHLIB=1 -DMICROPY_PY_WOLFSSL_UCRYPTOLIB=1 -DMICROPY_PY_WOLFSSL_USSL

ifeq ($(WOLFSSL_DEBUG),1)
CFLAGS_USERMOD += -DWOLFSSL_DEBUG=1
endif


# Add wolfSSL include paths 
CFLAGS_USERMOD += -I$(WOLFSSL_MOD_DIR)/wolfssl -I$(WOLFSSL_MOD_DIR)/wolfssl/wolfssl

# Add the user-specified (port-specific) user settings file to the include path 
CFLAGS_USERMOD += -I$(dir $(WOLFSSL_USER_SETTINGS_FILE))


# Add all C files to SRC_USERMOD.
SRC_USERMOD += $(WOLFSSL_MOD_DIR)/modussl_wolfssl.c
SRC_USERMOD += $(WOLFSSL_MOD_DIR)/moducryptolib_wolfssl.c
SRC_USERMOD += $(WOLFSSL_MOD_DIR)/moduhashlib_wolfssl.c
SRC_USERMOD += $(addprefix $(WOLFSSL_MOD_DIR)/wolfssl/,\
	src/crl.c \
	src/internal.c \
	src/keys.c \
	src/ocsp.c \
	src/sniffer.c \
	src/ssl.c \
	src/tls.c \
	src/tls13.c \
	src/wolfio.c \
	wolfcrypt/src/aes.c \
	wolfcrypt/src/cmac.c \
	wolfcrypt/src/des3.c \
	wolfcrypt/src/dh.c \
	wolfcrypt/src/ecc.c \
	wolfcrypt/src/hmac.c \
	wolfcrypt/src/random.c \
	wolfcrypt/src/rsa.c \
	wolfcrypt/src/sha.c \
	wolfcrypt/src/sha256.c \
	wolfcrypt/src/sha512.c \
	wolfcrypt/src/sha3.c \
	wolfcrypt/src/asm.c \
	wolfcrypt/src/asn.c \
	wolfcrypt/src/blake2s.c \
	wolfcrypt/src/chacha.c \
	wolfcrypt/src/chacha20_poly1305.c \
	wolfcrypt/src/coding.c \
	wolfcrypt/src/compress.c \
	wolfcrypt/src/cpuid.c \
	wolfcrypt/src/cryptocb.c \
	wolfcrypt/src/curve25519.c \
	wolfcrypt/src/curve448.c \
	wolfcrypt/src/ecc_fp.c \
	wolfcrypt/src/eccsi.c \
	wolfcrypt/src/ed25519.c \
	wolfcrypt/src/ed448.c \
	wolfcrypt/src/error.c \
	wolfcrypt/src/fe_448.c \
	wolfcrypt/src/fe_low_mem.c \
	wolfcrypt/src/fe_operations.c \
	wolfcrypt/src/ge_448.c \
	wolfcrypt/src/ge_low_mem.c \
	wolfcrypt/src/ge_operations.c \
	wolfcrypt/src/hash.c \
	wolfcrypt/src/kdf.c \
	wolfcrypt/src/integer.c \
	wolfcrypt/src/logging.c \
	wolfcrypt/src/md5.c \
	wolfcrypt/src/memory.c \
	wolfcrypt/src/pkcs12.c \
	wolfcrypt/src/pkcs7.c \
	wolfcrypt/src/poly1305.c \
	wolfcrypt/src/pwdbased.c \
	wolfcrypt/src/rc2.c \
	wolfcrypt/src/sakke.c \
	wolfcrypt/src/signature.c \
	wolfcrypt/src/srp.c \
	wolfcrypt/src/sp_arm32.c \
	wolfcrypt/src/sp_arm64.c \
	wolfcrypt/src/sp_armthumb.c \
	wolfcrypt/src/sp_c32.c \
	wolfcrypt/src/sp_c64.c \
	wolfcrypt/src/sp_cortexm.c \
	wolfcrypt/src/sp_dsp32.c \
	wolfcrypt/src/sp_int.c \
	wolfcrypt/src/sp_x86_64.c \
	wolfcrypt/src/tfm.c \
	wolfcrypt/src/wc_dsp.c \
	wolfcrypt/src/wc_encrypt.c \
	wolfcrypt/src/wc_pkcs11.c \
	wolfcrypt/src/wc_port.c \
	wolfcrypt/src/wolfevent.c \
	wolfcrypt/src/wolfmath.c \
	)

