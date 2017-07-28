/*
 * The file is licensed under the Apache License, Version 2.0
 * (c) 2017 Helmut Tschemernjak
 * 30826 Garbsen (Hannover) Germany
 */

#include "sha256.h"
#include "aes.h"

class RadioSecurity : public RadioSecurityInterface {
public:
    RadioSecurity();
    ~RadioSecurity();
    virtual int GetSecurityVersion(void);
    /*
     * The hash block size for SHA256 in bytes
     */
    virtual	int GetHashBlockSize(void);
    virtual	void HashPassword(void *seed, int seedLen, void *password, int pwLen, void *hashResult);
    
    virtual	int GetEncryptionBlockSize(void);
    virtual void *CreateEncryptionContext(void *key, int keyLen, void *seed = NULL, int seedlen = 0);
    virtual void DestroyEncryptionContext(void *context);
    virtual	void EncryptMessage(void *context, const void *input, void *output, int len);
    virtual void DecryptMessage(void *context, const void *input, void *output, int len);
    virtual void EncryptTest(void);
private:
    static int const _securityVers = 1;
};
