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
 * @file common_def.h
 * @author Dionysios Diamantopoulos, did@zurich.ibm.com, FPGA and PULP porting
 * @author Vladimir Rybalkin, rybalkin@eit.uni-kl.de, BLSTM code
 * @date 20 Sep 2017
 * @brief The header file containing configuration options for both HW and SW actions.
 * It contains options in the form of macros that are used both for HLS (HW) and compilation (SW).
 */


#ifndef __COMMON_DEF_H__
#define __COMMON_DEF_H__


/* BLSTM specific parameters */
#define NUMBER_OF_INPUTS 126
#define NUMBER_OF_NEURONS 100
#define HIGHT_IN_PIX 25
#define NUMBER_OF_CLASSES 110
#define MAX_NUMBER_COLUMNS_TEST_SET 732
#define BYPASS_COLUMNS 0

/*!
 * \def MAX_NUMBER_IMAGES_TEST_SET
 * The maximum number of input images. If the input folder provided has more
 *  images than this number, then they will be ignored.
 * */
#define MAX_NUMBER_IMAGES_TEST_SET 1 //3402 dataset size

/*!
 * \def ACC_CALLS_PER_ACTION
 * The number of accelerator calls on a single action execution.
 * This number defines how many data streams shall be input to IP from host memory (and results back)
 * It should be less or equal to MAX_NUMBER_IMAGES_TEST_SET. Valid for simulation and synthesis.
 * */
#define ACC_CALLS_PER_ACTION 1

/*!
 * \def HW_THREADS_PER_ACTION
 * The number of physical accelerator threads per action.
 * It differentiates from ACC_CALLS_PER_ACTION, as this value defines the number of physical accelerator
 * instantiations, regardless the input size, i.e. if HW_THREADS_PER_ACTION < ACC_CALLS_PER_ACTION, then
 * some of the physical accelerators shall be executed more than once (for serving extra load), while, when
 * HW_THREADS_PER_ACTION == ACC_CALLS_PER_ACTION, then all physical accelerators shall be executed exactly once.
 * It should be less or equal to ACC_CALLS_PER_ACTION. Valid only for synthesis.
 * */
#define HW_THREADS_PER_ACTION 1

/*!
 * \def COLS_PER_KERNEL_EXEC
 * The number of columns fed per single kernel execution.
 * In order to keep memory resources low, we can split the input columns and feed
 * them to the accelerator in chunks of x-columns. However, this affects the
 * accuracy, since the cut may happen at critical information. The more cutting
 * points, i.e. lower COLS_PER_KERNEL_EXEC, the higher probability of destroying
 * contemporal information, leading to less accuracy. When equals to
 * MAX_NUMBER_COLUMNS_TEST_SET, no splitting is forced.
 * */
#define COLS_PER_KERNEL_EXEC MAX_NUMBER_COLUMNS_TEST_SET

/*!
 * \def SHARED_MEM
 * Choose a single shared memory for various components.
 * Does not allow dataflow processing model
 * (even with true dual-port declaration) and impacts timing closure.
 * */
#define SHARED_MEM 0

/*!
 * \def FORMAT_IS_FLOAT
 * Choose either IEEE 32-bit floating point number or custom fixed-point format.
 * Valid for simulation and synthesis.
 * */
//#define FORMAT_IS_FLOAT

/*!
 * \def UNIQUE_WEIGHT_ROMS
  * */
//#define UNIQUE_WEIGHT_ROMS



#ifdef __cplusplus /* Fixed-point only for HLS code, not for x86/POWER host code */


#ifdef FORMAT_IS_FLOAT


typedef float DTYPE_WEIGHTS;
#define DTYPE_WEIGHTS_WGI_fw float
#define DTYPE_WEIGHTS_WGF_fw float
#define DTYPE_WEIGHTS_WGO_fw float
#define DTYPE_WEIGHTS_WCI_fw float
#define DTYPE_WEIGHTS_WIP_fw float
#define DTYPE_WEIGHTS_WFP_fw float
#define DTYPE_WEIGHTS_WOP_fw float

#define DTYPE_WEIGHTS_WGI_bw float
#define DTYPE_WEIGHTS_WGF_bw float
#define DTYPE_WEIGHTS_WGO_bw float
#define DTYPE_WEIGHTS_WCI_bw float
#define DTYPE_WEIGHTS_WIP_bw float
#define DTYPE_WEIGHTS_WFP_bw float
#define DTYPE_WEIGHTS_WOP_bw float
#define DTYPE_WEIGHTS_W2 float



typedef float DTYPE;
typedef float DTYPE_IN;
typedef float DTYPE_TRNLB;
typedef float DTYPE_IMG;
#if SHARED_MEM == 1
#define DTYPE_LAYERS float
#else
// The width of the streams among layers: determining internal bus widths for connecting layers
typedef float DTYPE_LAYERS;
#endif /* SHARED_MEM == 1 */
typedef float DTYPE_OUTPUT;

#else /* FORMAT_IS_FLOAT */

#include "ap_int.h"

// The width of the weights stored in ROMs
//typedef ap_fixed<8,4> DTYPE_WEIGHTS; // DSE: ap_fixed<8,4>

#define DTYPE_WEIGHTS ap_fixed<8,4>

//typedef ap_fixed<8,4> DTYPE;
#define DTYPE ap_fixed<8,4>

//typedef ap_fixed<17,7> DTYPE_IN; // DSE: ap_fixed<16,7>
#define DTYPE_IN ap_fixed<17,7>

//typedef ap_fixed<10,2> DTYPE_TRNLB; // DSE: ap_fixed<8,2>
#define DTYPE_TRNLB ap_fixed<10,2>

// The width of the input images: determining required bandwidth
//typedef ap_fixed<8,4> DTYPE_IMG; // DSE: ap_fixed<8,4>
#define DTYPE_IMG ap_fixed<8,4>

// Since DTYPE_LAYERS->ap_fixed<8,2> and DTYPE_TRNLB->ap_fixed<10,2>,
// we choose the 'higher' format for their shared memory, i.e. DTYPE_TRNLB.
#if SHARED_MEM == 1
#define DTYPE_LAYERS DTYPE_TRNLB  // DSE: ap_fixed<7,4>
#else
// The width of the streams among layers: determining internal bus widths for connecting layers
//typedef ap_fixed<8,2> DTYPE_LAYERS;  // DSE: ap_fixed<8,4>
#define DTYPE_LAYERS ap_fixed<8,4>
//typedef float DTYPE_LAYERS;
#endif

// The width of the output layer
//typedef ap_fixed<24,19> : 97.6336%% // DSE ap_fixed<9,6>:96.2442%, ap_fixed<16,12>:97.231%, ap_fixed<17,13>:97.5215, ap_fixed<18,14>: 97.5939%, ap_fixed<19,15>:97.6135%
#define DTYPE_OUTPUT ap_fixed<19,15>
#endif /* __cplusplus */

#endif /* FORMAT_IS_FLOAT */


#define PROFILE 0

/*!
 * \def INTERFACE_IS_STREAM
 * Choose the interface of kernel: VHLS stream (FIFO) or array (RAM)
 */
#define INTERFACE_IS_STREAM
//#define INTERFACE_IS_ARRAY

/*!
 * \def MANY_STREAMS_FOR_MANY_ACCS
 */
#define MANY_STREAMS_FOR_MANY_ACCS

/*!
 * \def EMULATING_IO_SINGLE_KERNEL_BLSTM
 * Instead of real instantiation of BLSTM kernel, just perform I/O emulation.
 * Use only for simulation/debugging with SNAP, not production sw/hw.
 *  */
//#define EMULATING_IO_SINGLE_KERNEL_BLSTM

/*!
 * \def DATA_MOVE_OVER_AXI
 * Choose either to feed kernel from an AXI interface (compatible with SNAP) or direct VHLS stream.
 * First (uncomment) applies for real production code, later (comment) for debugging.
 *  */
#define DATA_MOVE_OVER_AXI

/*!
 * \def TRIGF_APPROX
 * Options for approximating activation functions
 * 0 : No approximation, use of math.h functions
 * 1 : Use of low-latency math approximation functions
 * 2 : Approximation with look-up tables
 * 3 : Approximation with Piecewise Linear Functions (FIXME: in development)
 */
#define TRIGF_APPROX 2

/*!
 * \def MAX_PREDICTED_STRING_LENGTH
 * Choose the length of the predicted string
 * Normally, it should cover the worst-case which is MAX_NUMBER_COLUMNS_TEST_SET,
 * i.e. if a column generates a character (practically never). For memory space,
 * after profiling, we can adapt it to 83. For different dataset it may require
 * tuning.
 * */
#define MAX_PREDICTED_STRING_LENGTH 83

/*!
 * \def DIMENSION_OF_WEIGHTS
 * The dimension of weighting factors, i.e.
 * 1-> 1d[NUMBER_OF_NEURONS*NUMBER_OF_INPUTS]
 * 2-> 2d[NUMBER_OF_NEURONS][NUMBER_OF_INPUTS] -> Unsupported in Community version
 * */
#define DIMENSION_OF_WEIGHTS 1

/*!
 * \def UNIFRORM_DTYPE_WEIGHTS
 * Option to have the same datatype for all weights (1) or customized (0).
 * */
#define UNIFRORM_DTYPE_WEIGHTS 1

/*!
 * \def IMPLEMENT_FIFO_WITH_SINGLE_LUT
 * The depth size of hls::stream channels in dataflow execution.
 * The option of depth=1 (IMPLEMENT_FIFO_WITH_SINGLE_LUT=1) enables
 * the HLS compiler to built the channel using only one LUT instead
 * of BRAMs. In co-sim it probably has to be '0' so that FIFOs
 * keep enough space.
 * */
#define IMPLEMENT_FIFO_WITH_SINGLE_LUT 1

/*!
 * \def OUTPUT_LAYER_UNROLL_FACTOR
 * The unroll factor of cell loops in output layer
 * */
#define OUTPUT_LAYER_UNROLL_FACTOR 5 // 10

/*!
 * \def INPUT_LAYER_UNROLL_FACTOR
 * The unroll factor of cell loops in input layer
 * */
#define INPUT_LAYER_UNROLL_FACTOR 9 // 9


#define FRACT_BITS 4
#define FLOAT2FIXED(x) ((int)((x) * (1 << FRACT_BITS)))
#define FIXED2FLOAT(x) (((float)(x)) / (1 << FRACT_BITS

/*!
 * \def IMG_FLOAT_TO_FIXED_CASTING_IN_CPU
 * Choose CPU to make the casting of input values of images from float to
 * fixed-point. The option of '1' forces the use of Xilinx ap_int.h library to
 * do the casting in CPU. It results in higher latency comparing to do the
 * casting in FPGA. However it is a workaround with Vivado_HLS 2017.4 issue that
 * does not generate functional RTL on float-to-fixed casting.
 * */
#define IMG_FLOAT_TO_FIXED_CASTING_IN_CPU 1

/*!
 * \def VHLS_TB
 * co-sim hangs on discovery phase of action, thus we bypass it using this macro.
 * Caution: Only uncomment for VHLS co-sim, always comment for production code
 * */
//#define VHLS_TB

/*!
 * \def BLSTM_TB_FAILURE
 * An error code for tb-only
 * */
#define BLSTM_TB_FAILURE 0x0F

/* Printing level verbosity */
enum log_level_t {
  LOG_NOTHING,
  LOG_CRITICAL,
  LOG_ERROR,
  LOG_WARNING,
  LOG_INFO,
  LOG_DEBUG
};

/*!
 * \def DEBUG_LEVEL
 * The level of debugging information on stdout/stderr.
 * */
#define DEBUG_LEVEL LOG_DEBUG


#if IMPLEMENT_FIFO_WITH_SINGLE_LUT == 1
#define STREAM_ACTION_SIZE_IN 1
#define STREAM_KERNEL_SIZE_IN 1
#define STREAM_KERNEL_SIZE_OUT 1
#else
#define STREAM_ACTION_SIZE_IN MAX_NUMBER_COLUMNS_TEST_SET * HIGHT_IN_PIX
#define STREAM_KERNEL_SIZE_IN MAX_NUMBER_COLUMNS_TEST_SET * NUMBER_OF_NEURONS
#define STREAM_KERNEL_SIZE_OUT MAX_PREDICTED_STRING_LENGTH * NUMBER_OF_CLASSES
#endif


#ifndef FORMAT_IS_FLOAT
#if UNIFRORM_DTYPE_WEIGHTS == 1
//DEBUG: WGI_fw=1.560830, WGF_fw=2.413700, WGO_fw=-1.716290, WCI_fw=7.379360, WIP_fw=1.961290, WFP_fw=10.949700, WOP_fw=2.414710
#define DTYPE_WEIGHTS_WGI_fw DTYPE_WEIGHTS
#define DTYPE_WEIGHTS_WGF_fw DTYPE_WEIGHTS
#define DTYPE_WEIGHTS_WGO_fw DTYPE_WEIGHTS
#define DTYPE_WEIGHTS_WCI_fw DTYPE_WEIGHTS
#define DTYPE_WEIGHTS_WIP_fw DTYPE_WEIGHTS
#define DTYPE_WEIGHTS_WFP_fw DTYPE_WEIGHTS
#define DTYPE_WEIGHTS_WOP_fw DTYPE_WEIGHTS

//DEBUG: WGI_bw=-0.915375, WGF_bw=1.121140, WGO_bw=0.872268, WCI_bw=1.525470, WIP_bw=-0.431215, WFP_bw=-0.713736, WOP_bw=1.336620
#define DTYPE_WEIGHTS_WGI_bw DTYPE_WEIGHTS
#define DTYPE_WEIGHTS_WGF_bw DTYPE_WEIGHTS
#define DTYPE_WEIGHTS_WGO_bw DTYPE_WEIGHTS
#define DTYPE_WEIGHTS_WCI_bw DTYPE_WEIGHTS
#define DTYPE_WEIGHTS_WIP_bw DTYPE_WEIGHTS
#define DTYPE_WEIGHTS_WFP_bw DTYPE_WEIGHTS
#define DTYPE_WEIGHTS_WOP_bw DTYPE_WEIGHTS

//DEBUG: WGI_bw=-5.218670
#define DTYPE_WEIGHTS_W2 DTYPE_WEIGHTS

#else // UNIFRORM_DTYPE_WEIGHTS

//DEBUG: WGI_fw=1.560830, WGF_fw=2.413700, WGO_fw=-1.716290, WCI_fw=7.379360, WIP_fw=1.961290, WFP_fw=10.949700, WOP_fw=2.414710
#define DTYPE_WEIGHTS_WGI_fw ap_fixed<5,1>
#define DTYPE_WEIGHTS_WGF_fw ap_fixed<6,2>
#define DTYPE_WEIGHTS_WGO_fw ap_fixed<5,1>
#define DTYPE_WEIGHTS_WCI_fw ap_fixed<8,4>
#define DTYPE_WEIGHTS_WIP_fw ap_fixed<6,2>
#define DTYPE_WEIGHTS_WFP_fw ap_fixed<8,4>
#define DTYPE_WEIGHTS_WOP_fw ap_fixed<5,2>

//DEBUG: WGI_bw=-0.915375, WGF_bw=1.121140, WGO_bw=0.872268, WCI_bw=1.525470, WIP_bw=-0.431215, WFP_bw=-0.713736, WOP_bw=1.336620
#define DTYPE_WEIGHTS_WGI_bw ap_fixed<5,1>
#define DTYPE_WEIGHTS_WGF_bw ap_fixed<6,2>
#define DTYPE_WEIGHTS_WGO_bw ap_fixed<5,1>
#define DTYPE_WEIGHTS_WCI_bw ap_fixed<6,2>
#define DTYPE_WEIGHTS_WIP_bw ap_fixed<5,1>
#define DTYPE_WEIGHTS_WFP_bw ap_fixed<5,1>
#define DTYPE_WEIGHTS_WOP_bw ap_fixed<6,2>

//DEBUG: WGI_bw=-5.218670
#define DTYPE_WEIGHTS_W2 ap_fixed<7,4>

#endif // UNIFRORM_DTYPE_WEIGHTS
#endif // FORMAT_IS_FLOAT

#endif	/* __COMMON_DEF_H__ */
