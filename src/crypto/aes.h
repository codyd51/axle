#ifndef AES_H
#define AES_H

#include <std/std.h>

#define AES_BLOCK_SIZE 16     

typedef unsigned char BYTE;            // 8-bit byte
typedef unsigned int WORD;             // 32-bit word, change to "long" for 16-bit machines


void aes_key_setup(const BYTE key[],          // The key, must be 128, 192, or 256 bits
                   WORD w[],                  // Output key schedule to be used later
                   int keysize);              // Bit length of the key, 128, 192, or 256

void aes_encrypt(const BYTE in[],             // 16 bytes of plaintext
                 BYTE out[],                  // 16 bytes of ciphertext
                 const WORD key[],            // From the key setup
                 int keysize);                // Bit length of the key, 128, 192, or 256

void aes_decrypt(const BYTE in[],             // 16 bytes of ciphertext
                 BYTE out[],                  // 16 bytes of plaintext
                 const WORD key[],            // From the key setup
                 int keysize);                // Bit length of the key, 128, 192, or 256

int aes_encrypt_cbc(const BYTE in[],          // Plaintext
                    size_t in_len,            // Must be a multiple of AES_BLOCK_SIZE
                    BYTE out[],               // Ciphertext, same length as plaintext
                    const WORD key[],         // From the key setup
                    int keysize,              // Bit length of the key, 128, 192, or 256
                    const BYTE iv[]);         // IV, must be AES_BLOCK_SIZE bytes long

// Only output the CBC-MAC of the input.
int aes_encrypt_cbc_mac(const BYTE in[],      // plaintext
                        size_t in_len,        // Must be a multiple of AES_BLOCK_SIZE
                        BYTE out[],           // Output MAC
                        const WORD key[],     // From the key setup
                        int keysize,          // Bit length of the key, 128, 192, or 256
                        const BYTE iv[]);     // IV, must be AES_BLOCK_SIZE bytes long

void increment_iv(BYTE iv[],                  // Must be a multiple of AES_BLOCK_SIZE
                  int counter_size);          // Bytes of the IV used for counting (low end)

void aes_encrypt_ctr(const BYTE in[],         // Plaintext
                     size_t in_len,           // Any byte length
                     BYTE out[],              // Ciphertext, same length as plaintext
                     const WORD key[],        // From the key setup
                     int keysize,             // Bit length of the key, 128, 192, or 256
                     const BYTE iv[]);        // IV, must be AES_BLOCK_SIZE bytes long

void aes_decrypt_ctr(const BYTE in[],         // Ciphertext
                     size_t in_len,           // Any byte length
                     BYTE out[],              // Plaintext, same length as ciphertext
                     const WORD key[],        // From the key setup
                     int keysize,             // Bit length of the key, 128, 192, or 256
                     const BYTE iv[]);        // IV, must be AES_BLOCK_SIZE bytes long

int aes_encrypt_ccm(const BYTE plaintext[],              // IN  - Plaintext.
                    WORD plaintext_len,                  // IN  - Plaintext length.
                    const BYTE associated_data[],        // IN  - Associated Data included in authentication, but not encryption.
                    unsigned short associated_data_len,  // IN  - Associated Data length in bytes.
                    const BYTE nonce[],                  // IN  - The Nonce to be used for encryption.
                    unsigned short nonce_len,            // IN  - Nonce length in bytes.
                    BYTE ciphertext[],                   // OUT - Ciphertext, a concatination of the plaintext and the MAC.
                    WORD *ciphertext_len,                // OUT - The length of the ciphertext, always plaintext_len + mac_len.
                    WORD mac_len,                        // IN  - The desired length of the MAC, must be 4, 6, 8, 10, 12, 14, or 16.
                    const BYTE key[],                    // IN  - The AES key for encryption.
                    int keysize);                        // IN  - The length of the key in bits. Valid values are 128, 192, 256.

int aes_decrypt_ccm(const BYTE ciphertext[],             // IN  - Ciphertext, the concatination of encrypted plaintext and MAC.
                    WORD ciphertext_len,                 // IN  - Ciphertext length in bytes.
                    const BYTE assoc[],                  // IN  - The Associated Data, required for authentication.
                    unsigned short assoc_len,            // IN  - Associated Data length in bytes.
                    const BYTE nonce[],                  // IN  - The Nonce to use for decryption, same one as for encryption.
                    unsigned short nonce_len,            // IN  - Nonce length in bytes.
                    BYTE plaintext[],                    // OUT - The plaintext that was decrypted. Will need to be large enough to hold ciphertext_len - mac_len.
                    WORD *plaintext_len,                 // OUT - Length in bytes of the output plaintext, always ciphertext_len - mac_len .
                    WORD mac_len,                        // IN  - The length of the MAC that was calculated.
                    int *mac_auth,                       // OUT - TRUE if authentication succeeded, FALSE if it did not. NULL pointer will ignore the authentication.
                    const BYTE key[],                    // IN  - The AES key for decryption.
                    int keysize);                        // IN  - The length of the key in BITS. Valid values are 128, 192, 256.


int aes_test();
int aes_ecb_test();
int aes_cbc_test();
int aes_ctr_test();
int aes_ccm_test();

#endif   // AES_H