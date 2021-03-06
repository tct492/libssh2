/* Copyright (C) 2009, 2010 Simon Josefsson
 * Copyright (C) 2006, 2007 The Written Word, Inc.  All rights reserved.
 * Copyright (c) 2004-2006, Sara Golemon <sarag@libssh2.org>
 *
 * Author: Simon Josefsson
 *
 * Redistribution and use in source and binary forms,
 * with or without modification, are permitted provided
 * that the following conditions are met:
 *
 *   Redistributions of source code must retain the above
 *   copyright notice, this list of conditions and the
 *   following disclaimer.
 *
 *   Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials
 *   provided with the distribution.
 *
 *   Neither the name of the copyright holder nor the names
 *   of any other contributors may be used to endorse or
 *   promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

#include "libssh2_priv.h"

#ifdef LIBSSH2_OPENSSL /* compile only if we build with openssl */

#include <string.h>
#include "misc.h"

#ifndef EVP_MAX_BLOCK_LENGTH
#define EVP_MAX_BLOCK_LENGTH 32
#endif

static unsigned char *
write_bn(unsigned char *buf, const BIGNUM *bn, int bn_bytes)
{
    unsigned char *p = buf;

    /* Left space for bn size which will be written below. */
    p += 4;

    *p = 0;
    BN_bn2bin(bn, p + 1);
    if(!(*(p + 1) & 0x80)) {
        memmove(p, p + 1, --bn_bytes);
    }
    _libssh2_htonu32(p - 4, bn_bytes);  /* Post write bn size. */

    return p + bn_bytes;
}

int
_libssh2_rsa_new(libssh2_rsa_ctx ** rsa,
                 const unsigned char *edata,
                 unsigned long elen,
                 const unsigned char *ndata,
                 unsigned long nlen,
                 const unsigned char *ddata,
                 unsigned long dlen,
                 const unsigned char *pdata,
                 unsigned long plen,
                 const unsigned char *qdata,
                 unsigned long qlen,
                 const unsigned char *e1data,
                 unsigned long e1len,
                 const unsigned char *e2data,
                 unsigned long e2len,
                 const unsigned char *coeffdata, unsigned long coefflen)
{
    BIGNUM * e;
    BIGNUM * n;
    BIGNUM * d = 0;
    BIGNUM * p = 0;
    BIGNUM * q = 0;
    BIGNUM * dmp1 = 0;
    BIGNUM * dmq1 = 0;
    BIGNUM * iqmp = 0;

    e = BN_new();
    BN_bin2bn(edata, elen, e);

    n = BN_new();
    BN_bin2bn(ndata, nlen, n);

    if(ddata) {
        d = BN_new();
        BN_bin2bn(ddata, dlen, d);

        p = BN_new();
        BN_bin2bn(pdata, plen, p);

        q = BN_new();
        BN_bin2bn(qdata, qlen, q);

        dmp1 = BN_new();
        BN_bin2bn(e1data, e1len, dmp1);

        dmq1 = BN_new();
        BN_bin2bn(e2data, e2len, dmq1);

        iqmp = BN_new();
        BN_bin2bn(coeffdata, coefflen, iqmp);
    }

    *rsa = RSA_new();
#ifdef HAVE_OPAQUE_STRUCTS
    RSA_set0_key(*rsa, n, e, d);
#else
    (*rsa)->e = e;
    (*rsa)->n = n;
    (*rsa)->d = d;
#endif

#ifdef HAVE_OPAQUE_STRUCTS
    RSA_set0_factors(*rsa, p, q);
#else
    (*rsa)->p = p;
    (*rsa)->q = q;
#endif

#ifdef HAVE_OPAQUE_STRUCTS
    RSA_set0_crt_params(*rsa, dmp1, dmq1, iqmp);
#else
    (*rsa)->dmp1 = dmp1;
    (*rsa)->dmq1 = dmq1;
    (*rsa)->iqmp = iqmp;
#endif
    return 0;
}

int
_libssh2_rsa_sha1_verify(libssh2_rsa_ctx * rsactx,
                         const unsigned char *sig,
                         unsigned long sig_len,
                         const unsigned char *m, unsigned long m_len)
{
    unsigned char hash[SHA_DIGEST_LENGTH];
    int ret;

    if(_libssh2_sha1(m, m_len, hash))
        return -1; /* failure */
    ret = RSA_verify(NID_sha1, hash, SHA_DIGEST_LENGTH,
                     (unsigned char *) sig, sig_len, rsactx);
    return (ret == 1) ? 0 : -1;
}

#if LIBSSH2_DSA
int
_libssh2_dsa_new(libssh2_dsa_ctx ** dsactx,
                 const unsigned char *p,
                 unsigned long p_len,
                 const unsigned char *q,
                 unsigned long q_len,
                 const unsigned char *g,
                 unsigned long g_len,
                 const unsigned char *y,
                 unsigned long y_len,
                 const unsigned char *x, unsigned long x_len)
{
    BIGNUM * p_bn;
    BIGNUM * q_bn;
    BIGNUM * g_bn;
    BIGNUM * pub_key;
    BIGNUM * priv_key = NULL;

    p_bn = BN_new();
    BN_bin2bn(p, p_len, p_bn);

    q_bn = BN_new();
    BN_bin2bn(q, q_len, q_bn);

    g_bn = BN_new();
    BN_bin2bn(g, g_len, g_bn);

    pub_key = BN_new();
    BN_bin2bn(y, y_len, pub_key);

    if(x_len) {
        priv_key = BN_new();
        BN_bin2bn(x, x_len, priv_key);
    }

    *dsactx = DSA_new();

#ifdef HAVE_OPAQUE_STRUCTS
    DSA_set0_pqg(*dsactx, p_bn, q_bn, g_bn);
#else
    (*dsactx)->p = p_bn;
    (*dsactx)->g = g_bn;
    (*dsactx)->q = q_bn;
#endif

#ifdef HAVE_OPAQUE_STRUCTS
    DSA_set0_key(*dsactx, pub_key, priv_key);
#else
    (*dsactx)->pub_key = pub_key;
    (*dsactx)->priv_key = priv_key;
#endif
    return 0;
}

int
_libssh2_dsa_sha1_verify(libssh2_dsa_ctx * dsactx,
                         const unsigned char *sig,
                         const unsigned char *m, unsigned long m_len)
{
    unsigned char hash[SHA_DIGEST_LENGTH];
    DSA_SIG * dsasig;
    BIGNUM * r;
    BIGNUM * s;
    int ret = -1;

    r = BN_new();
    BN_bin2bn(sig, 20, r);
    s = BN_new();
    BN_bin2bn(sig + 20, 20, s);

    dsasig = DSA_SIG_new();
#ifdef HAVE_OPAQUE_STRUCTS
    DSA_SIG_set0(dsasig, r, s);
#else
    dsasig->r = r;
    dsasig->s = s;
#endif
    if(!_libssh2_sha1(m, m_len, hash))
        /* _libssh2_sha1() succeeded */
        ret = DSA_do_verify(hash, SHA_DIGEST_LENGTH, dsasig, dsactx);

    DSA_SIG_free(dsasig);

    return (ret == 1) ? 0 : -1;
}
#endif /* LIBSSH_DSA */

#if LIBSSH2_ECDSA

/* _libssh2_ecdsa_key_get_curve_type
 *
 * returns key curve type that maps to libssh2_curve_type
 *
 */

libssh2_curve_type
_libssh2_ecdsa_key_get_curve_type(_libssh2_ec_key *key)
{
    const EC_GROUP *group = EC_KEY_get0_group(key);
    return EC_GROUP_get_curve_name(group);
}

/* _libssh2_ecdsa_curve_type_from_name
 *
 * returns 0 for success, key curve type that maps to libssh2_curve_type
 *
 */

int
_libssh2_ecdsa_curve_type_from_name(const char *name, libssh2_curve_type *out_type)
{
    int ret = 0;
    libssh2_curve_type type;

    if(name == NULL || strlen(name) != 19)
        return -1;

    if(strcmp(name, "ecdsa-sha2-nistp256") == 0)
        type = LIBSSH2_EC_CURVE_NISTP256;
    else if(strcmp(name, "ecdsa-sha2-nistp384") == 0)
        type = LIBSSH2_EC_CURVE_NISTP384;
    else if(strcmp(name, "ecdsa-sha2-nistp521") == 0)
        type = LIBSSH2_EC_CURVE_NISTP521;
    else {
        ret = -1;
    }

    if(ret == 0 && out_type) {
        *out_type = type;
    }

    return ret;
}

/* _libssh2_ecdsa_curve_name_with_octal_new
 *
 * Creates a new public key given an octal string, length and type
 *
 */

int
_libssh2_ecdsa_curve_name_with_octal_new(libssh2_ecdsa_ctx ** ec_ctx,
     const unsigned char *k,
     size_t k_len, libssh2_curve_type curve)
{

    int ret = 0;
    const EC_GROUP *ec_group = NULL;
    EC_KEY *ec_key = EC_KEY_new_by_curve_name(curve);
    EC_POINT *point = NULL;

    if(ec_key) {
        ec_group = EC_KEY_get0_group(ec_key);
        point = EC_POINT_new(ec_group);
        ret = EC_POINT_oct2point(ec_group, point, k, k_len, NULL);
        ret = EC_KEY_set_public_key(ec_key, point);

        if(point != NULL)
            EC_POINT_free(point);

        if(ec_ctx != NULL)
            *ec_ctx = ec_key;
    }

    return (ret == 1) ? 0 : -1;
}

#define LIBSSH2_ECDSA_VERIFY(digest_type)                           \
{                                                                   \
    unsigned char hash[SHA##digest_type##_DIGEST_LENGTH];           \
    libssh2_sha##digest_type(m, m_len, hash);                       \
    ret = ECDSA_do_verify(hash, SHA##digest_type##_DIGEST_LENGTH,   \
      ecdsa_sig, ec_key);                                           \
                                                                    \
}

int
_libssh2_ecdsa_verify(libssh2_ecdsa_ctx * ctx,
      const unsigned char *r, size_t r_len,
      const unsigned char *s, size_t s_len,
      const unsigned char *m, size_t m_len)
{
    int ret = 0;
    EC_KEY *ec_key = (EC_KEY*)ctx;
    libssh2_curve_type type = _libssh2_ecdsa_key_get_curve_type(ec_key);

#ifdef HAVE_OPAQUE_STRUCTS
    ECDSA_SIG *ecdsa_sig = ECDSA_SIG_new();
    BIGNUM *pr = BN_new();
    BIGNUM *ps = BN_new();

    BN_bin2bn(r, r_len, pr);
    BN_bin2bn(s, s_len, ps);
    ECDSA_SIG_set0(ecdsa_sig, pr, ps);

#else
    ECDSA_SIG ecdsa_sig_;
    ECDSA_SIG *ecdsa_sig = &ecdsa_sig_;
    ecdsa_sig_.r = BN_new();
    BN_bin2bn(r, r_len, ecdsa_sig_.r);
    ecdsa_sig_.s = BN_new();
    BN_bin2bn(s, s_len, ecdsa_sig_.s);
#endif

    if(type == LIBSSH2_EC_CURVE_NISTP256) {
        LIBSSH2_ECDSA_VERIFY(256);
    }
    else if(type == LIBSSH2_EC_CURVE_NISTP384) {
        LIBSSH2_ECDSA_VERIFY(384);
    }
    else if(type == LIBSSH2_EC_CURVE_NISTP521) {
        LIBSSH2_ECDSA_VERIFY(512);
    }

#ifdef HAVE_OPAQUE_STRUCTS
    if(ecdsa_sig)
        ECDSA_SIG_free(ecdsa_sig);
#else
    BN_clear_free(ecdsa_sig_.s);
    BN_clear_free(ecdsa_sig_.r);
#endif

    return (ret == 1) ? 0 : -1;
}

#endif /* LIBSSH2_ECDSA */

int
_libssh2_cipher_init(_libssh2_cipher_ctx * h,
                     _libssh2_cipher_type(algo),
                     unsigned char *iv, unsigned char *secret, int encrypt)
{
#ifdef HAVE_OPAQUE_STRUCTS
    *h = EVP_CIPHER_CTX_new();
    return !EVP_CipherInit(*h, algo(), secret, iv, encrypt);
#else
    EVP_CIPHER_CTX_init(h);
    return !EVP_CipherInit(h, algo(), secret, iv, encrypt);
#endif
}

int
_libssh2_cipher_crypt(_libssh2_cipher_ctx * ctx,
                      _libssh2_cipher_type(algo),
                      int encrypt, unsigned char *block, size_t blocksize)
{
    unsigned char buf[EVP_MAX_BLOCK_LENGTH];
    int ret;
    (void) algo;
    (void) encrypt;

#ifdef HAVE_OPAQUE_STRUCTS
    ret = EVP_Cipher(*ctx, buf, block, blocksize);
#else
    ret = EVP_Cipher(ctx, buf, block, blocksize);
#endif
    if(ret == 1) {
        memcpy(block, buf, blocksize);
    }
    return ret == 1 ? 0 : 1;
}

#if LIBSSH2_AES_CTR && !defined(HAVE_EVP_AES_128_CTR)

#include <openssl/aes.h>
#include <openssl/evp.h>

typedef struct
{
    AES_KEY       key;
    EVP_CIPHER_CTX *aes_ctx;
    unsigned char ctr[AES_BLOCK_SIZE];
} aes_ctr_ctx;

static int
aes_ctr_init(EVP_CIPHER_CTX *ctx, const unsigned char *key,
             const unsigned char *iv, int enc) /* init key */
{
    /*
     * variable "c" is leaked from this scope, but is later freed
     * in aes_ctr_cleanup
     */
    aes_ctr_ctx *c;
    const EVP_CIPHER *aes_cipher;
    (void) enc;

    switch(EVP_CIPHER_CTX_key_length(ctx)) {
    case 16:
        aes_cipher = EVP_aes_128_ecb();
        break;
    case 24:
        aes_cipher = EVP_aes_192_ecb();
        break;
    case 32:
        aes_cipher = EVP_aes_256_ecb();
        break;
    default:
        return 0;
    }

    c = malloc(sizeof(*c));
    if(c == NULL)
        return 0;

#ifdef HAVE_OPAQUE_STRUCTS
    c->aes_ctx = EVP_CIPHER_CTX_new();
#else
    c->aes_ctx = malloc(sizeof(EVP_CIPHER_CTX));
#endif
    if(c->aes_ctx == NULL) {
        free(c);
        return 0;
    }

    if(EVP_EncryptInit(c->aes_ctx, aes_cipher, key, NULL) != 1) {
#ifdef HAVE_OPAQUE_STRUCTS
        EVP_CIPHER_CTX_free(c->aes_ctx);
#else
        free(c->aes_ctx);
#endif
        free(c);
        return 0;
    }

    EVP_CIPHER_CTX_set_padding(c->aes_ctx, 0);

    memcpy(c->ctr, iv, AES_BLOCK_SIZE);

    EVP_CIPHER_CTX_set_app_data(ctx, c);

    return 1;
}

static int
aes_ctr_do_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
                  const unsigned char *in,
                  size_t inl) /* encrypt/decrypt data */
{
    aes_ctr_ctx *c = EVP_CIPHER_CTX_get_app_data(ctx);
    unsigned char b1[AES_BLOCK_SIZE];
    int outlen = 0;

    if(inl != 16) /* libssh2 only ever encrypt one block */
        return 0;

    if(c == NULL) {
        return 0;
    }

/*
  To encrypt a packet P=P1||P2||...||Pn (where P1, P2, ..., Pn are each
  blocks of length L), the encryptor first encrypts <X> with <cipher>
  to obtain a block B1.  The block B1 is then XORed with P1 to generate
  the ciphertext block C1.  The counter X is then incremented
*/

    if(EVP_EncryptUpdate(c->aes_ctx, b1, &outlen, c->ctr, AES_BLOCK_SIZE) != 1) {
        return 0;
    }

    _libssh2_xor_data(out, in, b1, AES_BLOCK_SIZE);
    _libssh2_aes_ctr_increment(c->ctr, AES_BLOCK_SIZE);

    return 1;
}

static int
aes_ctr_cleanup(EVP_CIPHER_CTX *ctx) /* cleanup ctx */
{
    aes_ctr_ctx *c = EVP_CIPHER_CTX_get_app_data(ctx);

    if(c == NULL) {
        return 1;
    }

    if(c->aes_ctx != NULL) {
#ifdef HAVE_OPAQUE_STRUCTS
        EVP_CIPHER_CTX_free(c->aes_ctx);
#else
        _libssh2_cipher_dtor(c->aes_ctx);
        free(c->aes_ctx);
#endif
    }

    free(c);

    return 1;
}

static const EVP_CIPHER *
make_ctr_evp (size_t keylen, EVP_CIPHER *aes_ctr_cipher, int type)
{
#ifdef HAVE_OPAQUE_STRUCTS
    aes_ctr_cipher = EVP_CIPHER_meth_new(type, 16, keylen);
    if(aes_ctr_cipher) {
        EVP_CIPHER_meth_set_iv_length(aes_ctr_cipher, 16);
        EVP_CIPHER_meth_set_init(aes_ctr_cipher, aes_ctr_init);
        EVP_CIPHER_meth_set_do_cipher(aes_ctr_cipher, aes_ctr_do_cipher);
        EVP_CIPHER_meth_set_cleanup(aes_ctr_cipher, aes_ctr_cleanup);
    }
#else
    aes_ctr_cipher->nid = type;
    aes_ctr_cipher->block_size = 16;
    aes_ctr_cipher->key_len = keylen;
    aes_ctr_cipher->iv_len = 16;
    aes_ctr_cipher->init = aes_ctr_init;
    aes_ctr_cipher->do_cipher = aes_ctr_do_cipher;
    aes_ctr_cipher->cleanup = aes_ctr_cleanup;
#endif

    return aes_ctr_cipher;
}

const EVP_CIPHER *
_libssh2_EVP_aes_128_ctr(void)
{
#ifdef HAVE_OPAQUE_STRUCTS
    static EVP_CIPHER * aes_ctr_cipher;
    return !aes_ctr_cipher ?
        make_ctr_evp(16, aes_ctr_cipher, NID_aes_128_ctr) : aes_ctr_cipher;
#else
    static EVP_CIPHER aes_ctr_cipher;
    return !aes_ctr_cipher.key_len ?
        make_ctr_evp(16, &aes_ctr_cipher, 0) : &aes_ctr_cipher;
#endif
}

const EVP_CIPHER *
_libssh2_EVP_aes_192_ctr(void)
{
#ifdef HAVE_OPAQUE_STRUCTS
    static EVP_CIPHER * aes_ctr_cipher;
    return !aes_ctr_cipher ?
        make_ctr_evp(24, aes_ctr_cipher, NID_aes_192_ctr) : aes_ctr_cipher;
#else
    static EVP_CIPHER aes_ctr_cipher;
    return !aes_ctr_cipher.key_len ?
        make_ctr_evp(24, &aes_ctr_cipher, 0) : &aes_ctr_cipher;
#endif
}

const EVP_CIPHER *
_libssh2_EVP_aes_256_ctr(void)
{
#ifdef HAVE_OPAQUE_STRUCTS
    static EVP_CIPHER * aes_ctr_cipher;
    return !aes_ctr_cipher ?
        make_ctr_evp(32, aes_ctr_cipher, NID_aes_256_ctr) : aes_ctr_cipher;
#else
    static EVP_CIPHER aes_ctr_cipher;
    return !aes_ctr_cipher.key_len ?
        make_ctr_evp(32, &aes_ctr_cipher, 0) : &aes_ctr_cipher;
#endif
}

#endif /* LIBSSH2_AES_CTR */

void _libssh2_openssl_crypto_init(void)
{
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
    ENGINE_load_builtin_engines();
    ENGINE_register_all_complete();
#else
    OpenSSL_add_all_algorithms();
    OpenSSL_add_all_ciphers();
    ENGINE_load_builtin_engines();
    ENGINE_register_all_complete();
#endif
#ifndef HAVE_EVP_AES_128_CTR
    _libssh2_EVP_aes_128_ctr();
    _libssh2_EVP_aes_192_ctr();
    _libssh2_EVP_aes_256_ctr();
#endif
}

/* TODO: Optionally call a passphrase callback specified by the
 * calling program
 */
static int
passphrase_cb(char *buf, int size, int rwflag, char *passphrase)
{
    int passphrase_len = strlen(passphrase);
    (void) rwflag;

    if(passphrase_len > (size - 1)) {
        passphrase_len = size - 1;
    }
    memcpy(buf, passphrase, passphrase_len);
    buf[passphrase_len] = '\0';

    return passphrase_len;
}

typedef void * (*pem_read_bio_func)(BIO *, void **, pem_password_cb *,
                                    void *u);

static int
read_private_key_from_memory(void **key_ctx,
                             pem_read_bio_func read_private_key,
                             const char *filedata,
                             size_t filedata_len,
                             unsigned const char *passphrase)
{
    BIO * bp;

    *key_ctx = NULL;

    bp = BIO_new_mem_buf((char *)filedata, filedata_len);
    if(!bp) {
        return -1;
    }
    *key_ctx = read_private_key(bp, NULL, (pem_password_cb *) passphrase_cb,
                                (void *) passphrase);

    BIO_free(bp);
    return (*key_ctx) ? 0 : -1;
}

static int
read_private_key_from_file(void **key_ctx,
                           pem_read_bio_func read_private_key,
                           const char *filename,
                           unsigned const char *passphrase)
{
    BIO * bp;

    *key_ctx = NULL;

    bp = BIO_new_file(filename, "r");
    if(!bp) {
        return -1;
    }

    *key_ctx = read_private_key(bp, NULL, (pem_password_cb *) passphrase_cb,
                                (void *) passphrase);

    BIO_free(bp);
    return (*key_ctx) ? 0 : -1;
}

int
_libssh2_rsa_new_private_frommemory(libssh2_rsa_ctx ** rsa,
                                    LIBSSH2_SESSION * session,
                                    const char *filedata, size_t filedata_len,
                                    unsigned const char *passphrase)
{
    pem_read_bio_func read_rsa =
        (pem_read_bio_func) &PEM_read_bio_RSAPrivateKey;
    (void) session;

    _libssh2_init_if_needed();

    return read_private_key_from_memory((void **) rsa, read_rsa,
                                        filedata, filedata_len, passphrase);
}

int
_libssh2_rsa_new_private(libssh2_rsa_ctx ** rsa,
                         LIBSSH2_SESSION * session,
                         const char *filename, unsigned const char *passphrase)
{
    pem_read_bio_func read_rsa =
        (pem_read_bio_func) &PEM_read_bio_RSAPrivateKey;
    (void) session;

    _libssh2_init_if_needed();

    return read_private_key_from_file((void **) rsa, read_rsa,
                                      filename, passphrase);
}

#if LIBSSH2_DSA
int
_libssh2_dsa_new_private_frommemory(libssh2_dsa_ctx ** dsa,
                                    LIBSSH2_SESSION * session,
                                    const char *filedata, size_t filedata_len,
                                    unsigned const char *passphrase)
{
    pem_read_bio_func read_dsa =
        (pem_read_bio_func) &PEM_read_bio_DSAPrivateKey;
    (void) session;

    _libssh2_init_if_needed();

    return read_private_key_from_memory((void **) dsa, read_dsa,
                                        filedata, filedata_len, passphrase);
}

int
_libssh2_dsa_new_private(libssh2_dsa_ctx ** dsa,
                         LIBSSH2_SESSION * session,
                         const char *filename, unsigned const char *passphrase)
{
    pem_read_bio_func read_dsa =
        (pem_read_bio_func) &PEM_read_bio_DSAPrivateKey;
    (void) session;

    _libssh2_init_if_needed();

    return read_private_key_from_file((void **) dsa, read_dsa,
                                      filename, passphrase);
}
#endif /* LIBSSH_DSA */

#if LIBSSH2_ECDSA

int
_libssh2_ecdsa_new_private_frommemory(libssh2_ecdsa_ctx ** ec_ctx,
                                    LIBSSH2_SESSION * session,
                                    const char *filedata, size_t filedata_len,
                                    unsigned const char *passphrase)
{
    pem_read_bio_func read_ec =
        (pem_read_bio_func) &PEM_read_bio_ECPrivateKey;
    (void) session;

    _libssh2_init_if_needed();

    return read_private_key_from_memory((void **) ec_ctx, read_ec,
                                        filedata, filedata_len, passphrase);
}

int
_libssh2_ecdsa_new_private(libssh2_ecdsa_ctx ** ec_ctx,
       LIBSSH2_SESSION * session,
       const char *filename, unsigned const char *passphrase)
{
    pem_read_bio_func read_ec = (pem_read_bio_func) &PEM_read_bio_ECPrivateKey;
    (void) session;

    _libssh2_init_if_needed();

    return read_private_key_from_file((void **) ec_ctx, read_ec,
      filename, passphrase);
}

#endif /* LIBSSH2_ECDSA */


int
_libssh2_rsa_sha1_sign(LIBSSH2_SESSION * session,
                       libssh2_rsa_ctx * rsactx,
                       const unsigned char *hash,
                       size_t hash_len,
                       unsigned char **signature, size_t *signature_len)
{
    int ret;
    unsigned char *sig;
    unsigned int sig_len;

    sig_len = RSA_size(rsactx);
    sig = LIBSSH2_ALLOC(session, sig_len);

    if(!sig) {
        return -1;
    }

    ret = RSA_sign(NID_sha1, hash, hash_len, sig, &sig_len, rsactx);

    if(!ret) {
        LIBSSH2_FREE(session, sig);
        return -1;
    }

    *signature = sig;
    *signature_len = sig_len;

    return 0;
}

#if LIBSSH2_DSA
int
_libssh2_dsa_sha1_sign(libssh2_dsa_ctx * dsactx,
                       const unsigned char *hash,
                       unsigned long hash_len, unsigned char *signature)
{
    DSA_SIG *sig;
    const BIGNUM * r;
    const BIGNUM * s;
    int r_len, s_len;
    (void) hash_len;

    sig = DSA_do_sign(hash, SHA_DIGEST_LENGTH, dsactx);
    if(!sig) {
        return -1;
    }

#ifdef HAVE_OPAQUE_STRUCTS
    DSA_SIG_get0(sig, &r, &s);
#else
    r = sig->r;
    s = sig->s;
#endif
    r_len = BN_num_bytes(r);
    if(r_len < 1 || r_len > 20) {
        DSA_SIG_free(sig);
        return -1;
    }
    s_len = BN_num_bytes(s);
    if(s_len < 1 || s_len > 20) {
        DSA_SIG_free(sig);
        return -1;
    }

    memset(signature, 0, 40);

    BN_bn2bin(r, signature + (20 - r_len));
    BN_bn2bin(s, signature + 20 + (20 - s_len));

    DSA_SIG_free(sig);

    return 0;
}
#endif /* LIBSSH_DSA */

#if LIBSSH2_ECDSA

int
_libssh2_ecdsa_sign(LIBSSH2_SESSION * session, libssh2_ecdsa_ctx * ec_ctx,
    const unsigned char *hash, unsigned long hash_len,
    unsigned char **signature, size_t *signature_len)
{
    int r_len, s_len;
    int rc = 0;
    size_t out_buffer_len = 0;
    unsigned char *sp;
    const BIGNUM *pr = NULL, *ps = NULL;
    unsigned char *temp_buffer = NULL;
    unsigned char *out_buffer = NULL;

    ECDSA_SIG *sig = ECDSA_do_sign(hash, hash_len, ec_ctx);
    if(sig == NULL)
        return -1;
#ifdef HAVE_OPAQUE_STRUCTS
    ECDSA_SIG_get0(sig, &pr, &ps);
#else
    pr = sig->r;
    ps = sig->s;
#endif

    r_len = BN_num_bytes(pr) + 1;
    s_len = BN_num_bytes(ps) + 1;

    temp_buffer = malloc(r_len + s_len + 8);
    if(temp_buffer == NULL) {
        rc = -1;
        goto clean_exit;
    }

    sp = temp_buffer;
    sp = write_bn(sp, pr, r_len);
    sp = write_bn(sp, ps, s_len);

    out_buffer_len = (size_t)(sp - temp_buffer);

    out_buffer = LIBSSH2_CALLOC(session, out_buffer_len);
    if(out_buffer == NULL) {
        rc = -1;
        goto clean_exit;
    }

    memcpy(out_buffer, temp_buffer, out_buffer_len);

    *signature = out_buffer;
    *signature_len = out_buffer_len;

clean_exit:

    if(temp_buffer != NULL)
        free(temp_buffer);

    if(sig)
        ECDSA_SIG_free(sig);

    return rc;
}
#endif /* LIBSSH2_ECDSA */

int
_libssh2_sha1_init(libssh2_sha1_ctx *ctx)
{
#ifdef HAVE_OPAQUE_STRUCTS
    *ctx = EVP_MD_CTX_new();

    if(*ctx == NULL)
        return 0;

    if(EVP_DigestInit(*ctx, EVP_get_digestbyname("sha1")))
        return 1;

    EVP_MD_CTX_free(*ctx);
    *ctx = NULL;

    return 0;
#else
    EVP_MD_CTX_init(ctx);
    return EVP_DigestInit(ctx, EVP_get_digestbyname("sha1"));
#endif
}

int
_libssh2_sha1(const unsigned char *message, unsigned long len,
              unsigned char *out)
{
#ifdef HAVE_OPAQUE_STRUCTS
    EVP_MD_CTX * ctx = EVP_MD_CTX_new();

    if(ctx == NULL)
        return 1; /* error */

    if(EVP_DigestInit(ctx, EVP_get_digestbyname("sha1"))) {
        EVP_DigestUpdate(ctx, message, len);
        EVP_DigestFinal(ctx, out, NULL);
        EVP_MD_CTX_free(ctx);
        return 0; /* success */
    }
    EVP_MD_CTX_free(ctx);
#else
    EVP_MD_CTX ctx;

    EVP_MD_CTX_init(&ctx);
    if(EVP_DigestInit(&ctx, EVP_get_digestbyname("sha1"))) {
        EVP_DigestUpdate(&ctx, message, len);
        EVP_DigestFinal(&ctx, out, NULL);
        return 0; /* success */
    }
#endif
    return 1; /* error */
}

int
_libssh2_sha256_init(libssh2_sha256_ctx *ctx)
{
#ifdef HAVE_OPAQUE_STRUCTS
    *ctx = EVP_MD_CTX_new();

    if(*ctx == NULL)
        return 0;

    if(EVP_DigestInit(*ctx, EVP_get_digestbyname("sha256")))
        return 1;

    EVP_MD_CTX_free(*ctx);
    *ctx = NULL;

    return 0;
#else
    EVP_MD_CTX_init(ctx);
    return EVP_DigestInit(ctx, EVP_get_digestbyname("sha256"));
#endif
}

int
_libssh2_sha256(const unsigned char *message, unsigned long len,
                unsigned char *out)
{
#ifdef HAVE_OPAQUE_STRUCTS
    EVP_MD_CTX * ctx = EVP_MD_CTX_new();

    if(ctx == NULL)
        return 1; /* error */

    if(EVP_DigestInit(ctx, EVP_get_digestbyname("sha256"))) {
        EVP_DigestUpdate(ctx, message, len);
        EVP_DigestFinal(ctx, out, NULL);
        EVP_MD_CTX_free(ctx);
        return 0; /* success */
    }
    EVP_MD_CTX_free(ctx);
#else
    EVP_MD_CTX ctx;

    EVP_MD_CTX_init(&ctx);
    if(EVP_DigestInit(&ctx, EVP_get_digestbyname("sha256"))) {
        EVP_DigestUpdate(&ctx, message, len);
        EVP_DigestFinal(&ctx, out, NULL);
        return 0; /* success */
    }
#endif
    return 1; /* error */
}

int
_libssh2_sha384_init(libssh2_sha384_ctx *ctx)
{
#ifdef HAVE_OPAQUE_STRUCTS
    *ctx = EVP_MD_CTX_new();

    if(*ctx == NULL)
        return 0;

    if(EVP_DigestInit(*ctx, EVP_get_digestbyname("sha384")))
        return 1;

    EVP_MD_CTX_free(*ctx);
    *ctx = NULL;

    return 0;
#else
    EVP_MD_CTX_init(ctx);
    return EVP_DigestInit(ctx, EVP_get_digestbyname("sha384"));
#endif
}

int
_libssh2_sha384(const unsigned char *message, unsigned long len,
    unsigned char *out)
{
#ifdef HAVE_OPAQUE_STRUCTS
    EVP_MD_CTX * ctx = EVP_MD_CTX_new();

    if(ctx == NULL)
        return 1; /* error */

    if(EVP_DigestInit(ctx, EVP_get_digestbyname("sha384"))) {
        EVP_DigestUpdate(ctx, message, len);
        EVP_DigestFinal(ctx, out, NULL);
        EVP_MD_CTX_free(ctx);
        return 0; /* success */
    }
    EVP_MD_CTX_free(ctx);
#else
    EVP_MD_CTX ctx;

    EVP_MD_CTX_init(&ctx);
    if(EVP_DigestInit(&ctx, EVP_get_digestbyname("sha384"))) {
        EVP_DigestUpdate(&ctx, message, len);
        EVP_DigestFinal(&ctx, out, NULL);
        return 0; /* success */
    }
#endif
    return 1; /* error */
}

int
_libssh2_sha512_init(libssh2_sha512_ctx *ctx)
{
#ifdef HAVE_OPAQUE_STRUCTS
    *ctx = EVP_MD_CTX_new();

    if(*ctx == NULL)
        return 0;

    if(EVP_DigestInit(*ctx, EVP_get_digestbyname("sha512")))
        return 1;

    EVP_MD_CTX_free(*ctx);
    *ctx = NULL;

    return 0;
#else
    EVP_MD_CTX_init(ctx);
    return EVP_DigestInit(ctx, EVP_get_digestbyname("sha512"));
#endif
}

int
_libssh2_sha512(const unsigned char *message, unsigned long len,
    unsigned char *out)
{
#ifdef HAVE_OPAQUE_STRUCTS
    EVP_MD_CTX * ctx = EVP_MD_CTX_new();

    if(ctx == NULL)
        return 1; /* error */

    if(EVP_DigestInit(ctx, EVP_get_digestbyname("sha512"))) {
        EVP_DigestUpdate(ctx, message, len);
        EVP_DigestFinal(ctx, out, NULL);
        EVP_MD_CTX_free(ctx);
        return 0; /* success */
    }
    EVP_MD_CTX_free(ctx);
#else
    EVP_MD_CTX ctx;

    EVP_MD_CTX_init(&ctx);
    if(EVP_DigestInit(&ctx, EVP_get_digestbyname("sha512"))) {
        EVP_DigestUpdate(&ctx, message, len);
        EVP_DigestFinal(&ctx, out, NULL);
        return 0; /* success */
    }
#endif
    return 1; /* error */
}

int
_libssh2_md5_init(libssh2_md5_ctx *ctx)
{
#ifdef HAVE_OPAQUE_STRUCTS
    *ctx = EVP_MD_CTX_new();

    if(*ctx == NULL)
        return 0;

    if(EVP_DigestInit(*ctx, EVP_get_digestbyname("md5")))
        return 1;

    EVP_MD_CTX_free(*ctx);
    *ctx = NULL;

    return 0;
#else
    EVP_MD_CTX_init(ctx);
    return EVP_DigestInit(ctx, EVP_get_digestbyname("md5"));
#endif
}

static unsigned char *
gen_publickey_from_rsa(LIBSSH2_SESSION *session, RSA *rsa,
                       size_t *key_len)
{
    int            e_bytes, n_bytes;
    unsigned long  len;
    unsigned char *key;
    unsigned char *p;
    const BIGNUM * e;
    const BIGNUM * n;
#ifdef HAVE_OPAQUE_STRUCTS
    RSA_get0_key(rsa, &n, &e, NULL);
#else
    e = rsa->e;
    n = rsa->n;
#endif
    e_bytes = BN_num_bytes(e) + 1;
    n_bytes = BN_num_bytes(n) + 1;

    /* Key form is "ssh-rsa" + e + n. */
    len = 4 + 7 + 4 + e_bytes + 4 + n_bytes;

    key = LIBSSH2_ALLOC(session, len);
    if(key == NULL) {
        return NULL;
    }

    /* Process key encoding. */
    p = key;

    _libssh2_htonu32(p, 7);  /* Key type. */
    p += 4;
    memcpy(p, "ssh-rsa", 7);
    p += 7;

    p = write_bn(p, e, e_bytes);
    p = write_bn(p, n, n_bytes);

    *key_len = (size_t)(p - key);
    return key;
}

#if LIBSSH2_DSA
static unsigned char *
gen_publickey_from_dsa(LIBSSH2_SESSION* session, DSA *dsa,
                       size_t *key_len)
{
    int            p_bytes, q_bytes, g_bytes, k_bytes;
    unsigned long  len;
    unsigned char *key;
    unsigned char *p;

    const BIGNUM * p_bn;
    const BIGNUM * q;
    const BIGNUM * g;
    const BIGNUM * pub_key;
#ifdef HAVE_OPAQUE_STRUCTS
    DSA_get0_pqg(dsa, &p_bn, &q, &g);
#else
    p_bn = dsa->p;
    q = dsa->q;
    g = dsa->g;
#endif

#ifdef HAVE_OPAQUE_STRUCTS
    DSA_get0_key(dsa, &pub_key, NULL);
#else
    pub_key = dsa->pub_key;
#endif
    p_bytes = BN_num_bytes(p_bn) + 1;
    q_bytes = BN_num_bytes(q) + 1;
    g_bytes = BN_num_bytes(g) + 1;
    k_bytes = BN_num_bytes(pub_key) + 1;

    /* Key form is "ssh-dss" + p + q + g + pub_key. */
    len = 4 + 7 + 4 + p_bytes + 4 + q_bytes + 4 + g_bytes + 4 + k_bytes;

    key = LIBSSH2_ALLOC(session, len);
    if(key == NULL) {
        return NULL;
    }

    /* Process key encoding. */
    p = key;

    _libssh2_htonu32(p, 7);  /* Key type. */
    p += 4;
    memcpy(p, "ssh-dss", 7);
    p += 7;

    p = write_bn(p, p_bn, p_bytes);
    p = write_bn(p, q, q_bytes);
    p = write_bn(p, g, g_bytes);
    p = write_bn(p, pub_key, k_bytes);

    *key_len = (size_t)(p - key);
    return key;
}
#endif /* LIBSSH_DSA */

static int
gen_publickey_from_rsa_evp(LIBSSH2_SESSION *session,
                           unsigned char **method,
                           size_t *method_len,
                           unsigned char **pubkeydata,
                           size_t *pubkeydata_len,
                           EVP_PKEY *pk)
{
    RSA*           rsa = NULL;
    unsigned char *key;
    unsigned char *method_buf = NULL;
    size_t  key_len;

    _libssh2_debug(session,
                   LIBSSH2_TRACE_AUTH,
                   "Computing public key from RSA private key envelop");

    rsa = EVP_PKEY_get1_RSA(pk);
    if(rsa == NULL) {
        /* Assume memory allocation error... what else could it be ? */
        goto __alloc_error;
    }

    method_buf = LIBSSH2_ALLOC(session, 7);  /* ssh-rsa. */
    if(method_buf == NULL) {
        goto __alloc_error;
    }

    key = gen_publickey_from_rsa(session, rsa, &key_len);
    if(key == NULL) {
        goto __alloc_error;
    }
    RSA_free(rsa);

    memcpy(method_buf, "ssh-rsa", 7);
    *method         = method_buf;
    *method_len     = 7;
    *pubkeydata     = key;
    *pubkeydata_len = key_len;
    return 0;

  __alloc_error:
    if(rsa != NULL) {
        RSA_free(rsa);
    }
    if(method_buf != NULL) {
        LIBSSH2_FREE(session, method_buf);
    }

    return _libssh2_error(session,
                          LIBSSH2_ERROR_ALLOC,
                          "Unable to allocate memory for private key data");
}

#if LIBSSH2_DSA
static int
gen_publickey_from_dsa_evp(LIBSSH2_SESSION *session,
                           unsigned char **method,
                           size_t *method_len,
                           unsigned char **pubkeydata,
                           size_t *pubkeydata_len,
                           EVP_PKEY *pk)
{
    DSA*           dsa = NULL;
    unsigned char *key;
    unsigned char *method_buf = NULL;
    size_t  key_len;

    _libssh2_debug(session,
                   LIBSSH2_TRACE_AUTH,
                   "Computing public key from DSA private key envelop");

    dsa = EVP_PKEY_get1_DSA(pk);
    if(dsa == NULL) {
        /* Assume memory allocation error... what else could it be ? */
        goto __alloc_error;
    }

    method_buf = LIBSSH2_ALLOC(session, 7);  /* ssh-dss. */
    if(method_buf == NULL) {
        goto __alloc_error;
    }

    key = gen_publickey_from_dsa(session, dsa, &key_len);
    if(key == NULL) {
        goto __alloc_error;
    }
    DSA_free(dsa);

    memcpy(method_buf, "ssh-dss", 7);
    *method         = method_buf;
    *method_len     = 7;
    *pubkeydata     = key;
    *pubkeydata_len = key_len;
    return 0;

  __alloc_error:
    if(dsa != NULL) {
        DSA_free(dsa);
    }
    if(method_buf != NULL) {
        LIBSSH2_FREE(session, method_buf);
    }

    return _libssh2_error(session,
                          LIBSSH2_ERROR_ALLOC,
                          "Unable to allocate memory for private key data");
}
#endif /* LIBSSH_DSA */

#if LIBSSH2_ECDSA

static int
gen_publickey_from_ec_evp(LIBSSH2_SESSION *session,
                          unsigned char **method,
                          size_t *method_len,
                          unsigned char **pubkeydata,
                          size_t *pubkeydata_len,
                          EVP_PKEY *pk)
{
    int rc = 0;
    EC_KEY *ec = NULL;
    unsigned char *p;
    unsigned char *method_buf = NULL;
    unsigned char *key;
    size_t  key_len = 0;
    unsigned char *octal_value = NULL;
    size_t octal_len;
    const EC_POINT *public_key;
    const EC_GROUP *group;
    BN_CTX *bn_ctx;
    libssh2_curve_type type;

    _libssh2_debug(session,
       LIBSSH2_TRACE_AUTH,
       "Computing public key from EC private key envelop");

    bn_ctx = BN_CTX_new();
    if(bn_ctx == NULL)
        return -1;

    ec = EVP_PKEY_get1_EC_KEY(pk);
    if(ec == NULL) {
        rc = -1;
        goto clean_exit;
    }

    public_key = EC_KEY_get0_public_key(ec);
    group = EC_KEY_get0_group(ec);
    type = _libssh2_ecdsa_key_get_curve_type(ec);

    method_buf = LIBSSH2_ALLOC(session, 19);
    if(method_buf == NULL) {
        return _libssh2_error(session, LIBSSH2_ERROR_ALLOC,
            "out of memory");
    }

    if(type == LIBSSH2_EC_CURVE_NISTP256)
        memcpy(method_buf, "ecdsa-sha2-nistp256", 19);
    else if(type == LIBSSH2_EC_CURVE_NISTP384)
        memcpy(method_buf, "ecdsa-sha2-nistp384", 19);
    else if(type == LIBSSH2_EC_CURVE_NISTP521)
        memcpy(method_buf, "ecdsa-sha2-nistp521", 19);
    else {
        _libssh2_debug(session,
            LIBSSH2_TRACE_ERROR,
            "Unsupported EC private key type");
        rc = -1;
        goto clean_exit;
    }

    /* get length */
    octal_len = EC_POINT_point2oct(group, public_key, POINT_CONVERSION_UNCOMPRESSED, NULL, 0, bn_ctx);
    if(octal_len > EC_MAX_POINT_LEN) {
        rc = -1;
        goto clean_exit;
    }

    octal_value = malloc(octal_len);
    if(octal_value == NULL) {
        rc = -1;
        goto clean_exit;
    }

    /* convert to octal */
    if(EC_POINT_point2oct(group, public_key, POINT_CONVERSION_UNCOMPRESSED,
       octal_value, octal_len, bn_ctx) != octal_len) {
           rc = -1;
           goto clean_exit;
    }

    /* Key form is: type_len(4) + type(19) + domain_len(4) + domain(8) + pub_key_len(4) + pub_key(~65). */
    key_len = 4 + 19 + 4 + 8 + 4 + octal_len;
    key = LIBSSH2_ALLOC(session, key_len);
    if(key == NULL) {
        rc = -1;
        goto  clean_exit;
    }

    /* Process key encoding. */
    p = key;

    /* Key type */
    _libssh2_store_str(&p, (const char *)method_buf, 19);

    /* Name domain */
    _libssh2_store_str(&p, (const char *)method_buf + 11, 8);

    /* Public key */
    _libssh2_store_str(&p, (const char *)octal_value, octal_len);

    *method         = method_buf;
    *method_len     = 19;
    *pubkeydata     = key;
    *pubkeydata_len = key_len;

clean_exit:

    if(ec != NULL)
        EC_KEY_free(ec);

    if(bn_ctx != NULL) {
        BN_CTX_free(bn_ctx);
    }

    if(octal_value != NULL)
        free(octal_value);

    if(rc == 0)
        return 0;

    if(method_buf != NULL)
        LIBSSH2_FREE(session, method_buf);

    return -1;
}

/*
 * _libssh2_ecdsa_create_key
 *
 * Creates a local private key based on input curve
 * and returns octal value and octal length
 *
 */

int
_libssh2_ecdsa_create_key(_libssh2_ec_key **out_private_key,
                          unsigned char **out_public_key_octal,
                          size_t *out_public_key_octal_len, libssh2_curve_type curve_type)
{
    int ret = 1;
    size_t octal_len = 0;
    unsigned char octal_value[EC_MAX_POINT_LEN];
    const EC_POINT *public_key = NULL;
    EC_KEY *private_key = NULL;
    const EC_GROUP *group = NULL;

    /* create key */
    BN_CTX *bn_ctx = BN_CTX_new();
    if(!bn_ctx)
        return -1;

    private_key = EC_KEY_new_by_curve_name(curve_type);
    group = EC_KEY_get0_group(private_key);

    EC_KEY_generate_key(private_key);
    public_key = EC_KEY_get0_public_key(private_key);

    /* get length */
    octal_len = EC_POINT_point2oct(group, public_key, POINT_CONVERSION_UNCOMPRESSED, NULL, 0, bn_ctx);
    if(octal_len > EC_MAX_POINT_LEN) {
        ret = -1;
        goto clean_exit;
    }

    /* convert to octal */
    if(EC_POINT_point2oct(group, public_key, POINT_CONVERSION_UNCOMPRESSED,
       octal_value, octal_len, bn_ctx) != octal_len) {
           ret = -1;
           goto clean_exit;
    }

    if(out_private_key != NULL)
        *out_private_key = private_key;

    if(out_public_key_octal) {
        *out_public_key_octal = malloc(octal_len);
        if(out_public_key_octal == NULL) {
            ret = -1;
            goto clean_exit;
        }

        memcpy(*out_public_key_octal, octal_value, octal_len);
    }

    if(out_public_key_octal_len != NULL)
        *out_public_key_octal_len = octal_len;

clean_exit:

    if(bn_ctx)
        BN_CTX_free(bn_ctx);

    return (ret == 1) ? 0 : -1;
}

/* _libssh2_ecdh_gen_k
 *
 * Computes the shared secret K given a local private key,
 * remote public key and length
 */

int
_libssh2_ecdh_gen_k(_libssh2_bn **k, _libssh2_ec_key *private_key,
    const unsigned char *server_public_key, size_t server_public_key_len)
{
    int ret = 0;
    int rc;
    size_t secret_len;
    unsigned char *secret = NULL;
    const EC_GROUP *private_key_group;
    EC_POINT *server_public_key_point;

    BN_CTX *bn_ctx = BN_CTX_new();

    if(!bn_ctx)
        return -1;

    if(k == NULL)
        return -1;

    private_key_group = EC_KEY_get0_group(private_key);

    server_public_key_point = EC_POINT_new(private_key_group);
    if(server_public_key_point == NULL)
        return -1;

    rc = EC_POINT_oct2point(private_key_group, server_public_key_point, server_public_key, server_public_key_len, bn_ctx);
    if(rc != 1) {
        ret = -1;
        goto clean_exit;
    }

    secret_len = (EC_GROUP_get_degree(private_key_group) + 7) / 8;
    secret = malloc(secret_len);
    if(!secret) {
        ret = -1;
        goto clean_exit;
    }

    secret_len = ECDH_compute_key(secret, secret_len, server_public_key_point, private_key, NULL);

    if(secret_len <= 0 || secret_len > EC_MAX_POINT_LEN) {
        ret = -1;
        goto clean_exit;
    }

    BN_bin2bn(secret, secret_len, *k);

clean_exit:

    if(server_public_key_point != NULL)
        EC_POINT_free(server_public_key_point);

    if(bn_ctx != NULL)
        BN_CTX_free(bn_ctx);

    if(secret != NULL)
        free(secret);

    return ret;
}


#endif /* LIBSSH2_ECDSA */

int
_libssh2_pub_priv_keyfile(LIBSSH2_SESSION *session,
                          unsigned char **method,
                          size_t *method_len,
                          unsigned char **pubkeydata,
                          size_t *pubkeydata_len,
                          const char *privatekey,
                          const char *passphrase)
{
    int       st;
    BIO*      bp;
    EVP_PKEY* pk;
    int       pktype;

    _libssh2_debug(session,
                   LIBSSH2_TRACE_AUTH,
                   "Computing public key from private key file: %s",
                   privatekey);

    bp = BIO_new_file(privatekey, "r");
    if(bp == NULL) {
        return _libssh2_error(session,
                              LIBSSH2_ERROR_FILE,
                              "Unable to extract public key from private key "
                              "file: Unable to open private key file");
    }

    BIO_reset(bp);
    pk = PEM_read_bio_PrivateKey(bp, NULL, NULL, (void *)passphrase);
    BIO_free(bp);

    if(pk == NULL) {
        return _libssh2_error(session,
                              LIBSSH2_ERROR_FILE,
                              "Unable to extract public key "
                              "from private key file: "
                              "Wrong passphrase or invalid/unrecognized "
                              "private key file format");
    }

#ifdef HAVE_OPAQUE_STRUCTS
    pktype = EVP_PKEY_id(pk);
#else
    pktype = pk->type;
#endif

    switch(pktype) {
    case EVP_PKEY_RSA :
        st = gen_publickey_from_rsa_evp(
            session, method, method_len, pubkeydata, pubkeydata_len, pk);
        break;

#if LIBSSH2_DSA
    case EVP_PKEY_DSA :
        st = gen_publickey_from_dsa_evp(
            session, method, method_len, pubkeydata, pubkeydata_len, pk);
        break;
#endif /* LIBSSH_DSA */

#if LIBSSH2_ECDSA
    case EVP_PKEY_EC :
        st = gen_publickey_from_ec_evp(
            session, method, method_len, pubkeydata, pubkeydata_len, pk);
    break;
#endif

    default :
        st = _libssh2_error(session,
                            LIBSSH2_ERROR_FILE,
                            "Unable to extract public key "
                            "from private key file: "
                            "Unsupported private key file format");
        break;
    }

    EVP_PKEY_free(pk);
    return st;
}

int
_libssh2_pub_priv_keyfilememory(LIBSSH2_SESSION *session,
                                unsigned char **method,
                                size_t *method_len,
                                unsigned char **pubkeydata,
                                size_t *pubkeydata_len,
                                const char *privatekeydata,
                                size_t privatekeydata_len,
                                const char *passphrase)
{
    int       st;
    BIO*      bp;
    EVP_PKEY* pk;
    int       pktype;

    _libssh2_debug(session,
                   LIBSSH2_TRACE_AUTH,
                   "Computing public key from private key.");

    bp = BIO_new_mem_buf((char *)privatekeydata, privatekeydata_len);
    if(!bp) {
        return -1;
    }

    BIO_reset(bp);
    pk = PEM_read_bio_PrivateKey(bp, NULL, NULL, (void *)passphrase);
    BIO_free(bp);

    if(pk == NULL) {
        return _libssh2_error(session,
                              LIBSSH2_ERROR_FILE,
                              "Unable to extract public key "
                              "from private key file: "
                              "Wrong passphrase or invalid/unrecognized "
                              "private key file format");
    }

#ifdef HAVE_OPAQUE_STRUCTS
    pktype = EVP_PKEY_id(pk);
#else
    pktype = pk->type;
#endif

    switch(pktype) {
    case EVP_PKEY_RSA :
        st = gen_publickey_from_rsa_evp(session, method, method_len,
                                        pubkeydata, pubkeydata_len, pk);
        break;
#if LIBSSH2_DSA
    case EVP_PKEY_DSA :
        st = gen_publickey_from_dsa_evp(session, method, method_len,
                                        pubkeydata, pubkeydata_len, pk);
        break;
#endif /* LIBSSH_DSA */
    default :
        st = _libssh2_error(session,
                            LIBSSH2_ERROR_FILE,
                            "Unable to extract public key "
                            "from private key file: "
                            "Unsupported private key file format");
        break;
    }

    EVP_PKEY_free(pk);
    return st;
}

void
_libssh2_dh_init(_libssh2_dh_ctx *dhctx)
{
    *dhctx = BN_new();                          /* Random from client */
}

int
_libssh2_dh_key_pair(_libssh2_dh_ctx *dhctx, _libssh2_bn *public,
                     _libssh2_bn *g, _libssh2_bn *p, int group_order,
                     _libssh2_bn_ctx *bnctx)
{
    /* Generate x and e */
    BN_rand(*dhctx, group_order * 8 - 1, 0, -1);
    BN_mod_exp(public, g, *dhctx, p, bnctx);
    return 0;
}

int
_libssh2_dh_secret(_libssh2_dh_ctx *dhctx, _libssh2_bn *secret,
                   _libssh2_bn *f, _libssh2_bn *p,
                   _libssh2_bn_ctx *bnctx)
{
    /* Compute the shared secret */
    BN_mod_exp(secret, f, *dhctx, p, bnctx);
    return 0;
}

void
_libssh2_dh_dtor(_libssh2_dh_ctx *dhctx)
{
    BN_clear_free(*dhctx);
    *dhctx = NULL;
}

#endif /* LIBSSH2_OPENSSL */
