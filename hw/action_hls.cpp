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
 * @file action_hls.cpp
 * @author Dionysios Diamantopoulos, did@zurich.ibm.com, FPGA and PULP porting
 * @author Vladimir Rybalkin, rybalkin@eit.uni-kl.de, BLSTM code
 * @date 9 Jan 2018
 * @brief Original template version for adapting bmstm into SW "action"
 */

#include <stdlib.h>
#include <cstddef>
#include <string.h>
#ifndef FORMAT_IS_FLOAT
#include <ap_int.h>
#endif /* FORMAT_IS_FLOAT */
#include "action_hls.H"

#include "neuron.hpp"


unsigned int global_a = 0;
unsigned int write_fw = 0, write_bw = 0;


DTYPE_IMG bytesToFloatA(char b0, char b1, char b2, char b3)
{
#pragma HLS INLINE
    DTYPE_IMG ret_value = 0;
#if IMG_FLOAT_TO_FIXED_CASTING_IN_CPU == 1
    *((char*)(&ret_value) + 0) = b0;
#else
    float output;
    *((char*)(&output) + 3) = b3;
    *((char*)(&output) + 2) = b2;
    *((char*)(&output) + 1) = b1;
    *((char*)(&output) + 0) = b0;
    ret_value  = static_cast<DTYPE_IMG>(output);
#endif
    return ret_value ;
}

// Cast a word from input port (512b) to a char* word (64B)
void mbus_to_stream(snap_membus_t mem,
#ifdef INTERFACE_IS_STREAM
#ifdef MANY_STREAMS_FOR_MANY_ACCS
							hls::stream<DTYPE_IMG> image_fw[ACC_CALLS_PER_ACTION],
							hls::stream<DTYPE_IMG> image_bw[ACC_CALLS_PER_ACTION],
#else /* MANY_STREAMS_FOR_MANY_ACCS */
							hls::stream<DTYPE_IMG> &image_fw,
							hls::stream<DTYPE_IMG> &image_bw,
#endif /* MANY_STREAMS_FOR_MANY_ACCS */
#else /* INTERFACE_IS_STREAM */
#ifdef MANY_STREAMS_FOR_MANY_ACCS
							DTYPE_IMG image_fw[ACC_CALLS_PER_ACTION][MAX_NUMBER_COLUMNS_TEST_SET * HIGHT_IN_PIX],
							DTYPE_IMG image_bw[ACC_CALLS_PER_ACTION][MAX_NUMBER_COLUMNS_TEST_SET * HIGHT_IN_PIX],
#else /* MANY_STREAMS_FOR_MANY_ACCS */
							DTYPE_IMG image_fw[ACC_CALLS_PER_ACTION * MAX_NUMBER_COLUMNS_TEST_SET * HIGHT_IN_PIX],
							DTYPE_IMG image_bw[ACC_CALLS_PER_ACTION * MAX_NUMBER_COLUMNS_TEST_SET * HIGHT_IN_PIX],
#endif /* MANY_STREAMS_FOR_MANY_ACCS */
							uint32_t *addr_fw,
							uint32_t *addr_bw,
#endif /* INTERFACE_IS_STREAM */
							uint32_t *pixels_fed,
							uint32_t pixels_all
#if ACC_CALLS_PER_ACTION > 1
							,uint32_t *pixels_fed_curr_img,
							uint32_t *pixels_curr_img,
							uint16_t cols[8],
							uint32_t *img_id
#endif /* ACC_CALLS_PER_ACTION */
							)
{
#pragma HLS INLINE off

	assert(sizeof(float)==4);
    snap_membus_t tmp = mem;

#if ACC_CALLS_PER_ACTION == 1
    const uint32_t img_id_val = 0;
    const uint32_t *img_id = &img_id_val;
#endif /* ACC_CALLS_PER_ACTION */

	union {
    	float f;
    	char b[sizeof(float)];
    } u;
    //u.f = 0.0;
    DTYPE_IMG f;
	char b0,b1,b2,b3;

    loop_mbus_to_word:
    for (unsigned char k = 0; k < sizeof(word_t); k+=4) {
    //#pragma HLS UNROLL
    	b0 = u.b[0] = tmp(7, 0);
        b1 = u.b[1] = tmp(15, 8);
        b2 = u.b[2] = tmp(23, 16);
        b3 = u.b[3] = tmp(31, 24);
        f = bytesToFloatA(b0,b1,b2,b3);
        tmp = tmp >> 32;

    	//std::cout << "DEBUG in: global_a = " << global_a++ << ", pixels_all = " << pixels_all << \
    			", *pixels_fed = " << *pixels_fed << " ";

        if (*pixels_fed != pixels_all) {
#if ACC_CALLS_PER_ACTION > 1
        	if (*pixels_fed_curr_img == *pixels_curr_img) {
        		//std::cout << "pixels_curr_img = " << *pixels_curr_img << ", pixels_fed_curr_img = " << *pixels_fed_curr_img << ", write_fw = " << write_fw << ", write_bw = " << write_bw << std::endl;
        		*img_id = *img_id + 1;
        		*pixels_curr_img = cols[*img_id] * 2 * HIGHT_IN_PIX;
    			*pixels_fed_curr_img = 0;
#ifndef INTERFACE_IS_STREAM
#ifdef MANY_STREAMS_FOR_MANY_ACCS
    			*addr_fw = *addr_bw = 0;
#endif /* MANY_STREAMS_FOR_MANY_ACCS */
#endif /* INTERFACE_IS_STREAM */
        	}

        	if (*pixels_fed_curr_img < *pixels_curr_img) {
        		if (*pixels_fed_curr_img < (*pixels_curr_img)>>1) {
#else /* ACC_CALLS_PER_ACTION */
            if (*pixels_fed < pixels_all) {
            	if (*pixels_fed < (pixels_all)>>1) {
#endif /* ACC_CALLS_PER_ACTION */
#ifdef INTERFACE_IS_STREAM
#ifdef MANY_STREAMS_FOR_MANY_ACCS
        			image_fw[*img_id].write(f);
        			//cast.f = (DTYPE_IMG)u.f;
        			//printf("write fw : %f in DTYP is %f (0x%x)\n", u.f, f, *b);
#else /* MANY_STREAMS_FOR_MANY_ACCS */
        			image_fw.write((DTYPE_IMG)u.f);
        			write_fw++;
        			//std::cout << "write fw : " << u.f << std::endl;
#endif /* MANY_STREAMS_FOR_MANY_ACCS */
#else /* INTERFACE_IS_STREAM */
#ifdef MANY_STREAMS_FOR_MANY_ACCS
        			image_fw[*img_id][*addr_fw] = (DTYPE_IMG)u.f;
        			*addr_fw = *addr_fw + 1;
#else /* MANY_STREAMS_FOR_MANY_ACCS */
        			//std::cout << " DEBUG *addr_fw = " << *addr_fw << std::endl;
        			image_fw[*addr_fw] = (DTYPE_IMG)u.f;
        			*addr_fw = *addr_fw + 1;
#endif /* MANY_STREAMS_FOR_MANY_ACCS */

#endif
        		}
        		else {
#ifdef INTERFACE_IS_STREAM
#ifdef MANY_STREAMS_FOR_MANY_ACCS
        			image_bw[*img_id].write(f);
#else /* MANY_STREAMS_FOR_MANY_ACCS */
        			image_bw.write((DTYPE_IMG)u.f);
        			write_bw++;
        			//std::cout << "write bw : " << u.f << std::endl;
#endif /* MANY_STREAMS_FOR_MANY_ACCS */
#else /* INTERFACE_IS_STREAM */
#ifdef MANY_STREAMS_FOR_MANY_ACCS
        			image_bw[*img_id][*addr_bw] = (DTYPE_IMG)u.f;
        			*addr_bw = *addr_bw + 1;
#else /* MANY_STREAMS_FOR_MANY_ACCS */
        			//std::cout << " DEBUG *addr_bw = " << *addr_bw << std::endl;
        			image_bw[*addr_bw] = (DTYPE_IMG)u.f;
        			*addr_bw = *addr_bw + 1;
#endif /* MANY_STREAMS_FOR_MANY_ACCS */
#endif /* INTERFACE_IS_STREAM */
        		}
#if ACC_CALLS_PER_ACTION > 1
        		*pixels_fed_curr_img = *pixels_fed_curr_img + 1;
#endif /* ACC_CALLS_PER_ACTION */
        	}
        	*pixels_fed = *pixels_fed + 1;
        }
    }
}

// Cast a uint32_t* word (64B) to output port (512b) in little-endian format
static snap_membus_t word_to_mbus_little_endian(uint32_t *vecPredictedStringInd)
{
#pragma HLS INLINE off
	assert(sizeof(float)==4);
	snap_membus_t mem = 0;
	uint32_t index=BPERDW/4-1;
	union {
	  uint32_t i;
      char b[sizeof(float)];
    } u;

    loop_word_to_mbus:
	for (int k = sizeof(word_t)-4; k >= 0; k-=4, index--) {

//#pragma HLS UNROLL
			u.i = vecPredictedStringInd[index];
			mem = mem << 32;
			mem(7, 0)  = u.b[0];
			mem(15,8)  = u.b[1];
			mem(23,16) = u.b[2];
			mem(31,24) = u.b[3];
		//std::cout << mem << "\n";
		//}
	}
	return mem;
}


//----------------------------------------------------------------------
//--- MAIN PROGRAM -----------------------------------------------------
//----------------------------------------------------------------------
void process_action(snap_membus_t din_gmem[2288],
			  snap_membus_t dout_gmem[46],
			  /* snap_membus_t *d_ddrmem, *//* not needed */
			  action_reg *act_reg)
{
#pragma HLS INLINE off

	uint32_t size, bytes_to_transfer, pixels_all, pixels_fed, cnt_trans;
#if ACC_CALLS_PER_ACTION > 1
	uint32_t size_from_cols_reg, pixels_fed_curr_img, pixels_curr_img, imgs, img_id;
#endif /* ACC_CALLS_PER_ACTION */

	uint64_t i_idx, o_idx;

	/* byte address received need to be aligned with port width */
	i_idx = act_reg->Data.in.addr >> ADDR_RIGHT_SHIFT;
	o_idx = act_reg->Data.out.addr >> ADDR_RIGHT_SHIFT;
	size = act_reg->Data.in.size;


	const unsigned int imgs_cols_regs_on_AXIl = sizeof(act_reg->Data.imgcols.cols)/sizeof(act_reg->Data.imgcols.cols[0]);

	/* check that there are enough registers on AXI lite MMIO, so that calls can store the results */
	assert(imgs_cols_regs_on_AXIl >= ACC_CALLS_PER_ACTION);

#if ACC_CALLS_PER_ACTION > 1
	uint16_t cols[imgs_cols_regs_on_AXIl];
#pragma HLS ARRAY_PARTITION variable=cols complete dim=1
	imgs = 0;
	size_from_cols_reg = 0;
	for (unsigned int i = 0; i < imgs_cols_regs_on_AXIl; i++) {
		cols[i] = act_reg->Data.imgcols.cols[i];
		size_from_cols_reg += cols[i];
		if (cols[i] != 0)
			imgs++;
	}
	size_from_cols_reg = size_from_cols_reg << 3; // x2 for bw/fw, x4 for sizeof(float) in bytes
	size_from_cols_reg *= HIGHT_IN_PIX;

	/* check that size on argument call is the actual size of all images reported to AXI registers */
	assert(size == size_from_cols_reg);
	if (size != size_from_cols_reg)
		act_reg->Data.status |= ACC_ERR_SIZE_MISMATCH;

	/* check that current input images are enough for what accelerator is waiting for */
	assert(imgs <= ACC_CALLS_PER_ACTION);
#else
#ifndef VHLS_TB
	uint16_t *cols = act_reg->Data.imgcols.cols;
#else
	uint16_t cols[8] = {1,3,3,3,3,3,3,3};
	for (unsigned int i = 0; i < ACC_CALLS_PER_ACTION; i++ ) {
		if (cols[i] != act_reg->Data.imgcols.cols[i]) {
			printf("cols[%u]=%u != act_reg->Data.imgcols.cols[%u]=%u. Please check for TB only\n", \
					i, cols[i], i, act_reg->Data.imgcols.cols[i]);
			assert(cols[i] == act_reg->Data.imgcols.cols[i]);
		}
	}
#endif /* VHLS_TB */
#endif /* ACC_CALLS_PER_ACTION */

	pixels_all = size/sizeof(float);
	pixels_fed = 0;
	std::cout << "DEBUG: pixels to push to steam: " << pixels_all << "\n";

#ifdef INTERFACE_IS_STREAM
	const int stream_size = STREAM_ACTION_SIZE_IN;
#ifdef MANY_STREAMS_FOR_MANY_ACCS
	hls::stream<DTYPE_IMG> image_fw[ACC_CALLS_PER_ACTION];
	hls::stream<DTYPE_IMG> image_bw[ACC_CALLS_PER_ACTION];
	#pragma HLS STREAM variable=image_fw depth=stream_size dim=2
	#pragma HLS STREAM variable=image_bw depth=stream_size dim=2
#else /* MANY_STREAMS_FOR_MANY_ACCS */
	hls::stream<DTYPE_IMG> image_fw;
	hls::stream<DTYPE_IMG> image_bw;
	#pragma HLS STREAM variable=image_fw depth=stream_size dim=1
	#pragma HLS STREAM variable=image_bw depth=stream_size dim=1
#endif /* MANY_STREAMS_FOR_MANY_ACCS */
#else /* INTERFACE_IS_STREAM */
#ifdef MANY_STREAMS_FOR_MANY_ACCS
	DTYPE_IMG image_fw[ACC_CALLS_PER_ACTION][MAX_NUMBER_COLUMNS_TEST_SET * HIGHT_IN_PIX];
	DTYPE_IMG image_bw[ACC_CALLS_PER_ACTION][MAX_NUMBER_COLUMNS_TEST_SET * HIGHT_IN_PIX];
	uint32_t addr_fw = 0;
	uint32_t addr_bw = 0;
#else /* MANY_STREAMS_FOR_MANY_ACCS */
	DTYPE_IMG image_fw[ACC_CALLS_PER_ACTION * MAX_NUMBER_COLUMNS_TEST_SET * HIGHT_IN_PIX];
	DTYPE_IMG image_bw[ACC_CALLS_PER_ACTION * MAX_NUMBER_COLUMNS_TEST_SET * HIGHT_IN_PIX];
	uint32_t addr_fw = 0;
	uint32_t addr_bw = 0;
#endif /* MANY_STREAMS_FOR_MANY_ACCS */
#endif /* INTERFACE_IS_STREAM */


	//const int stream_buf_size = 2 * MAX_NUMBER_COLUMNS_TEST_SET * HIGHT_IN_PIX * sizeof(float) / sizeof(snap_membus_t);
	//hls::stream<snap_membus_t> stream_buffer_in;
	//snap_membus_t stream_buffer_in[stream_buf_size];

	/* Update MMIO status register */
	act_reg->Data.status |= ACC_INIT;

#if ACC_CALLS_PER_ACTION > 1
	img_id = 0;
	pixels_curr_img = cols[0] * 2 * HIGHT_IN_PIX;
	pixels_fed_curr_img = 0;
#endif /* ACC_CALLS_PER_ACTION */

	cnt_trans = 0;
	main_loop_in:
	while (size > 0) {
		const int min_tripcount = 1;
		const int max_tripcount = 2 * MAX_NUMBER_COLUMNS_TEST_SET * HIGHT_IN_PIX * sizeof(float) / sizeof(snap_membus_t);
#pragma HLS LOOP_TRIPCOUNT min=min_tripcount max=max_tripcount

//#pragma HLS PIPELINE
		snap_membus_t buffer_in = 0;

		/* Limit the number of bytes to process to a 64B word */
		bytes_to_transfer = MIN(size, (uint32_t)sizeof(buffer_in));

		// Temporary workaround due to Xilinx memcpy issue - fixed in HLS 2017.4 */
		//memcpy(&buffer_in, din_gmem + i_idx, sizeof(buffer_in));
		buffer_in = (din_gmem + i_idx)[0];

		//std::cout << "DEBUG in: size = " << size << ", i_idx = " << i_idx << ", bytes_to_transfer = " << bytes_to_transfer << "\n";
		/* cast 64B word buffer to a float[16] img and append to stream fifo */
		mbus_to_stream(	buffer_in,
						image_fw,
						image_bw,
#ifndef INTERFACE_IS_STREAM
						&addr_fw,
						&addr_bw,
#endif
						&pixels_fed,
						pixels_all
#if ACC_CALLS_PER_ACTION > 1
						,&pixels_fed_curr_img,
						&pixels_curr_img,
						cols,
						&img_id
#endif
						);

		//stream_buffer_in.write(buffer_in);
		//stream_buffer_in[cnt_trans] = buffer_in;

		size -= bytes_to_transfer;
		i_idx++;
		cnt_trans++;
	}

	act_reg->Data.axitrans_in = cnt_trans;

	/* Up to this point we have successfully load into fw/bw streams the pixels of image */

	/* Update MMIO status register */
	act_reg->Data.status |= ACC_DATA_FED;

	uint8_t vecPredictedStringInd[ACC_CALLS_PER_ACTION][MAX_PREDICTED_STRING_LENGTH];
	uint8_t vecPredictedStringLen[ACC_CALLS_PER_ACTION];
#pragma HLS ARRAY_PARTITION variable=vecPredictedStringInd complete dim=1
#pragma HLS ARRAY_PARTITION variable=vecPredictedStringLen complete dim=1
	//uint32_t vecPredictedStringIndBuf[ACC_CALLS_PER_ACTION*MAX_NUMBER_COLUMNS_TEST_SET];
	hls::stream<uint32_t> vecPredictedStringIndStream;
	const int str_len_stream_size = ACC_CALLS_PER_ACTION*MAX_PREDICTED_STRING_LENGTH;
#pragma HLS STREAM variable=vecPredictedStringIndStream depth=str_len_stream_size dim=1

	std::cout << "DEBUG: pixels = "<< pixels_all << " numberOfColumns = " << act_reg->Data.imgcols.cols[0] << "\n";

	Many_Kernels_BLSTM(
			 image_fw,
			 image_bw,
			 cols,
			 vecPredictedStringInd,
			 vecPredictedStringLen);

#ifndef INTERFACE_IS_STREAM
#ifndef MANY_STREAMS_FOR_MANY_ACCS
#if ACC_CALLS_PER_ACTION > 1
	// FIXME: shall make it work with arrays also
		printf("ERROR: unsupported option INTERFACE_IS_ARRAY and SINGLE_STREAM_FOR_MANY_ACCS, when ACC_CALLS_PER_ACTION>1. Aborting... \n");
		exit(-1);
#endif
#endif
#endif

	for (unsigned int i = 0; i < ACC_CALLS_PER_ACTION; i++) {
		std::cout << "DEBUG: vecPredictedStringLen[" << i << "] = " << (uint32_t)vecPredictedStringLen[i] << std::endl;
		act_reg->Data.imgstrlen.cols[i] = vecPredictedStringLen[i];

		/* Check that the returned ids can be stored on local memory, else report the error */
		if (vecPredictedStringLen[i] > MAX_PREDICTED_STRING_LENGTH)
			act_reg->Data.status |= ACC_ERR_MAX_RET;
	}

	act_reg->Data.strlen = vecPredictedStringLen[0]; // deprecated

	/* Update MMIO status register */
	act_reg->Data.status |= ACC_EXECUTED;

	//for (unsigned int i = 0; i < PredictedStringLen; i++)
	//	std::cout << "DEBUG: vecPredictedStringInd[" << i << "] = " << vecPredictedStringInd[i] << "\n";

	// Starting forwarding results back to cpu
	// Slicing results to size of 512b and do an AXI transfer at a time
	size = 0;
	merge_results_loop_out:
	for (unsigned int i = 0, index = 0; i < ACC_CALLS_PER_ACTION; i++) {
		size += vecPredictedStringLen[i] << 2; // *= sizeof(uint32_t);
		//std::cout << "DEBUG fw results: vecPredictedStringLen[" << i << "] = " << vecPredictedStringLen[i] << std::endl;
		for (unsigned int j = 0; j < vecPredictedStringLen[i]; j++) {
			const int min_tripcount = 1;
			const int max_tripcount = MAX_PREDICTED_STRING_LENGTH;
#pragma HLS LOOP_TRIPCOUNT min=min_tripcount max=max_tripcount
			//vecPredictedStringIndBuf[index++] = vecPredictedStringInd[i][j];
			vecPredictedStringIndStream.write((uint32_t)vecPredictedStringInd[i][j]);
			//std::cout << "DEBUG fw results: vecPredictedStringIndBuf[" << index << "] = " << vecPredictedStringInd[i][j] << std::endl;
		}
	}
	//std::cout << "DEBUG fw results: size = " << size << std::endl;
	cnt_trans = 0;

	uint32_t axi_world[sizeof(snap_membus_t)/sizeof(uint32_t)];

	main_loop_out:
	while (size > 0) {
		  const int min_tripcount = 1;
		  const int max_tripcount = ACC_CALLS_PER_ACTION * MAX_PREDICTED_STRING_LENGTH * sizeof(uint8_t) / sizeof(snap_membus_t);
#pragma HLS LOOP_TRIPCOUNT min=min_tripcount max=max_tripcount
		//#pragma HLS PIPELINE
		snap_membus_t buffer_out = 0;

		/* Limit the number of bytes to process to a 64B word */
		bytes_to_transfer = MIN(size, (uint32_t)sizeof(buffer_out));

		/* packing 16 4-bytes words into a single */
		for (unsigned int i = 0; i < sizeof(snap_membus_t)/sizeof(uint32_t); i++) {
			if (vecPredictedStringIndStream.empty() == false)
				axi_world[i] = vecPredictedStringIndStream.read();
			else
				axi_world[i] = 0; /* have to explicitly do this ! */
		}

		/* read from stream fifo and cast float[16] img to a 64B word buffer */
		buffer_out = word_to_mbus_little_endian(axi_world); // FIXME: shall change 16 to a static define

		//std::cout << "DEBUG out: size = " << size << ", o_idx = " << o_idx << ", bytes_to_transfer = " << bytes_to_transfer << "\n";

		// Temporary workaround due to Xilinx memcpy issue - fixed in HLS 2017.4 */
		//memcpy(dout_gmem + o_idx, &buffer_out, sizeof(buffer_out));
		(dout_gmem + o_idx)[0] = buffer_out;

		size -= bytes_to_transfer;
		o_idx++;
		cnt_trans++;
	}

	act_reg->Control.Retc = SNAP_RETC_SUCCESS;

	act_reg->Data.axitrans_out = cnt_trans;

	/* Update MMIO status register */
	act_reg->Data.status |= ACC_DATA_RETURNED;

	//return 0;
}

//--- TOP LEVEL MODULE -------------------------------------------------
void hls_action(snap_membus_t *din_gmem,
		snap_membus_t *dout_gmem,
		/* snap_membus_t *d_ddrmem, */ /* if unused => export SDRAM_USED=FALSE */
		action_reg *act_reg,
		action_RO_config_reg *Action_Config)
{
#pragma HLS INLINE off
	// Host Memory AXI Interface
const int depth_in = CEILING_POS((float)(ACC_CALLS_PER_ACTION * 2 * MAX_NUMBER_COLUMNS_TEST_SET*HIGHT_IN_PIX*sizeof(float))/sizeof(snap_membus_t));
#pragma HLS INTERFACE m_axi port=din_gmem bundle=host_mem offset=slave depth=depth_in \
  max_read_burst_length=64 max_write_burst_length=64
#pragma HLS INTERFACE s_axilite port=din_gmem bundle=ctrl_reg offset=0x030

const int depth_out = CEILING_POS((float)(ACC_CALLS_PER_ACTION * MAX_PREDICTED_STRING_LENGTH*sizeof(uint32_t))/sizeof(snap_membus_t));
#pragma HLS INTERFACE m_axi port=dout_gmem bundle=host_mem offset=slave depth=depth_out \
  max_read_burst_length=64  max_write_burst_length=64
#pragma HLS INTERFACE s_axilite port=dout_gmem bundle=ctrl_reg offset=0x040

	std::cout << "DEBUG: AXI master interface with width = " << sizeof(snap_membus_t) << " bytes, depth_in = " << depth_in << ", depth_out = " << depth_out << "\n";
/*	// DDR memory Interface
 * #pragma HLS INTERFACE m_axi port=d_ddrmem bundle=card_mem0 offset=slave depth=512 \
 *   max_read_burst_length=64  max_write_burst_length=64
 * #pragma HLS INTERFACE s_axilite port=d_ddrmem bundle=ctrl_reg offset=0x050
 */
	// Host Memory AXI Lite Master Interface
#pragma HLS DATA_PACK variable=Action_Config
#pragma HLS INTERFACE s_axilite port=Action_Config bundle=ctrl_reg offset=0x010
#pragma HLS DATA_PACK variable=act_reg
#pragma HLS INTERFACE s_axilite port=act_reg bundle=ctrl_reg offset=0x100
#pragma HLS INTERFACE s_axilite port=return bundle=ctrl_reg

#ifndef VHLS_TB
	/* Required Action Type Detection */
	// 	NOTE: switch generates better vhdl than "if" */
	// Test used to exit the action if no parameter has been set.
 	// Used for the discovery phase of the cards */
	switch (act_reg->Control.flags) {
	case 0:
		Action_Config->action_type = BLSTM_ACTION_TYPE;
		Action_Config->release_level = RELEASE_LEVEL;
		act_reg->Control.Retc = 0xe00f;
		act_reg->Data.status = ACTION_DISCOVERED;
		return;
		break;
	default:
#else
		/* Update MMIO status register */
		act_reg->Data.status = ACTION_CALL;
#endif
		/* Update MMIO status register */
		act_reg->Data.status |= PROCESS_INIT;

        process_action(din_gmem, dout_gmem, act_reg);
        /* process_action(din_gmem, dout_gmem, d_ddrmem, act_reg); */

    	/* Update MMIO status register */
    	act_reg->Data.status |= PROCESS_EXECUTED;

#ifndef VHLS_TB
		break;
	}
#endif
}
