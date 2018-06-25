/****************************************************************************
   Copyright 2018 - The OPRECOMP Project Consortium,
                    IBM Research GmbH. All rights reserved.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
****************************************************************************/

/**
 * @file action_hls.H
 * @author Dionysios Diamantopoulos, did@zurich.ibm.com
 * @date 19 Nov 2017
 * @brief The action-related structs and functions for BLSTM.
 */


#ifndef __ACTION_HW_BLSTM_H__
#define __ACTION_HW_BLSTM_H__


#ifndef HOST_SW
#include <stdint.h>
#include <string.h>
#ifndef FORMAT_IS_FLOAT
#include <ap_int.h>
#endif /* FORMAT_IS_FLOAT */
#include "hls_snap.H"
#endif
#include <action_blstm.h> /* BLSTM Job definition */
#define RELEASE_LEVEL		0x00000021

#define MAX_NB_OF_BYTES_READ	(256 * 1024)
#define CARD_DRAM_SIZE		(1 * 1024 *1024 * 1024)
#define MAX_NB_OF_WORDS_READ	(MAX_NB_OF_BYTES_READ/BPERDW)

typedef char word_t[BPERDW];
typedef float fword_t[BPERDW/4];

//---------------------------------------------------------------------
typedef struct {
	CONTROL Control;						/*  16 bytes */
	blstm_job_t Data;				 		/* 108 bytes : job : 27 x 32 bit registers*/
	uint8_t padding[SNAP_HLS_JOBSIZE - sizeof(blstm_job_t)];	/* Padding to SNAP_HLS_JOBSIZE = 108 bytes */
} action_reg; 								/* 108 + 16 = 124 bytes*/


void hls_action(snap_membus_t *din_gmem,
		snap_membus_t *dout_gmem,
		/* snap_membus_t *d_ddrmem, */ /* if unused => export SDRAM_USED=FALSE */
		action_reg *act_reg,
		action_RO_config_reg *Action_Config);

#endif  /* __ACTION_HW_BLSTM_H__*/
