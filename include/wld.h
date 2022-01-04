/*
    wld.h 

    This file provides source code for a sample implementation of
    a workload distribution (WLD) model that allows for multiple FM 
    function calls to be distributed over a group of HSM adapters.
    This code is sample ONLY and Thales Inc. assumes no liability
    or responsibility for its correct operation.  Refer to the 
    Application Guide and readme file for a desription of its use.
*/


#ifndef _WLD_H_
#define _WLD_H_

#undef UNICODE

#include <stdbool.h>
#include <fm/host/stdint.h>
#include <fm/host/md.h>

#define WLD_NO_SLOT_ID 9999
#define MAX_WLD_PARTITIONS 20

// WLD Error Codes
#define WLDR_OK                         0
#define WLDR_NO_SLOT_AVAILABLE          1
#define WLDR_NO_SLOTLIST_DEFINED        2
#define WLDR_WLD_ALREADY_INITIALIZED    3
#define WLDR_MD_CMD_ERROR               4

typedef unsigned long int WLD_RV;

typedef struct WLD_PARTITION_LOOKUP {
    uint32_t slot;
    bool active;
    uint32_t embeddedSlot;
    uint32_t hsmID;
} WLD_PARTITION_LOOKUP;

WLD_RV InitializeWLD(uint32_t *pSlotList, uint32_t numSlots);

WLD_RV GetWLDSlotID(uint32_t *pSlotID, uint32_t *pEmbeddedSlotID);

MD_RV SendWLDMessageToFM(uint32_t slotID,
    uint16_t fmNumber,
    MD_Buffer_t *pReq,
    uint32_t timeout,
    MD_Buffer_t *pResp,
    uint32_t *pReceivedLen,
    uint32_t *pFMStatus);

#endif