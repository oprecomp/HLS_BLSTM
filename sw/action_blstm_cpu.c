/****************************************************************************
   Copyright 2017 - The OPRECOMP Project Consortium,
                    IBM Research GmbH, University of Kaiserslautern, 
                    All rights reserved.
                    
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
 * @file action_blstm_cpu.c
 * @author Dionysios Diamantopoulos, did@zurich.ibm.com, FPGA and PULP porting
 * @author Vladimir Rybalkin, rybalkin@eit.uni-kl.de, BLSTM code
 * @date 9 Jan 2018
 * @brief The software action for BLSTM code. It uses a buffer for passing input
 * images to the main BLSTM kernel function. OpenMP is used for parallelization.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <endian.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libsnap.h>
#include <linux/types.h>	/* __be64 */
#include <asm/byteorder.h>

#include <snap_internal.h>
#include <snap_tools.h>
#include <action_blstm.h>
#include <assert.h>
#include <omp.h>

#include "../../actions/hls_blstm/include/common_def.h"
#include "./include/neuron.h"


static int mmio_write32(struct snap_card *card,
			uint64_t offs, uint32_t data)
{
	act_trace("  %s(%p, %llx, %x)\n", __func__, card,
		  (long long)offs, data);
	return 0;
}

static int mmio_read32(struct snap_card *card,
		       uint64_t offs, uint32_t *data)
{
	act_trace("  %s(%p, %llx, %x)\n", __func__, card,
		  (long long)offs, *data);
	return 0;
}


/**
 * @brief The software action for BLSTM code. It uses a buffer for passing input
 * images to the main BLSTM kernel function. OpenMP is used for parallelization.
 * @param action The SNAP action struct. In CPU-software action it is used only
 * to pass success/fail parameters.
 * @param job Pointer to struct with pointers to I/O buffers.
 * @param job_len The length of job in bytes.
 * @return 'O' upon success. Any errors are registerd in action->job.retc.
 */
static int action_main(struct snap_sim_action *action,
		       void *job, unsigned int job_len)
{
	struct blstm_job *js = (struct blstm_job *)job;
	float *src;
	unsigned int *dst;
	size_t len_in, len_out;
	unsigned int i, j, k;
  unsigned int total_pixels_in_action;

	/* No error checking ... */
	act_trace("%s(%p, %p, %d) type_in=%d type_out=%d jobsize %ld bytes\n",
		  __func__, action, job, job_len, js->in.type, js->out.type,
		  sizeof(*js));

	//__hexdump(stderr, js, sizeof(*js));

    const unsigned int imgs_cols_regs_on_AXIl = sizeof(js->imgcols.cols)/sizeof(js->imgcols.cols[0]);
    uint16_t cols[8];
    unsigned int imgs = 0, size_from_cols_reg = 0;
    static unsigned int action_id = 0;
    for ( i = 0; i < imgs_cols_regs_on_AXIl; i++ ) {
			cols[i] = js->imgcols.cols[i];
			size_from_cols_reg += cols[i];
			if (cols[i] != 0)
				imgs++;
    }
	len_in = js->in.size;
	len_out = js->out.size;
	dst = (unsigned int *)(unsigned long)js->out.addr;
	src = (float *)(unsigned long)js->in.addr;

	act_trace("   copy %p to %p %ld bytes\n", src, dst, len_in);

	if (DEBUG_LEVEL >= LOG_INFO) fprintf(stdout, "INFO: Total images: %u\n", imgs);
	for ( i = 0; i < imgs; i++ )
		if (DEBUG_LEVEL >= LOG_INFO) fprintf(stdout, "INFO: img[%u]:%u columns\n", i, cols[i]);


  /* Allocate buffers for fw/bw images */
	float **image_fw = (float**)malloc(imgs*sizeof(float*));
	assert (image_fw != NULL);
	float **image_bw = (float**)malloc(imgs*sizeof(float*));
	assert (image_bw != NULL);
	unsigned int **vecPredictedStringInd = (unsigned int**)malloc(imgs*sizeof(unsigned int*));
	assert (vecPredictedStringInd != NULL);
	unsigned int *vecPredictedStringLen = (unsigned int*)malloc(imgs*sizeof(unsigned int));
	assert (vecPredictedStringInd != NULL);


	for ( i = 0; i < imgs; i++ ) {
		image_fw[i] = (float*)malloc(cols[i]*HIGHT_IN_PIX*sizeof(float));
		assert (image_fw[i] != NULL);
		image_bw[i] = (float*)malloc(cols[i]*HIGHT_IN_PIX*sizeof(float));
		assert (image_bw[i] != NULL);
		vecPredictedStringInd[i] = (unsigned int*)malloc(MAX_PREDICTED_STRING_LENGTH*sizeof(unsigned int));
		assert (vecPredictedStringInd[i] != NULL);
	}

	// Emulating an auxiliary kernel, adding all float inputs from src and write the integer part to dst
	if (DEBUG_LEVEL >= LOG_INFO) fprintf(stdout, "INFO: Receiving %ld float inputs of size %ld from src (%p) and write output (up)to %ld ints of size %ld on dst (%p)\n",
			len_in/sizeof(float), len_in, src, len_out/sizeof(unsigned int), len_out, dst);

	total_pixels_in_action = 0;
  for ( i = 0; i < imgs; i++ ) {
		memcpy(image_fw[i], src + total_pixels_in_action, (cols[i] * HIGHT_IN_PIX) * sizeof(float));
		memcpy(image_bw[i], src + total_pixels_in_action + cols[i] * HIGHT_IN_PIX , (cols[i] * HIGHT_IN_PIX) * sizeof(float));
		//for ( j = 0; j < (cols[i] * HIGHT_IN_PIX); j++ ) {
		//	image_fw[i][j] = src[total_pixels_in_action + j];
		//	image_bw[i][j] = src[total_pixels_in_action + cols[i] * HIGHT_IN_PIX + j];
		//}
			total_pixels_in_action += 2 * cols[i] * HIGHT_IN_PIX;
	}
	/*
    for ( i = 0; i < imgs; i++ ) {
			if (DEBUG_LEVEL >= LOG_DEBUG) printf("Debug in: action_id:%u, img:%u contains %u columns\n", action_id, i, cols[i]);
        for ( j = 0; j < cols[i]; j++ ) {
            for ( k = 0; k < HIGHT_IN_PIX; k++ ) {
                // do something for a single pixel (bw and fw)
								printf("DEBUG: src[%u]=%f\n", (i+1)*(j*HIGHT_IN_PIX+k), src[(i+1)*(j*HIGHT_IN_PIX+k)]);
								//image_fw[i][j*HIGHT_IN_PIX+k] = src[(i+1)*(j*HIGHT_IN_PIX+k)];
								//image_bw[i][j*HIGHT_IN_PIX+k] = src[(i+1)*((j+cols[i])*(HIGHT_IN_PIX)+k)];
                printf("DEBUG: image_fw[%d][%u]=%f\n", i, j*HIGHT_IN_PIX+k, image_fw[i][j*HIGHT_IN_PIX+k]);
								printf("DEBUG: image_bw[%d][%u]=%f\n", i, j*HIGHT_IN_PIX+k, image_bw[i][j*HIGHT_IN_PIX+k]);
            }
        }
		}
	*/

	#pragma omp parallel
	#pragma omp for schedule(dynamic)
	for ( i = 0; i < imgs; i++ ) {
		if (DEBUG_LEVEL >= LOG_DEBUG) printf("Debug out: action_id:%u, img:%u contains %u columns\n", action_id, i, cols[i]);
		Single_Kernel_BLSTM(
			image_fw[i],
			image_bw[i],
			cols[i],
			vecPredictedStringInd[i],
			&vecPredictedStringLen[i]);
	}

	k=0;
	for ( i = 0; i < imgs; i++ ) {
    js->imgstrlen.cols[i] = vecPredictedStringLen[i];
    for ( j = 0; j < vecPredictedStringLen[i]; j++ ) {
        /* Ensure that the character ids are within known limits */
        dst[k] = MIN(vecPredictedStringInd[i][j], (unsigned int)(NUMBER_OF_CLASSES-1));
        //printf("DEBUG: dst[%u]=%u\n", k, dst[k]);
				k++;
    }
	}

	for ( i = 0; i < imgs; i++ ) {
		free(image_fw[i]);
		free(image_bw[i]);
	}
	free(image_fw);
  free(image_bw);

  /* Keep record of action call among several iterations (static) */
  action_id++;

  /* Emulating success execution of process-accelerator */
  js->status = PROCESS_EXECUTED;

	action->job.retc = SNAP_RETC_SUCCESS;
	return 0;

}

static struct snap_sim_action action = {
	.vendor_id = SNAP_VENDOR_ID_ANY,
	.device_id = SNAP_DEVICE_ID_ANY,
	.action_type = BLSTM_ACTION_TYPE,

	.job = { .retc = SNAP_RETC_FAILURE, },
	.state = ACTION_IDLE,
	.main = action_main,
	.priv_data = NULL,	/* this is passed back as void *card */
	.mmio_write32 = mmio_write32,
	.mmio_read32 = mmio_read32,

	.next = NULL,
};

static void _init(void) __attribute__((constructor));

static void _init(void)
{
	snap_action_register(&action);
}
