/*
    main.c 

    This file provides source code for a sample implementation of
    a workload distribution (WLD) model that allows for multiple FM 
    function calls to be distributed over a group of HSM adapters.
    This code is sample ONLY and Thales Inc. assumes no liability
    or responsibility for its correct operation.  Refer to the 
    Application Guide and readme file for a desription of its use.
*/

#undef UNICODE


#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cryptoki_v2.h"
#include <stdbool.h>
#include "fm/common/fm_byteorder.h"

#include "md.h"
#include "wld.h"

void*                           LibHandle = NULL;
CK_FUNCTION_LIST*               P11Functions = NULL;
char                            EnvLib[4096];

/*
    CK_BBOOL GetLibrary()

    Get the Cryptoki library full path from the SfntLibPath env variable
*/
CK_BBOOL GetLibrary()
{
    CK_BBOOL myRC = CK_FALSE;
    char* pPath = NULL;

    pPath = getenv("SfntLibPath");
    if (pPath == NULL)
    {
        printf("Failed to get \"SfntLibPath\"\n");
        printf("Please create and export an environment variable that points to the\n");
        printf("cryptoki library including library name (allows libcklog2.so)\n");
        return CK_FALSE;
    }

    memset(EnvLib, 0, sizeof(EnvLib));
    snprintf(EnvLib, sizeof(EnvLib) - 1, "%s", pPath);
    myRC = CK_TRUE;

    return myRC;
}

/*
    CK_BBOOL LoadP11Functions()

    Load and initialize the standard PKCS#11 function pointers
*/
CK_BBOOL LoadP11Functions()
{
    CK_BBOOL myRC = CK_FALSE;
    CK_C_GetFunctionList C_GetFunctionList = NULL;
    CK_RV rv = CKR_TOKEN_NOT_PRESENT;

    if (GetLibrary() == CK_FALSE)
        return CK_FALSE;

    LibHandle = dlopen(EnvLib, RTLD_NOW);
    if (LibHandle)
    {
        C_GetFunctionList = (CK_C_GetFunctionList)dlsym(LibHandle, "C_GetFunctionList");
    }

    if (!LibHandle)
    {
        printf("failed to load %s\n", EnvLib);
    }

    if (C_GetFunctionList)
    {
        rv = C_GetFunctionList(&P11Functions);
    }

    if (P11Functions)
    {
        rv = P11Functions->C_Initialize(NULL_PTR);
    }

    if (rv == CKR_OK)
    {
        myRC = CK_TRUE;
    }

    return myRC;
}

/*
    WLD_RV SendCmdToFm()

    This command sends a command to the HSM adapter selected from the WLD slot list.
    The command is very simple - it passes an embedded slot ID and key handle to the
    sample FM, which in turn will verify that the key exists
*/
WLD_RV SendCmdToFM(uint32_t slotID, uint32_t embeddedSlotID, uint32_t hKey, int *fmErr)
{
    MD_RV mdResult = MDR_UNSUCCESSFUL;
    MD_Buffer_t request[3];
    MD_Buffer_t reply;

    uint32_t appState = 0;
    uint32_t recvlen = 0;
    uint32_t eSlot = embeddedSlotID;
    uint32_t hkey = hKey;

    // Set the Request buffers
    eSlot = fm_htobe32(eSlot);
    request[0].pData = (uint8_t *)&eSlot;
    request[0].length = sizeof(eSlot);

    hkey = fm_htobe32(hkey);
    request[1].pData = (uint8_t *)&hkey;
    request[1].length = sizeof(hkey);

    request[2].pData = NULL;
    request[2].length = 0;

    // And the reply buffer
    reply.pData = NULL;
    reply.length = 0;

    mdResult = SendWLDMessageToFM(slotID,
        FM_NUMBER_CUSTOM_FM,
        request,
        0,
        &reply,
        &recvlen,
        &appState);

    if (mdResult != MDR_OK)
    {
        printf("MD_SendReceive failed: %x\n", (unsigned int)mdResult);
    }
    else
    {
        *fmErr = (int)appState;
    }

    return mdResult;
}


/*
    CK_RV PerformFMFunction()

    This function demonstrates the use of the GetWLDSlotID() function and
    how to respond to and error back from the SendWLDMessageToFM() function.
    (See the SendCmdToFM function above.)
*/

CK_RV PerformFMFunction(int *fmErr)
{
    CK_RV rv = CKR_OK;
    WLD_RV wldErr = WLDR_OK;
    int cmdErr = 0;
    uint32_t slotID = WLD_NO_SLOT_ID, embeddedSlotID;
    CK_OBJECT_HANDLE hObject;
    CK_SESSION_HANDLE hSession;
    CK_CHAR myAESKey[] = "MyAESKey";
    CK_CHAR pswd[] = "userpin";
    CK_ULONG retCount;
    CK_ATTRIBUTE findAttr = {CKA_LABEL, myAESKey, sizeof(myAESKey)-1};

    while (1)
    {
        if (GetWLDSlotID(&slotID, &embeddedSlotID) == WLDR_NO_SLOT_AVAILABLE)
        {
            rv = CKR_TOKEN_NOT_PRESENT;
            break;
        }
        
        printf("slotID=%d, embeddedSlotID=%d,  ", (int)slotID, (int)embeddedSlotID);

        rv = P11Functions->C_OpenSession(slotID, CKF_RW_SESSION | CKF_SERIAL_SESSION,
            NULL, NULL, &hSession);
        if (rv == CKR_OK)
        {
            rv = P11Functions->C_Login(hSession, CKU_CRYPTO_OFFICER, pswd, sizeof(pswd)-1);
            if (rv != CKR_OK)
                break;
        }
        else
            break;

        // Retrieve the encryption key handle on this slot
        rv = P11Functions->C_FindObjectsInit(hSession, &findAttr, 1);
        if (rv == CKR_OK)
        {
            rv = P11Functions->C_FindObjects(hSession, &hObject, 1, &retCount);
            if (rv != CKR_OK || retCount != 1)
            {
                if (retCount != 1)
                {
                    printf("NO KEY FOUND!");
                    rv = CKR_OBJECT_HANDLE_INVALID;
                }
                break;
            }
        }

        printf("hKey=%d, ", (int)hObject);
        
        // Create FM command block and transmit
        wldErr = SendCmdToFM(slotID, embeddedSlotID, (uint32_t)hObject, &cmdErr);
        if (wldErr == WLDR_MD_CMD_ERROR)
            rv = CKR_FUNCTION_FAILED;
        
        *fmErr = cmdErr;

        (void) P11Functions->C_CloseSession(hSession);

        // Command was sent successfully to FM - break from loop
        if (rv == CKR_OK)
            break;
    }

    return rv;
}

/*
    int main()

    This function is the main entry point for the sample that
    demonstrates use of the workload distribution (WLD) logic
*/
int main(int argc, char* argv[])
{
    int rc = -1;
    CK_RV rv = CKR_TOKEN_NOT_PRESENT;
    WLD_RV wldErr;
    MD_RV mdErr;
    CK_ULONG iterations = 20;
    int fmErr;
    int i;

    if (LoadP11Functions() == CK_FALSE)
    {
        printf("Failed to load PKCS11 library!\n");
        goto doneMain;
    }

    if (argc < 2)
    {
        printf("\nUsage: fmtest <#iterations>\n");
        goto doneMain;
    }
    else
        iterations = (CK_ULONG)atoi(argv[1]);

    // Initialize the MD interface
    mdErr = MD_Initialize();
    if (mdErr != MDR_OK)
        goto doneMain;

    // Initialize WLD Slot (or Adapter) list
    wldErr = InitializeWLD(NULL, 0);
    if (wldErr == WLDR_NO_SLOT_AVAILABLE || wldErr == WLDR_NO_SLOTLIST_DEFINED)
    {
        printf("\nERROR: No FM Slots and/or Adapters available - wldErr=%d \n", (int)wldErr);
        goto doneMain;
    }

    printf("\nStarting %d iterations of special FM function:\n", (int)iterations);

    for (i=0; i < iterations; i++)
    {
        printf("\ntest %d: ", i);
        rv = PerformFMFunction(&fmErr);
        printf("...done, rv=%x\n", fmErr);

        if (rv != CKR_OK)
            break;
    }

doneMain:

    printf("\nAll done!\n");

    if (P11Functions)
    {
        P11Functions->C_Finalize(NULL_PTR);
    }

    if (LibHandle)
    {
        dlclose(LibHandle);
    }

    return rc;
}
