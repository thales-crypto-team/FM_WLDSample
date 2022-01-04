/****************************************************************************\
*
* File: hdr.c
*
* Description: FM Identification structure
*
* Copyright © 2018 - 2021 SafeNet. All rights reserved.

* This file contains information that is
* proprietary to SafeNet, and may not be distributed
* or copied without written consent from SafeNet.
*
\****************************************************************************/

#include <fm/hsm/mkfmhdr.h>

#define FM_VERSION 0x0100 /* V1.00 */
#define FM_SER_NO  0
#define FM_MANUFACTURER "Gemalto Inc"
#define FM_NAME "WLD_SAMPLE"

DEFINE_FM_HEADER(FM_NUMBER_CUSTOM_FM,
		FM_VERSION,
		FM_SER_NO,
		FM_MANUFACTURER,
		FM_NAME);

