/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Linaro Ltd.
 * Copyright (c) 2019 Paul Sokolovsky
 * Copyright (c) 2023 wolfSSL Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "py/mpconfig.h"
#if !MICROPY_PY_USSL && MICROPY_PY_WOLFSSL_USSL

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h> // needed because mp_is_nonblocking_error uses system error codes

#include "py/runtime.h"
#include "py/stream.h"
#include "py/objstr.h"

#ifdef HAVE_CONFIG_H
    #include <config.h>
#endif

#ifndef WOLFSSL_USER_SETTINGS
    #include <wolfssl/options.h>
#endif
#include "wolfssl/wolfcrypt/settings.h"

#include "wolfssl/ssl.h"

#define MP_STREAM_POLL_RDWR (MP_STREAM_POLL_RD | MP_STREAM_POLL_WR)

typedef struct _mp_obj_ssl_socket_t {
    mp_obj_base_t base;
    mp_obj_t sock;

    WOLFSSL_CTX *ctx;
    WOLFSSL *ssl;

    uintptr_t poll_mask; // Indicates which read or write operations the protocol needs next
    int last_error; // The last error code, if any
} mp_obj_ssl_socket_t;

struct ssl_args {
    mp_arg_val_t key;
    mp_arg_val_t cert;
    mp_arg_val_t server_side;
    mp_arg_val_t server_hostname;
    mp_arg_val_t cert_reqs;
    mp_arg_val_t cadata;
    mp_arg_val_t do_handshake;
};

STATIC const mp_obj_type_t ussl_socket_type;


#if WOLFSSL_DEBUG
void wolfSSL_logging_cb(const int logLevel, const char *const logMessage)
{
    mp_printf(&mp_plat_print, "WOLFSSL DBG: %s\n", logMessage);
}
#endif 

STATIC NORETURN void wolfssl_raise_error(int err) {

    mp_obj_str_t *o_str = m_new_obj_maybe(mp_obj_str_t);
    byte *o_str_buf = m_new_maybe(byte, WOLFSSL_MAX_ERROR_SZ);
    if (o_str == NULL || o_str_buf == NULL) {
        mp_raise_OSError(err);
    }

    wolfSSL_ERR_error_string(err, (char*)o_str_buf);
    
    // Put the exception object together
    o_str->base.type = &mp_type_str;
    o_str->data = o_str_buf;
    o_str->len = strnlen((char*)o_str_buf, WOLFSSL_MAX_ERROR_SZ);
    o_str->hash = qstr_compute_hash(o_str->data, o_str->len);
    // raise
    mp_obj_t args[2] = { MP_OBJ_NEW_SMALL_INT(err), MP_OBJ_FROM_PTR(o_str)};
    nlr_raise(mp_obj_exception_make_new(&mp_type_OSError, 2, 0, args));
}

// _wolfssl_ssl_send is called by wolfSSL as a custom IO function to send bytes from the underlying socket
STATIC int _wolfssl_ssl_send(WOLFSSL* ssl, char* buf, int len, void* ctx) {
    //printf("******Send\n");
    mp_obj_t sock = *(mp_obj_t *)ctx;

    const mp_stream_p_t *sock_stream = mp_get_stream(sock);
    int err;

    mp_uint_t out_sz = sock_stream->write(sock, buf, len, &err);
    //printf("  out_sz = %ld\n  err = %d\n",out_sz, err);
    if (out_sz == MP_STREAM_ERROR) {
        //printf("  ssl send :: MP_STREAM_ERROR\n");
        if (mp_is_nonblocking_error(err)) {
            //printf("  WANT_WRITE\n");
            return WOLFSSL_CBIO_ERR_WANT_WRITE;
        }
        // BRN-TODO MBEDTLS negates the below value, apparently it only passes through negatives? Circle back to this
        return err; // convert an MP_ERRNO to something mbedtls passes through as error
    } else {
        return out_sz;
    }
}

// _wolfssl_ssl_recv is called by wolfSSL as a custom IO function to receive bytes from the underlying socket
STATIC int _wolfssl_ssl_recv(WOLFSSL* ssl, char *buf, int len, void* ctx) {
    //printf("******Recv\n");
    mp_obj_t sock = *(mp_obj_t *)ctx;

    const mp_stream_p_t *sock_stream = mp_get_stream(sock);
    int err;

    mp_uint_t out_sz = sock_stream->read(sock, buf, len, &err);
    //printf("  out_sz = %ld\n  err = %d\n",out_sz, err);
    if (out_sz == MP_STREAM_ERROR) {
        //printf("  ssl recv :: MP_STREAM_ERROR\n");
        if (mp_is_nonblocking_error(err)) {
            //printf("  WANT_READ\n");
            return WOLFSSL_CBIO_ERR_WANT_READ;
        }
        return err;
    } else {
        //printf("out_sz = %ld\n",out_sz);
        
        // BRN-TODO should we really be singling out this case? 
        if (out_sz == 0) {
            return WOLFSSL_CBIO_ERR_WANT_READ;
        }

        return out_sz;
    }
}


STATIC mp_obj_ssl_socket_t *socket_new(mp_obj_t sock, struct ssl_args *args) {
    int ret = WOLFSSL_FAILURE;

    // Verify the socket object has the full stream protocol
    mp_get_stream_raise(sock, MP_STREAM_OP_READ | MP_STREAM_OP_WRITE | MP_STREAM_OP_IOCTL);

    #if MICROPY_PY_USSL_FINALISER
    mp_obj_ssl_socket_t *o = m_new_obj_with_finaliser(mp_obj_ssl_socket_t);
    #else
    mp_obj_ssl_socket_t *o = m_new_obj(mp_obj_ssl_socket_t);
    #endif
    o->base.type = &ussl_socket_type;
    o->sock = sock;
    o->poll_mask = 0;
    o->last_error = 0;

    if ((ret = wolfSSL_Init()) != WOLFSSL_SUCCESS) {
        // BRN-TODO: best way to handle?
        goto cleanup;
    }

#if WOLFSSL_DEBUG
    wolfSSL_Debugging_ON();
    wolfSSL_SetLoggingCb(wolfSSL_logging_cb);
#endif 

    // BRN-TODO: entropy seeding?

    // if server, load server method and associated certs, CA, keys, also look at SNI...
    if (args->server_side.u_bool) {
        if ((o->ctx = wolfSSL_CTX_new(wolfTLSv1_3_server_method())) == NULL) {
            // BRN-TODO: error handling
            goto cleanup;
        }
    }
    else {
        if ((o->ctx = wolfSSL_CTX_new(wolfTLSv1_3_client_method())) == NULL) {
            // BRN-TODO: error handling
            goto cleanup;
        }
    }

    // unit tests use a 512 bit key 
    if (wolfSSL_CTX_SetMinRsaKey_Sz(o->ctx, 512) != WOLFSSL_SUCCESS) {
        goto cleanup;
    }

    // Parse key in DER ASN1 format only
    if (args->key.u_obj != mp_const_none) {
        size_t key_len;
        const byte *key = (const byte *)mp_obj_str_get_data(args->key.u_obj, &key_len);
        // BRN-TODO: check key_len+1, this accounts for null return
        ret = wolfSSL_CTX_use_PrivateKey_buffer(o->ctx, key, key_len, SSL_FILETYPE_ASN1);
        if (ret != SSL_SUCCESS) {
            mp_raise_ValueError(MP_ERROR_TEXT("invalid key"));
            goto cleanup;
        }

        size_t cert_len;
        const byte *cert = (const byte *)mp_obj_str_get_data(args->cert.u_obj, &cert_len);
        // BRN-TODO: check len should include terminating null
        ret = wolfSSL_CTX_use_certificate_buffer(o->ctx, cert, cert_len, SSL_FILETYPE_ASN1);
        if (ret != SSL_SUCCESS) {
            mp_raise_ValueError(MP_ERROR_TEXT("invalid cert"));
            goto cleanup;
        }
    }

    if (args->cadata.u_obj != mp_const_none) {
        size_t cacert_len;
        const byte *cacert = (const byte *)mp_obj_str_get_data(args->cadata.u_obj, &cacert_len);
        // BRN-TODO: check len should include terminating null
        ret = wolfSSL_CTX_load_verify_buffer(o->ctx, cacert, cacert_len, SSL_FILETYPE_ASN1);
        if (ret != SSL_SUCCESS) {
            mp_raise_ValueError(MP_ERROR_TEXT("invalid CA"));
            goto cleanup;
        }
    } /* else {
        ret = wolfSSL_CTX_set_cipher_list(o->ctx, "ADH-AES128-SHA");
        if (ret != SSL_SUCCESS) {
            //printf("Failed to set cipher list\n"); 
        }
    }
    */

    // server-side SNI
    if (args->server_hostname.u_obj != mp_const_none) {
        const char *sni = mp_obj_str_get_str(args->server_hostname.u_obj);
        ret = wolfSSL_CTX_UseSNI(o->ctx, WOLFSSL_SNI_HOST_NAME, sni, strlen(sni));
        if (ret != WOLFSSL_SUCCESS) {
            mp_raise_ValueError(MP_ERROR_TEXT("unable to set SNI"));
            // BRN-TODO: error handling for ret
            goto cleanup;
        }
        wolfSSL_CTX_SNI_SetOptions(o->ctx, WOLFSSL_SNI_HOST_NAME, WOLFSSL_SNI_CONTINUE_ON_MISMATCH);
    }

    // set authentication mode based on arguments
    // BRN-TODO validate cert_reqs args?
    wolfSSL_CTX_set_verify(o->ctx, args->cert_reqs.u_int, NULL);

    // bind sockets
    wolfSSL_CTX_SetIOSend(o->ctx, _wolfssl_ssl_send);
    wolfSSL_CTX_SetIORecv(o->ctx, _wolfssl_ssl_recv);

    // create new session context
    //printf("Creating session context...\n");
    if ((o->ssl = wolfSSL_new(o->ctx)) == NULL) {
        //printf("Unable to create session context...\n");
        // BRN-TODO: error handling for ret
        goto cleanup;
    }

    // set IO context for our custom IO callbacks to point to the socket instance
    wolfSSL_SetIOReadCtx(o->ssl, &o->sock);
    wolfSSL_SetIOWriteCtx(o->ssl, &o->sock);

    // optionally, do handshake
    //if (args->server_side.u_bool && args->do_handshake.u_bool) {
    if (args->do_handshake.u_bool) {
        //printf("***DOING HANDSHAKE\n");
        while ((ret = wolfSSL_connect(o->ssl)) != WOLFSSL_SUCCESS)  {
            if (ret != WOLFSSL_ERROR_WANT_WRITE && ret != WOLFSSL_ERROR_WANT_READ) {
                goto cleanup;
            }
            //printf("*** HANDSHAKE WANT READ/WRITE detected");
            #ifdef MICROPY_EVENT_POLL_HOOK
            MICROPY_EVENT_POLL_HOOK
            #endif
        }
    }

    return o;

cleanup:
    // get error from SSL object before freeing it
    int err = WOLFSSL_SUCCESS;
    if (o->ssl != NULL) {
        err = wolfSSL_get_error(o->ssl, ret);
    } else {
        err = ret;
    }
    
    wolfSSL_Cleanup();

    switch (err) {
        case ASN_PARSE_E:
            mp_raise_ValueError(MP_ERROR_TEXT("invalid key"));
            break;
        default:
            wolfssl_raise_error(err);
            break;
    }

    wolfSSL_Cleanup();

    // BRN-TODO Python error raising
    //if (ret == MBEDTLS_ERR_SSL_ALLOC_FAILED) {
    //    mp_raise_OSError(MP_ENOMEM);
    //} else if (ret == MBEDTLS_ERR_PK_BAD_INPUT_DATA) {
    //    mp_raise_ValueError(MP_ERROR_TEXT("invalid key"));
    //} else if (ret == MBEDTLS_ERR_X509_BAD_INPUT_DATA) {
    //    mp_raise_ValueError(MP_ERROR_TEXT("invalid cert"));
    //} else {
    //    mbedtls_raise_error(ret);
    //}
}

STATIC mp_obj_t mod_ssl_getpeercert(mp_obj_t o_in, mp_obj_t binary_form) {
    mp_obj_ssl_socket_t *o = MP_OBJ_TO_PTR(o_in);
    if (!mp_obj_is_true(binary_form)) {
        mp_raise_NotImplementedError(NULL);
    }

    WOLFSSL_X509* peer_cert = wolfSSL_get_peer_certificate(o->ssl);

    int cert_der_len = 0;
    const unsigned char * peer_cert_der = wolfSSL_X509_get_der(peer_cert, &cert_der_len);

    if (peer_cert == NULL) {
        return mp_const_none;
    }
    return mp_obj_new_bytes(peer_cert_der, cert_der_len);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mod_ssl_getpeercert_obj, mod_ssl_getpeercert);

STATIC void socket_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    (void)kind;
    mp_obj_ssl_socket_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, "<_SSLSocket %p>", self);
}

STATIC mp_uint_t socket_read(mp_obj_t o_in, void *buf, mp_uint_t size, int *errcode) {
    //printf("**** SOCKET READ\n");
    mp_obj_ssl_socket_t *o = MP_OBJ_TO_PTR(o_in);
    o->poll_mask = 0;

    if (o->last_error) {
        //printf("**** SOCKET READ: o->last_error=true, ret MP_STREAM_ERROR\n");
        *errcode = o->last_error;
        return MP_STREAM_ERROR;
    }

    int ret = wolfSSL_read(o->ssl, buf, size);
    //printf("**** SOCKET READ: wolfSSL_read returned %d\n",ret);
    if (ret > 0) {
        // we read data
        return ret;
    } else {
        // ret==0 will be returned upon failure. This may be caused by a either a clean (close notify alert) shutdown 
        // or just that the peer closed the connection. Call wolfSSL_get_error() for the specific error code. 
        // WOLFSSL_FATAL_ERROR will be returned upon failure when either an error occurred or, when using non-blocking sockets, the 
        // WOLFSSL_ERROR_WANT_READ or WOLFSSL_ERROR_WANT_WRITE error was received and and the application needs to call wolfSSL_read() again.
        // in either case, we only care about handling WANT_READ/WANT_WRITE and everything else is a stream error
        int err = wolfSSL_get_error(o->ssl, ret);
        //printf("**** SOCKET READ: wolfSSL_read error=%d\n",err);
        if (err == WOLFSSL_ERROR_WANT_READ) {
            //printf("**** SOCKET READ: wolfSSL_read error :::: WANT_READ\n");
            ret = MP_EWOULDBLOCK;
        } else if (err == WOLFSSL_ERROR_WANT_WRITE) {
            //printf("**** SOCKET READ: wolfSSL_read error :::: WANT_WRITE\n");
            // If handshake is not finished, read attempt may end up in protocol
            // wanting to write next handshake message. The same may happen with
            // renegotation.
            ret = MP_EWOULDBLOCK;
            o->poll_mask = MP_STREAM_POLL_WR;
        } else {
            //printf("**** SOCKET READ: wolfSSL_read error :::: OTHER\n");
            // If handshake is not finished, read attempt may end up in protocol
            // BRN-TODO (err vs ret here?) 
            o->last_error = err;
        }
        // BRN-TODO (err vs ret here?) 
        *errcode = ret;
        return MP_STREAM_ERROR;
    }
}

STATIC mp_uint_t socket_write(mp_obj_t o_in, const void *buf, mp_uint_t size, int *errcode) {
    //printf("**** SOCKET WRITE\n");
    mp_obj_ssl_socket_t *o = MP_OBJ_TO_PTR(o_in);
    o->poll_mask = 0;

    if (o->last_error) {
        //printf("**** SOCKET WRITE : o->last_error=true, ret MP_STREAM_ERROR\n");
        *errcode = o->last_error;
        return MP_STREAM_ERROR;
    }

    int ret = wolfSSL_write(o->ssl, buf, size);
    //printf("**** SOCKET WRITE: wolfSSL_write returned %d\n",ret);
    if (ret > 0) {
        return ret;
    } else {
        int err = wolfSSL_get_error(o->ssl, ret);
        //printf("**** SOCKET WRITE: wolfSSL_write error=%d\n",err);
        if (err == WOLFSSL_ERROR_WANT_WRITE) {
            //printf("**** SOCKET WRITE: wolfSSL_read error :::: WANT_WRITE\n");
            ret = MP_EWOULDBLOCK;
        } else if (err == WOLFSSL_ERROR_WANT_READ) {
            //printf("**** SOCKET WRITE: wolfSSL_write error :::: WANT_READ\n");
            // If handshake is not finished, write attempt may end up in protocol
            // wanting to read next handshake message. The same may happen with
            // renegotation.
            ret = MP_EWOULDBLOCK;
            o->poll_mask = MP_STREAM_POLL_RD;
        } else {
            //printf("**** SOCKET WRITE: wolfSSL_read error :::: OTHER\n");
            // BRN-TODO (err vs ret here?) 
            o->last_error = err;
        }
        // BRN-TODO (err vs ret here?) 
        *errcode = ret;
        return MP_STREAM_ERROR;
    }
}


STATIC mp_obj_t socket_setblocking(mp_obj_t self_in, mp_obj_t flag_in) {
    mp_obj_ssl_socket_t *o = MP_OBJ_TO_PTR(self_in);
    mp_obj_t sock = o->sock;
    mp_obj_t dest[3];
    mp_load_method(sock, MP_QSTR_setblocking, dest);
    dest[2] = flag_in;
    return mp_call_method_n_kw(1, 0, dest);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(socket_setblocking_obj, socket_setblocking);

STATIC mp_uint_t socket_ioctl(mp_obj_t o_in, mp_uint_t request, uintptr_t arg, int *errcode) {
    mp_obj_ssl_socket_t *self = MP_OBJ_TO_PTR(o_in);
    mp_uint_t ret = 0;

    uintptr_t saved_arg = 0;
    mp_obj_t sock = self->sock;
    if (sock == MP_OBJ_NULL || (request != MP_STREAM_CLOSE && self->last_error != 0)) {
        // Closed or error socket:
        return MP_STREAM_POLL_NVAL;
    }

    if (request == MP_STREAM_CLOSE) {
        self->sock = MP_OBJ_NULL;
        wolfSSL_Cleanup();
    } else if (request == MP_STREAM_POLL) {
        // If the library signaled us that it needs reading or writing, only check that direction,
        // but save what the caller asked because we need to restore it later
        if (self->poll_mask && (arg & MP_STREAM_POLL_RDWR)) {
            saved_arg = arg & MP_STREAM_POLL_RDWR;
            arg = (arg & ~saved_arg) | self->poll_mask;
        }

        // Take into account that the library might have buffered data already
        int has_pending = 0;
        if (arg & MP_STREAM_POLL_RD) {
            has_pending = wolfSSL_pending(self->ssl);
            if (has_pending) {
                ret |= MP_STREAM_POLL_RD;
                if (arg == MP_STREAM_POLL_RD) {
                    // Shortcut if we only need to read and we have buffered data, no need to go to the underlying socket
                    return MP_STREAM_POLL_RD;
                }
            }
        }
    }

    // Pass all requests down to the underlying socket
    ret |= mp_get_stream(sock)->ioctl(sock, request, arg, errcode);

    if (request == MP_STREAM_POLL) {
        // The direction the library needed is available, return a fake result to the caller so that
        // it reenters a read or a write to allow the handshake to progress
        if (ret & self->poll_mask) {
            ret |= saved_arg;
        }
    }
    return ret;
}

STATIC const mp_rom_map_elem_t ussl_socket_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_read), MP_ROM_PTR(&mp_stream_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_readinto), MP_ROM_PTR(&mp_stream_readinto_obj) },
    { MP_ROM_QSTR(MP_QSTR_readline), MP_ROM_PTR(&mp_stream_unbuffered_readline_obj) },
    { MP_ROM_QSTR(MP_QSTR_write), MP_ROM_PTR(&mp_stream_write_obj) },
    { MP_ROM_QSTR(MP_QSTR_setblocking), MP_ROM_PTR(&socket_setblocking_obj) },
    { MP_ROM_QSTR(MP_QSTR_close), MP_ROM_PTR(&mp_stream_close_obj) },
    #if MICROPY_PY_USSL_FINALISER
    { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&mp_stream_close_obj) },
    #endif
//    #if MICROPY_UNIX_COVERAGE
    { MP_ROM_QSTR(MP_QSTR_ioctl), MP_ROM_PTR(&mp_stream_ioctl_obj) },
//    #endif
    { MP_ROM_QSTR(MP_QSTR_getpeercert), MP_ROM_PTR(&mod_ssl_getpeercert_obj) },
};

STATIC MP_DEFINE_CONST_DICT(ussl_socket_locals_dict, ussl_socket_locals_dict_table);

STATIC const mp_stream_p_t ussl_socket_stream_p = {
    .read = socket_read,
    .write = socket_write,
    .ioctl = socket_ioctl,
};

STATIC MP_DEFINE_CONST_OBJ_TYPE(
    ussl_socket_type,
    MP_QSTR_ussl,
    MP_TYPE_FLAG_NONE,
    // Save on qstr's, reuse same as for module
    print, socket_print,
    protocol, &ussl_socket_stream_p,
    locals_dict, &ussl_socket_locals_dict
    );

STATIC mp_obj_t mod_ssl_wrap_socket(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    // TODO: Implement more args
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_key, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_cert, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_server_side, MP_ARG_KW_ONLY | MP_ARG_BOOL, {.u_bool = false} },
        { MP_QSTR_server_hostname, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_cert_reqs, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = WOLFSSL_VERIFY_NONE}},
        { MP_QSTR_cadata, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_do_handshake, MP_ARG_KW_ONLY | MP_ARG_BOOL, {.u_bool = true} },
    };

    // TODO: Check that sock implements stream protocol
    mp_obj_t sock = pos_args[0];

    struct ssl_args args;
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args,
        MP_ARRAY_SIZE(allowed_args), allowed_args, (mp_arg_val_t *)&args);

    return MP_OBJ_FROM_PTR(socket_new(sock, &args));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mod_ssl_wrap_socket_obj, 1, mod_ssl_wrap_socket);

STATIC const mp_rom_map_elem_t mp_module_ssl_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_ussl) },
    { MP_ROM_QSTR(MP_QSTR_wrap_socket), MP_ROM_PTR(&mod_ssl_wrap_socket_obj) },
    { MP_ROM_QSTR(MP_QSTR_CERT_NONE), MP_ROM_INT(WOLFSSL_VERIFY_NONE) },
    // BRN-TODO removing optional verification okay?
    // { MP_ROM_QSTR(MP_QSTR_CERT_OPTIONAL), MP_ROM_INT(WOLFSSL_VERIFY_OPTIONAL) },
    { MP_ROM_QSTR(MP_QSTR_CERT_REQUIRED), MP_ROM_INT(WOLFSSL_VERIFY_PEER) },
};

STATIC MP_DEFINE_CONST_DICT(mp_module_ssl_globals, mp_module_ssl_globals_table);

const mp_obj_module_t mp_module_ussl = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&mp_module_ssl_globals,
};

MP_REGISTER_MODULE(MP_QSTR_ussl, mp_module_ussl);

#endif // !MICROPY_PY_USSL && MICROPY_PY_WOLFSSL_USSL
