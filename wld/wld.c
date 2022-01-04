/*
    wld.c 

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
#include <stdbool.h>
#include <pthread.h>

#include "wld.h"

static WLD_PARTITION_LOOKUP WLD_PartitionTable[MAX_WLD_PARTITIONS];
static uint32_t WLD_PartitionCount = 0;
static uint32_t WLD_CurrentPartitionIndex = 0;
static bool InWLDMode = false;

static pthread_mutex_t wld_mutex;
static int defaultHSM = 3;

// Enable this define to print the contents of the WLD_PartitionTable
// #define DEBUG_WLD 1

// Get the index for the Partition table for this slot
static uint32_t getWLD_HSMIndexFromSlot(uint32_t slotID)
{
    uint32_t i;

    // Create a critical section for this function
    pthread_mutex_lock(&wld_mutex);

    for(i=0; i < WLD_PartitionCount; i++)
    {
        if (slotID == WLD_PartitionTable[i].slot)
            break;
    }

    pthread_mutex_unlock(&wld_mutex);
    return i;
}

// Set all the slots for this adapter to inactive
static void SetHSMInactive(uint32_t adapter)
{
    uint32_t i;

    // Create a critical section for this function
    pthread_mutex_lock(&wld_mutex);

    for(i=0; i < WLD_PartitionCount; i++)
    {
        if (adapter == WLD_PartitionTable[i].hsmID)
        {
            WLD_PartitionTable[i].active = false;
        }
    }

    pthread_mutex_unlock(&wld_mutex);
    return;
}

// Initalize the WLD_PartitionTable
WLD_RV InitializeWLD(uint32_t *pSlotList, uint32_t numSlots)
{
    WLD_RV rv = WLDR_NO_SLOT_AVAILABLE;
    MD_RV mdResult = MDR_OK;
    HsmState_t hsmState;
    char *WLD_EnvStr = NULL;
    char *part;
    uint32_t i;
    unsigned long int embSlot;

    // If the WLD has been initialized an error will be returned, but 
    // may be ignored by the calling function.
    if (InWLDMode)
        return WLDR_WLD_ALREADY_INITIALIZED;


    // First zeroize the Partition Lookup Table
    memset(WLD_PartitionTable, 0, sizeof(WLD_PartitionTable));
    WLD_PartitionCount = 0;

    // Check for the enviroment variable and if it exsists,
    // parse out the configured WLD partitions - if all is good
    // set the InWLDMode flag to TRUE
    WLD_EnvStr = getenv( "WLD_SLOT_LIST" );
    if (WLD_EnvStr == NULL && pSlotList == NULL)
    {
        rv = WLDR_NO_SLOTLIST_DEFINED;
    }
    else
    {
        InWLDMode = true;
        if (pSlotList)
        {
            for (i=0; i < MAX_WLD_PARTITIONS && i < numSlots; i++)
            {
                WLD_PartitionTable[i].slot = pSlotList[i];
                WLD_PartitionCount++;
            }
        }
        else
        {
            part = strtok(WLD_EnvStr, " ,");
            for (i=0; i < MAX_WLD_PARTITIONS && part != NULL; i++)
            {
                WLD_PartitionTable[i].slot = atoi(part);
                WLD_PartitionCount++;
                part = strtok(NULL, " ,");
            }
        }
    }

#if DEBUG_WLD
    printf("\n\nWLD Env set = %s, count=%d, partitions: ", WLD_EnvStr, WLD_PartitionCount);

    for (i=0; i < WLD_PartitionCount; i++)
    {
        printf("%d,", WLD_PartitionTable[i].slot);
    }
    printf("\n");
#endif

    // For each partition in the table, determine which adapter (hsmID)
    // the partition targets, the status of the adapter (and partition)
    // and the embedded slot number in the FM that corresponds to this
    // partition
    if (InWLDMode)
    {
        for (i=0; i < WLD_PartitionCount; i++)
        {
            // First get the adapter (hsmID) for this partition
            mdResult = MD_GetHsmIndexForSlot(WLD_PartitionTable[i].slot,
                &WLD_PartitionTable[i].hsmID);
            if (mdResult == MDR_OK)
            {
                // Check the state of the HSM
                mdResult = MD_GetHsmState(WLD_PartitionTable[i].hsmID, &hsmState, NULL);
            }

            if (mdResult == MDR_OK && hsmState == S_NORMAL_OPERATION)
            {
                // Now get the embedded slot number (on that adapter) for this partition
                mdResult = MD_GetEmbeddedSlotID(WLD_PartitionTable[i].slot,
                    &embSlot);
                if (mdResult == MDR_OK)
                {
                    // We have an active partition - mark it so
                    WLD_PartitionTable[i].embeddedSlot = (uint32_t)embSlot;
                    WLD_PartitionTable[i].active = true;
                }
            }
            else
            {
                printf("\nError setting up WLD Table: mdResult=%x, hsmState=%x\n",
                    mdResult, hsmState);
            }
        }

        // Let's make sure we have at least one active slot in the table
        // Othewise return an error
        for (i=0; i < WLD_PartitionCount; i++)
        {
            if (WLD_PartitionTable[i].active == true)
            {
                rv = WLDR_OK;
                break;
            }
        }

#if DEBUG_WLD
        printf("\n\nWLD_ParititonTable setup complete: count=%d, ret=%x\n",
            WLD_PartitionCount, mdResult);

        for (i=0; i < WLD_PartitionCount; i++)
        {
            printf("WLD Partitions: part=%d, active=%s, hsmID=%d, embSlot=%d\n",
                WLD_PartitionTable[i].slot,
                WLD_PartitionTable[i].active ? "yes" : "no",
                WLD_PartitionTable[i].hsmID,
                WLD_PartitionTable[i].embeddedSlot);
        }
        printf("\n");
#endif
    }

    return rv;
}

// Get the next available active slot
WLD_RV GetWLDSlotID(uint32_t *pSlotID, uint32_t *pEmbeddedSlotID)
{
    WLD_RV rv = WLDR_OK;
    uint32_t index;
    bool slotFound = false;

    if (!InWLDMode)
        return WLDR_NO_SLOTLIST_DEFINED;

    if (!pSlotID)
        return WLDR_NO_SLOT_AVAILABLE;

    // Create a critial section for remainder of this function
    pthread_mutex_lock(&wld_mutex);

    index = WLD_CurrentPartitionIndex;

    // Increment the current index and return the first slot ID (and
    // embedded slot ID if the pointer is non-NULL) that is active.
    // If we return back to the original index, no active slots were
    // found so return an error
    do
    {
        index++;
        if (index == WLD_PartitionCount)
        {
            index = 0;
        }

        if (WLD_PartitionTable[index].active)
        {
            *pSlotID = WLD_PartitionTable[index].slot;
            slotFound = true;
            if (pEmbeddedSlotID)
                *pEmbeddedSlotID = WLD_PartitionTable[index].embeddedSlot;
            break;
        }
    } while(index != WLD_CurrentPartitionIndex);

    if (!slotFound)
    {
        rv = WLDR_NO_SLOT_AVAILABLE;
    }
    else
        WLD_CurrentPartitionIndex = index;

    // Release the mutex for this section
    pthread_mutex_unlock(&wld_mutex);

    return rv;
}

// This function is a wrapper around the MD_SendReceive function
// If the WLD_NO_SLOT_ID slot number is passed in (i.e. any slot
// can be used) then the function will try to replay the op if a
// particular adapter fails. Otherwise it will simply set the
// adapter to inactive and return the MD error code
MD_RV SendWLDMessageToFM(uint32_t slotID,
    uint16_t fmNumber,
    MD_Buffer_t *pReq,
    uint32_t timeout,
    MD_Buffer_t *pResp,
    uint32_t *pReceivedLen,
    uint32_t *pFMStatus)
{
    MD_RV mdResult = MDR_OK;
    WLD_RV wldErr = WLDR_OK;
    uint32_t adapter = defaultHSM;
    uint32_t appState = 0;
    uint32_t originatorID = 0;
    uint32_t recvlen = 0;
    uint32_t index = 0;
    uint32_t slot = slotID;

    do
    {
        // If slotID == WLD_NO_SLOT_ID (i.e. the application
        // doesn't care which slot is used) then get the 
        // next available slot and try it.  If it fails, loop
        // around again and try another slot.
        if (slotID == WLD_NO_SLOT_ID)
        {
            wldErr = GetWLDSlotID(&slot, NULL);
            if (wldErr != WLDR_OK)
            {
                // Either no slot list defined or none available -
                // break out of loop in either case
                mdResult = MDR_INVALID_HSM_INDEX;
                break;
            }
        }

        if (mdResult == MDR_OK)
        {
            // Get the HsmID associated with this slot number
            index = getWLD_HSMIndexFromSlot(slot);
            if (index < WLD_PartitionCount)
            {
                adapter = WLD_PartitionTable[index].hsmID;
                mdResult = MD_SendReceive( adapter,
                            originatorID,
                            fmNumber,
                            pReq,
                            0,
                            pResp,
                            &recvlen,
                            &appState);
            }

            if (mdResult == MDR_OK)
            {
                *pReceivedLen = recvlen;
                *pFMStatus = appState;
                break;
            }
            else if (mdResult == MDR_UNSUCCESSFUL ||
                mdResult == MDR_INTERNAL_ERROR)
            {
                // Set this adapter as inactive
                SetHSMInactive(adapter);
            }
            // Any other MD error should be returned to the
            // application to be handled appropriately
            else
                break;
        }
    } while (slotID == WLD_NO_SLOT_ID); // Only loop for this slotID setting

    return mdResult;
}
