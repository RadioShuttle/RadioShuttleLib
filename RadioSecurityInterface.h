/*
 * The file is Licensed under the Apache License, Version 2.0
 * (c) 2017 Helmut Tschemernjak
 * 30826 Garbsen (Hannover) Germany
 */



class RadioSecurityInterface {
public:
    virtual int GetSecurityVersion(void) = 0;
    /*
     * The hash block size for SHA256 in bytes
     */
    virtual	int GetHashBlockSize(void) = 0;
    /*
     * The hash calculation for public use uses a seed (e.g. random) and the password
     */
    virtual	void HashPassword(void *seed, int seedLen, void *password, int pwLen, void *hashResult) = 0;

    /*
     * The encryption/decryption block size in bytes
     */
    virtual	int GetEncryptionBlockSize(void) = 0;
    
    /*
     * The creation of a context allocates the memory needed and initializes
     * the key and initial vector (iv).
     */
    virtual void *CreateEncryptionContext(void *key, int keyLen, void *seed = NULL, int seedlen = 0) = 0;
    
    /*
     * release the context and it's allocated memory.
     */
    virtual void DestroyEncryptionContext(void *context) = 0;
    
	virtual	void EncryptMessage(void *context, const void *input, void *output, int len) = 0;
	virtual void DecryptMessage(void *context, const void *input, void *output, int len) = 0;
    virtual void EncryptTest(void) = 0;

};
