/****************************************************************************
   Copyright 2018 - The OPRECOMP Project Consortium,
                    IBM Research GmbH,
										University of Kaiserslautern - Microelectronic Systems
										Design Research Group, All rights reserved.

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
 * @file action_blstm.h
 * @author Dionysios Diamantopoulos, did@zurich.ibm.com
 * @date 5 Oct 2017
 * @brief The action-related structs for BLSTM
 * */


#ifndef __ACTION_BLSTM_H__
#define __ACTION_BLSTM_H__

#include <snap_types.h>

#ifdef __cplusplus
extern "C" {
#endif

//#define HELLOWORLD_ACTION_TYPE 0x10141008
//#define BLSTM_ACTION_TYPE 0x00000101
#define BLSTM_ACTION_TYPE 0x10141008

/* Enumerator holding accelerator status and error information.
 * The 2-LSB bytes are keeping status information.
 * The 2-MSB bytes are keeping error information.
 * May change to any-value assignment, but keeping distinct
 * bit-unique values to enable reporting for flow-control (debug)
 * */
typedef enum {						   /* Bits  :   Value   */
	ACTION_CALL 			= 0x00,    /* 7:0   : 0000 0000 */
	ACTION_DISCOVERED 		= 0x01,    /* 7:0   : 0000 0001 */
	PROCESS_INIT 			= 0x02,    /* 7:0   : 0000 0010 */
	ACC_INIT				= 0x04,    /* 7:0   : 0000 0100 */
	ACC_DATA_FED			= 0x08,    /* 7:0   : 0000 1000 */
	ACC_EXECUTED			= 0x10,    /* 7:0   : 0001 0000 */
	ACC_DATA_RETURNED		= 0x20,    /* 7:0   : 0010 0000 */
	PROCESS_EXECUTED		= 0x40,    /* 7:0   : 0100 0000 */
	ACC_ERR_MAX_RET			= 0x10000, /* 23:16 : 0000 0001 */
	ACC_ERR_SIZE_MISMATCH	= 0x20000, /* 23:16 : 0000 0010 */
} status_t;

typedef struct simgcols {
        /* Keep the struct aligned to 64 bits */
		uint16_t cols[8];
} simgcols_t;

typedef struct blstm_job {
	struct snap_addr in;		/* input data */
	struct snap_addr out;   	/* offset table */
	uint32_t status;			/* out:  4 bytes - the status of accelerator */
	uint32_t strlen;			/* out:  4 bytes - the number of alphabet id's found in current stream */
	uint32_t axitrans_in;		/* inout:4 bytes - the number of axi transactions completed on feeding data */
	uint32_t axitrans_out;		/* inout:4 bytes - the number of axi transactions completed on returning data */
	//uint64_t pad;
	struct simgcols imgcols;	/* struct holding the columns of every image */
	struct simgcols imgstrlen;	/* struct holding the returned strlen of every image */
} blstm_job_t;

#ifdef __cplusplus
}
#endif

#endif	/* __ACTION_BLSTM_H__ */
