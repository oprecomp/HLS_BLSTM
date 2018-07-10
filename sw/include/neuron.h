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
 * @file neuron.h
 * @author Dionysios Diamantopoulos, did@zurich.ibm.com, FPGA and PULP porting
 * @author Vladimir Rybalkin, rybalkin@eit.uni-kl.de, BLSTM code
 * @date 22 Dec 2017
 * @brief Header file for the main BLSTM code, adapted to run on POWER8 over a CAPI/SNAP WED.
 * The BLSTM sw_action uses the same I/O interface as the hw_action so that the
 * user can easily execute either flavors.
 * */

#ifndef NEURON_H
#define NEURON_H

#include <math.h>       // tanh, log
#include <sys/types.h>

#include "../../include/common_def.h"

#ifndef FLT_MAX
#define FLT_MAX 3.402823466e+38F /* max value */
#endif
#ifndef FLT_MIN
#define FLT_MIN 1.175494351e-38F /* min positive value */
#endif



	//====================================================================================================================================================================================================================
	// LSTM
	//====================================================================================================================================================================================================================

	// The dot product corresponding to a single gate of the LSTM memory cell
	float DotVectorToVector126(float *source, 	// IN  // size: 1.0 + HIGHT_IN_PIX + NUMBER_OF_NEURONS = NUMBER_OF_INPUTS
									  float *weights);	// IN  // size: NUMBER_OF_INPUTS

	void DotVectorToVector126_four(float source[NUMBER_OF_INPUTS],
										float weights0[NUMBER_OF_INPUTS],
										float weights1[NUMBER_OF_INPUTS],
										float weights2[NUMBER_OF_INPUTS],
										float weights3[NUMBER_OF_INPUTS],
										float outputs[4]);

	// The function of a single LSTM memory cell
	void HiddenLayerSingleMemoryCell(float *source,					// IN  // size: 1.0 + HIGHT_IN_PIX + NUMBER_OF_NEURONS = NUMBER_OF_INPUTS
									 unsigned int currentColumn,	// IN  // The current column of the image
									 float in_state,				// IN  // A single input state
									 float *WGI,   					// IN  // size: NUMBER_OF_INPUTS
									 float *WGF,   					// IN  // size: NUMBER_OF_INPUTS
									 float *WGO,   					// IN  // size: NUMBER_OF_INPUTS
									 float *WCI,   					// IN  // size: NUMBER_OF_INPUTS
									 float WIP,						// IN  // A single peephole weight
									 float WFP,						// IN  // A single peephole weight
									 float WOP,						// IN  // A single peephole weight
									 float *out_state,				// OUT // A single output state
									 float *output);              	// OUT // A single output


	void Hidden_Layer(float *image,					// IN  // size: numberOfColumns * HIGHT_IN_PIX
					  unsigned int numberOfColumns,	// IN  //
					  float *WGI, 					// IN  // size: NUMBER_OF_NEURONS * NUMBER_OF_INPUTS
					  float *WGF, 					// IN  // size: NUMBER_OF_NEURONS * NUMBER_OF_INPUTS
					  float *WGO, 					// IN  // size: NUMBER_OF_NEURONS * NUMBER_OF_INPUTS
					  float *WCI, 					// IN  // size: NUMBER_OF_NEURONS * NUMBER_OF_INPUTS
					  float *WIP, 					// IN  // size: NUMBER_OF_NEURONS
					  float *WFP, 					// IN  // size: NUMBER_OF_NEURONS
					  float *WOP,					// IN  // size: NUMBER_OF_NEURONS
					  float *result);				// OUT // size: numberOfColumns * NUMBER_OF_NEURONS


	float divexpf_lookup(float x);
	float tanh_lookup(float x);
	float expf_lookup(float x);

	//====================================================================================================================================================================================================================
	// Connectionist Temporal Classification Layer (CTC layer)
	//====================================================================================================================================================================================================================

	// The dot product corresponding to a single neuron of the output layer operating on an concatinated output from the forward and the bakward hidden layers
	 float DotVectorToVector201(float *W2,		// IN  // size: NUMBER_OF_NEURONS * 2 + 1
									  float *input_fw, 	// IN  // size: NUMBER_OF_NEURONS
							          float *input_bw);	// IN  // size: NUMBER_OF_NEURONS

	// The function of a single neuron of the output layer
	void OutputLayerSinlgleNeuron(float *W2, 		// IN  // size: NUMBER_OF_NEURONS * 2 + 1
										 float *input_fw,	// IN  // size: NUMBER_OF_NEURONS
										 float *input_bw, 	// IN  // size: NUMBER_OF_NEURONS
										 float *output);	// OUT //

	void Output_Layer(unsigned int numberOfColumns, // IN  //
					  float *W2, 					// IN  // size: NUMBER_OF_CLASSES * (NUMBER_OF_NEURONS * 2 + 1)
					  float *input_fw,			 	// IN  // size: numberOfColumns * NUMBER_OF_NEURONS
					  float *input_bw,			 	// IN  // size: numberOfColumns * NUMBER_OF_NEURONS
					  float *output); 				// OUT // size: numberOfColumns * NUMBER_OF_CLASSE

	unsigned int max_element_syn_test(float *image, unsigned int __first, unsigned int __last);

	// Reconstruct a line from the labels
	void TranslateBack(unsigned int numberOfColumns, float *input, unsigned int *output, unsigned int* str_len, float threshold);

	// The main function for a single image
	void Single_Kernel_BLSTM(
			float *image_fw,
			float *image_bw,
			unsigned int numberOfColumns,
			unsigned int *vecPredictedStringInd,
			unsigned int *str_len);

	//====================================================================================================================================================================================================================
	// AUXILIARY
	//====================================================================================================================================================================================================================


	void print_usage_and_exit(const char *argv0);


#endif
