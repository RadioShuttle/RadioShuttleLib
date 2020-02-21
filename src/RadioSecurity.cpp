/*
 * The file is licensed under the Apache License, Version 2.0
 * (c) 2019 Helmut Tschemernjak
 * 30826 Garbsen (Hannover) Germany
 */

#ifdef ARDUINO
#include <Arduino.h>
#define FEATURE_LORA	1
#include "arduino-util.h"
#endif

#ifdef __MBED__
#include "mbed.h"
#include "main.h"
#include "PinMap.h"
#endif

#include "RadioSecurityInterface.h"
#include "RadioSecurity.h"

#ifdef FEATURE_LORA
RadioSecurity::RadioSecurity(void)
{
    
}

RadioSecurity::~RadioSecurity(void)
{
}

int
RadioSecurity::GetSecurityVersion(void)
{
    return _securityVers;
}

int
RadioSecurity::GetHashBlockSize(void)
{
    return SHA256_BLOCK_SIZE;
}

void
RadioSecurity::HashPassword(void *seed, int seedLen, void *password, int pwLen, void *hashResult)
{
    SHA256_CTX *shactx = (SHA256_CTX *)new uint8_t[sizeof(SHA256_CTX)];
	if (!shactx)
        return;
    
    sha256_init(shactx);
    if (seedLen)
        sha256_update(shactx, (BYTE *)seed, seedLen);
    if (password)
        sha256_update(shactx, (BYTE *)password, pwLen);
    sha256_final(shactx, (BYTE *)hashResult);
    
    delete[] shactx;
}


int
RadioSecurity::GetEncryptionBlockSize(void)
{
    return AES128_KEYLEN;
}



void *
RadioSecurity::CreateEncryptionContext(void *key, int keyLen, void *seed, int seedlen)
{
    AES_CTX *aesctx = (AES_CTX *)new uint8_t[sizeof(AES_CTX)];
    
    uint8_t mykey[AES128_KEYLEN];
    uint8_t myseed[AES128_KEYLEN];
    
    if (seed) {
    	memset(myseed, 0, sizeof(myseed));
        memcpy(myseed, seed, seedlen > AES128_KEYLEN ? AES128_KEYLEN : seedlen);
    }
    memset(mykey, 0, sizeof(mykey));
    memcpy(mykey, key, keyLen  > AES128_KEYLEN ? AES128_KEYLEN : keyLen);

    AES128_InitContext(aesctx, mykey, seed ? myseed : NULL);

    return aesctx;
}

void
RadioSecurity::DestroyEncryptionContext(void *context)
{
	delete[] (AES_CTX *)context;
}


void
RadioSecurity::EncryptMessage(void *context, const void *input, void *output, int len)
{
    uint8_t *in = (uint8_t *)input;
    uint8_t *out = (uint8_t *)output;
    int off = 0;
    
    while (off < len) {
    	AES128_ECB_encrypt((AES_CTX *)context, in + off, out + off);
        off += AES128_KEYLEN;
    }
}


void
RadioSecurity::DecryptMessage(void *context, const void *input, void *output, int len)
{
    uint8_t *in = (uint8_t *)input;
    uint8_t *out = (uint8_t *)output;
	int off = 0;
    
    while (off < len) {
        AES128_ECB_decrypt((AES_CTX *)context, in + off, out + off);
        off += AES128_KEYLEN;
    }
}

void
RadioSecurity::EncryptTest(void)
{
    uint8_t key[] = {0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6, 0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c};
    uint8_t iv[]  = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f };
    
    {
        
        dprintf("ECB encrypt: ");
        void *context = CreateEncryptionContext(key, sizeof(key));
        
        // static void test_encrypt_ecb(void)
        uint8_t in[]  = {0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96, 0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a};
        uint8_t out[] = {0x3a, 0xd7, 0x7b, 0xb4, 0x0d, 0x7a, 0x36, 0x60, 0xa8, 0x9e, 0xca, 0xf3, 0x24, 0x66, 0xef, 0x97};
        uint8_t buffer[16];
        
        
        AES128_ECB_encrypt((AES_CTX *)context, in, buffer);
        
        
        if (memcmp((char*) out, (char*) buffer, 16) == 0)
        {
            dprintf("SUCCESS!");
        } else {
            dprintf("FAILURE!");
        }
        DestroyEncryptionContext(context);
    }
    
    {
        dprintf("ECB decrypt: ");
        void *context = CreateEncryptionContext(key, sizeof(key));

        uint8_t in[]  = {0x3a, 0xd7, 0x7b, 0xb4, 0x0d, 0x7a, 0x36, 0x60, 0xa8, 0x9e, 0xca, 0xf3, 0x24, 0x66, 0xef, 0x97};
        uint8_t out[] = {0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96, 0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a};
        uint8_t buffer[16];
        
        AES128_ECB_decrypt((AES_CTX *)context, in, buffer);
        
        if(memcmp((char*) out, (char*) buffer, 16) == 0) {
            dprintf("SUCCESS!");
        } else {
            dprintf("FAILURE!");
        }
        DestroyEncryptionContext(context);
    }

    
    {
        dprintf("CBC encrypt: ");
        void *context = CreateEncryptionContext(key, sizeof(key), iv, sizeof(iv));

        uint8_t in[]  = { 0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96, 0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
            0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c, 0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
            0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11, 0xe5, 0xfb, 0xc1, 0x19, 0x1a, 0x0a, 0x52, 0xef,
            0xf6, 0x9f, 0x24, 0x45, 0xdf, 0x4f, 0x9b, 0x17, 0xad, 0x2b, 0x41, 0x7b, 0xe6, 0x6c, 0x37, 0x10 };
    	uint8_t out[] = { 0x76, 0x49, 0xab, 0xac, 0x81, 0x19, 0xb2, 0x46, 0xce, 0xe9, 0x8e, 0x9b, 0x12, 0xe9, 0x19, 0x7d,
        	0x50, 0x86, 0xcb, 0x9b, 0x50, 0x72, 0x19, 0xee, 0x95, 0xdb, 0x11, 0x3a, 0x91, 0x76, 0x78, 0xb2,
        	0x73, 0xbe, 0xd6, 0xb8, 0xe3, 0xc1, 0x74, 0x3b, 0x71, 0x16, 0xe6, 0x9e, 0x22, 0x22, 0x95, 0x16,
        	0x3f, 0xf1, 0xca, 0xa1, 0x68, 0x1f, 0xac, 0x09, 0x12, 0x0e, 0xca, 0x30, 0x75, 0x86, 0xe1, 0xa7 };
	
        uint8_t buffer[64];

    
    	AES128_CBC_encrypt_buffer((AES_CTX	*)context, buffer, in, 64);
    
    
        if(memcmp((char*) out, (char*) buffer, 64) == 0)     {
            dprintf("SUCCESS!");
        } else     {
            dprintf("FAILURE!");
        }
        DestroyEncryptionContext(context);
    }
    
    {
        dprintf("CBC decrypt: ");
        void *context = CreateEncryptionContext(key, sizeof(key), iv, sizeof(iv));

        uint8_t in[]  = { 0x76, 0x49, 0xab, 0xac, 0x81, 0x19, 0xb2, 0x46, 0xce, 0xe9, 0x8e, 0x9b, 0x12, 0xe9, 0x19, 0x7d,
            0x50, 0x86, 0xcb, 0x9b, 0x50, 0x72, 0x19, 0xee, 0x95, 0xdb, 0x11, 0x3a, 0x91, 0x76, 0x78, 0xb2,
            0x73, 0xbe, 0xd6, 0xb8, 0xe3, 0xc1, 0x74, 0x3b, 0x71, 0x16, 0xe6, 0x9e, 0x22, 0x22, 0x95, 0x16,
            0x3f, 0xf1, 0xca, 0xa1, 0x68, 0x1f, 0xac, 0x09, 0x12, 0x0e, 0xca, 0x30, 0x75, 0x86, 0xe1, 0xa7 };
        uint8_t out[] = { 0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96, 0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
            0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c, 0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
            0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11, 0xe5, 0xfb, 0xc1, 0x19, 0x1a, 0x0a, 0x52, 0xef,
            0xf6, 0x9f, 0x24, 0x45, 0xdf, 0x4f, 0x9b, 0x17, 0xad, 0x2b, 0x41, 0x7b, 0xe6, 0x6c, 0x37, 0x10 };
        uint8_t buffer[64];
        
        
        AES128_CBC_decrypt_buffer((AES_CTX	*)context, buffer+0, in+0,  16);
        AES128_CBC_decrypt_buffer((AES_CTX	*)context, buffer+16, in+16, 16);
        AES128_CBC_decrypt_buffer((AES_CTX	*)context, buffer+32, in+32, 16);
        AES128_CBC_decrypt_buffer((AES_CTX	*)context, buffer+48, in+48, 16);
        
        if (memcmp((char*) out, (char*) buffer, 64) == 0) {
            dprintf("SUCCESS!");
        } else {
            dprintf("FAILURE!");
        }
        DestroyEncryptionContext(context);
    }
}


#endif // FEATURE_LORA
