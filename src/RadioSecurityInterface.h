/*
 * The file is licensed under the Apache License, Version 2.0
 * (c) 2019 Helmut Tschemernjak
 * 30826 Garbsen (Hannover) Germany
 */

#ifndef __RADIOSECURITYINTERFACE_H__
#define __RADIOSECURITYINTERFACE_H__

class RadioSecurityInterface {
public:
    virtual ~RadioSecurityInterface() { }

    /*
     * Get security protocol version to allow
     * and differentiate between multiple security versions
     */
    virtual int GetSecurityVersion(void) = 0;
    
    /*
     * The block size for the hash code (e.g. SHA256) in bytes
     */
    virtual	int GetHashBlockSize(void) = 0;
    
    /*
     * The calculation for the public password hash code utilizes a seed (e.g. random)
     * and the cleartext password
     */
    virtual	void HashPassword(void *seed, int seedLen, void *password, int pwLen, void *hashResult) = 0;

    /*
     * The encryption/decryption block size in bytes (e.g. 16 bytes for AES128)
     */
    virtual	int GetEncryptionBlockSize(void) = 0;
    
    /*
     * The creation of a context allocates the memory needed, and initializes
     * its data (e.g. key and initial vector 'iv' for AES)
     */
    virtual void *CreateEncryptionContext(void *key, int keyLen, void *seed = NULL, int seedlen = 0) = 0;
    
    /*
     * Release the context and its allocated memory from CreateEncryptionContext
     */
    virtual void DestroyEncryptionContext(void *context) = 0;
    
    /*
     *Encrypts a cleartext input message into an encrypted output block
     */
	virtual	void EncryptMessage(void *context, const void *input, void *output, int len) = 0;

    /*
     * Decrypts an input block into an cleartext output message
     */
	virtual void DecryptMessage(void *context, const void *input, void *output, int len) = 0;

    virtual void EncryptTest(void) = 0;
};

#endif // __RADIOSECURITYINTERFACE_H__
