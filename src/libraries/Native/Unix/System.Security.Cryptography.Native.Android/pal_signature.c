// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

#include "pal_signature.h"

int32_t AndroidCryptoNative_SignWithSignatureObject(JNIEnv* env,
                                                    jobject signatureObject,
                                                    jobject privateKey,
                                                    const uint8_t* dgst,
                                                    int32_t dgstlen,
                                                    uint8_t* sig,
                                                    int32_t* siglen)
{
    assert(dgst);
    assert(sig);
    assert(signatureObject);
    assert(privateKey);
    if (!siglen)
    {
        return FAIL;
    }

    (*env)->CallVoidMethod(env, signatureObject, g_SignatureInitSign, privateKey);
    ON_EXCEPTION_PRINT_AND_GOTO(error);

    jbyteArray digestArray = (*env)->NewByteArray(env, dgstlen);
    (*env)->SetByteArrayRegion(env, digestArray, 0, dgstlen, (const jbyte*)dgst);
    (*env)->CallVoidMethod(env, signatureObject, g_SignatureUpdate, digestArray);
    ReleaseLRef(env, digestArray);
    ON_EXCEPTION_PRINT_AND_GOTO(error);

    jbyteArray sigResult = (*env)->CallObjectMethod(env, signatureObject, g_SignatureSign);
    ON_EXCEPTION_PRINT_AND_GOTO(error);
    jsize sigSize = (*env)->GetArrayLength(env, sigResult);
    *siglen = sigSize;
    (*env)->GetByteArrayRegion(env, sigResult, 0, sigSize, (jbyte*)sig);
    ReleaseLRef(env, sigResult);
    return SUCCESS;

error:
    return FAIL;
}

int32_t AndroidCryptoNative_VerifyWithSignatureObject(JNIEnv* env,
                                                      jobject signatureObject,
                                                      jobject publicKey,
                                                      const uint8_t* dgst,
                                                      int32_t dgstlen,
                                                      const uint8_t* sig,
                                                      int32_t siglen)
{
    assert(dgst);
    assert(sig);
    assert(signatureObject);
    assert(publicKey);

    (*env)->CallVoidMethod(env, signatureObject, g_SignatureInitVerify, publicKey);
    ON_EXCEPTION_PRINT_AND_GOTO(error);

    jbyteArray digestArray = (*env)->NewByteArray(env, dgstlen);
    (*env)->SetByteArrayRegion(env, digestArray, 0, dgstlen, (const jbyte*)dgst);
    (*env)->CallVoidMethod(env, signatureObject, g_SignatureUpdate, digestArray);
    ReleaseLRef(env, digestArray);
    ON_EXCEPTION_PRINT_AND_GOTO(error);

    jbyteArray sigArray = (*env)->NewByteArray(env, siglen);
    (*env)->SetByteArrayRegion(env, sigArray, 0, siglen, (const jbyte*)sig);
    jboolean verified = (*env)->CallBooleanMethod(env, signatureObject, g_SignatureVerify, sigArray);
    ReleaseLRef(env, sigArray);
    ON_EXCEPTION_PRINT_AND_GOTO(error);

    return verified ? SUCCESS : FAIL;

error:
    return SIGNATURE_VERIFICATION_ERROR;
}
