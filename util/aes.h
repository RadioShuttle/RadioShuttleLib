#ifndef _AES128_H_
#define _AES128_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// #define the macros below to 1/0 to enable/disable the mode of operation.
//
// CBC enables AES128 encryption in CBC-mode of operation and handles 0-padding.
// ECB enables the basic ECB 16-byte block algorithm. Both can be enabled simultaneously.

// The #ifndef-guard allows it to be configured before #include'ing or at compile time.
#ifndef DISABLE_CBC_MODE
 #define CBC_MODE
#endif

#ifndef DISABLE_ECB_MODE
 #define ECB_MODE
#endif

/*****************************************************************************/
/* Private variables:                                                        */
/*****************************************************************************/
// state - array holding the intermediate results during decryption.
typedef uint8_t state_t[4][4];

// Key length in bytes [128 bit]
#define AES128_KEYLEN 16
// The number of rounds in AES Cipher.
#define Nr 10


typedef struct {
    state_t* state;
	uint8_t RoundKey[176];	// The array that stores the round keys
#ifdef CBC_MODE
    // uint8_t* Iv;
    uint8_t InitialVector[AES128_KEYLEN]; // Initial Vector used only for CBC mode
#endif
} AES_CTX;

void AES128_InitContext(AES_CTX *ctx, const uint8_t* key, const uint8_t* initialVector);

#ifdef ECB_MODE

void AES128_ECB_encrypt(AES_CTX *ctx, const uint8_t* input, uint8_t *output);
void AES128_ECB_decrypt(AES_CTX *ctx, const uint8_t* input, uint8_t *output);

#endif // ECB_MODE


#ifdef CBC_MODE

void AES128_CBC_encrypt_buffer(AES_CTX *ctx, uint8_t* output, const uint8_t* input, uint32_t length);
void AES128_CBC_decrypt_buffer(AES_CTX *ctx, uint8_t* output, const uint8_t* input, uint32_t length);

#endif // CBC_MODE

#ifdef __cplusplus
}
#endif

#endif //_AES128_H_
