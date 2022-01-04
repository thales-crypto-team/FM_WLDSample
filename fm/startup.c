/****************************************************************************\
*
* File: startup.c
*
* Description: FM Sample that verifies key handle on embedded slot ID
*
* Copyright © 2018 - 2021 SafeNet. All rights reserved.

* This file contains information that is
* proprietary to SafeNet, and may not be distributed
* or copied without written consent from SafeNet.
*
\****************************************************************************/


#include <stdlib.h>
#include <stdio.h>       //printf()
#include <string.h>

#include <cryptoki.h>

#include "fm/hsm/fmsw.h"
#include "fm/hsm/fm_io_service.h"
#include "fm/hsm/fm.h"

#include "fmcrypto.h"
#include <endian.h>


/********************************************************************
    IqrFM_HandleMessage

    Read the FM command and search for the hKey object handle on the 
    passed in embedded slot ID.  If an error occurs log a message 
    in the HSM debug window
*/
static
int IqrFM_HandleMessage( FmMsgHandle token )
{
    uint32_t slot;
    uint32_t hKey, hHSMKey=0;

    CK_RV ckResult = CKR_OK;
    CK_OBJECT_HANDLE hObj;
    CK_SESSION_HANDLE hSession;
    CK_SLOT_ID eSlot;
    CK_ULONG retcount = 0;
    const char label[] = "MyAESKey";
    CK_ATTRIBUTE findAttr = {CKA_LABEL, (CK_BYTE_PTR)label, strlen(label)};


    // Read in the passed in parameters from the message block
    if (SVC_IO_Read32(token, &slot) != sizeof(slot))
    {
        ckResult = CKR_ARGUMENTS_BAD;
        goto done;
    }

    if (SVC_IO_Read32(token, &hKey) != sizeof(hKey))
    {
        ckResult = CKR_ARGUMENTS_BAD;
        goto done;
    }

    // Open a session
    eSlot = (CK_SLOT_ID)slot;
    ckResult = C_OpenSession(eSlot, CKF_RW_SESSION|CKF_SERIAL_SESSION, NULL, NULL, &hSession);
    if (ckResult == CKR_OK)
    {
        ckResult = C_FindObjectsInit(hSession, &findAttr, 1);
        if (ckResult == CKR_OK)
        {
            ckResult = C_FindObjects(hSession, &hObj, 1, &retcount);
            (void)C_FindObjectsFinal(hSession);
        }
    }

    if (ckResult == CKR_OK)
    {
        // Found the object with that label - make sure same handle as was passed in
        hHSMKey = (uint32_t)hObj;
        if (hHSMKey != hKey || retcount != 1)
            ckResult = CKR_OBJECT_HANDLE_INVALID;
    }

    if (ckResult != CKR_OK)
    {
        printf("SampleFM: key=%d, hsmkey=%d, rv=%x\n",
            (int)hKey, (int)hHSMKey, (unsigned int)ckResult);
    }

done:

    return (int)ckResult;
}

FM_RV Startup(void)
{
    FM_RV rv;

    /* register handler for our new API */
    rv = FMSW_RegisterStreamDispatch(GetFMID(), IqrFM_HandleMessage);

    return rv;
}
