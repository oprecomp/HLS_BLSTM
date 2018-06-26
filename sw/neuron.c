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
 * @file neuron.c
 * @author Dionysios Diamantopoulos, did@zurich.ibm.com, FPGA and PULP porting
 * @author Vladimir Rybalkin, rybalkin@eit.uni-kl.de, BLSTM code
 * @date 22 Dec 2017
 * @brief The main BLSTM code, adapted to run on POWER8 over a CAPI/SNAP WED.
 * The BLSTM sw_action uses the same I/O interface as the hw_action so that the
 * user can easily execute either flavors.
 * */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#include "./include/neuron.h"
#include "./include/model.h"
#include "../hw/generate_luts.h"
#include "./include/lut.h"
#include "../hw/include/tiny_math.h"



	//====================================================================================================================================================================================================================
	// LSTM
	//====================================================================================================================================================================================================================

	// The dot product corresponding to a single gate of the LSTM memory cell
	inline float DotVectorToVector126(float *source, 	// IN  // size: 1.0 + HIGHT_IN_PIX + NUMBER_OF_NEURONS = NUMBER_OF_INPUTS
									  float *weights)	// IN  // size: NUMBER_OF_INPUTS
	{
		float output = 0.0;
		float tmp;

		for(unsigned int i = 0; i < NUMBER_OF_INPUTS; i++)
		{
			tmp = source[i] * weights[i];
			output += tmp;
		}
		return output;
	}



	// The dot product corresponding to a four gates of the LSTM memory cell
	void DotVectorToVector126_four(float source[NUMBER_OF_INPUTS],
									  float weights0[NUMBER_OF_INPUTS],
									  float weights1[NUMBER_OF_INPUTS],
									  float weights2[NUMBER_OF_INPUTS],
									  float weights3[NUMBER_OF_INPUTS],
									  float outputs[4])	// IN  // size: NUMBER_OF_INPUTS
	{
		outputs[0] = outputs[1] = outputs[2] = outputs[3] = 0.0;
		float tmp0, tmp1, tmp2, tmp3;
		for(unsigned int i = 0; i < NUMBER_OF_INPUTS; i++)
		{

		float src_fw = source[i];
		tmp0 = src_fw * weights0[i];
		tmp1 = src_fw * weights1[i];
		tmp2 = src_fw * weights2[i];
		tmp3 = src_fw * weights3[i];
		outputs[0] += tmp0;
		outputs[1] += tmp1;
		outputs[2] += tmp2;
		outputs[3] += tmp3;
		}
	}



	float divexpf_lookup(float x) {
		const float step = ((float)fabs(MAX_TARGET_DIVEXPF)+(float)fabs(MIN_TARGET_DIVEXPF)) / (LUT_SIZE_DIVEXPF-1);
		if (x <= MIN_TARGET_DIVEXPF)
			return 0;
		else if (x >= MAX_TARGET_DIVEXPF)
			return 1;
		else {
			//Since the divisor is a constant, then the problem devolves into a multiplication by a constant (the reciprocal of the divisor).
			int index = (int)(((x-(MIN_TARGET_DIVEXPF))/step));
			//printf("DEBUG: divexpf_lut[%d]=%f, step=%f\n", index, divexpf_lut[index], step);
			return divexpf_lut[index];
		}
	}

		float tanh_lookup(float x) {
			const float step = ((float)fabs(MAX_TARGET_TANH)+(float)fabs(MIN_TARGET_TANH)) / (LUT_SIZE_TANH-1);
			if (x <= MIN_TARGET_TANH)
				return -1;
			else if (x >= MAX_TARGET_TANH)
				return 1;
			else {
				//Since the divisor is a constant, then the problem devolves into a multiplication by a constant (the reciprocal of the divisor).
				int index = (int)(((x-(MIN_TARGET_TANH))/step));
				//printf("DEBUG: tanh_lut[%d]=%f, step=%f\n", index, tanh_lut[index], step);
				return tanh_lut[index];
			}
		}


		float expf_lookup(float x) {
			const float step = ((float)fabs(MAX_TARGET_EXPF)+(float)fabs(MIN_TARGET_EXPF)) / (LUT_SIZE_EXPF-1);
			if (x <= MIN_TARGET_EXPF)
				return 0;
			else if (x >= MAX_TARGET_EXPF)
				return expf_lut[LUT_SIZE_EXPF-1];
			else {
				//Since the divisor is a constant, then the problem devolves into a multiplication by a constant (the reciprocal of the divisor).
				int index = (int)(((x-(MIN_TARGET_EXPF))/step));
				//printf("DEBUG: expf_lut[%d]=%f, step=%f\n", index, expf_lut[index], step);
				return expf_lut[index];
			}
		}


	// The function of a single LSTM memory cell
	void HiddenLayerSingleMemoryCell(float *source,	// IN  // size: 1.0 + HIGHT_IN_PIX + NUMBER_OF_NEURONS = NUMBER_OF_INPUTS
									 unsigned int currentColumn,		// IN  // The current column of the image
									 float in_state,				// IN  // A single input state
									 float *WGI,   					// IN  // size: NUMBER_OF_INPUTS
									 float *WGF,   					// IN  // size: NUMBER_OF_INPUTS
									 float *WGO,   					// IN  // size: NUMBER_OF_INPUTS
									 float *WCI,   					// IN  // size: NUMBER_OF_INPUTS
									 float WIP,							// IN  // A single peephole weight
									 float WFP,							// IN  // A single peephole weight
									 float WOP,							// IN  // A single peephole weight
									 float *out_state,			// OUT // A single output state
									 float *output)         // OUT // A single output

	{
		float gix, gfx, gox, cix;
		float gi, gf, go, ci;
		float tmp_in_state, tmp_out_state;
		float outputs[4];
		#if TRIGF_APPROX == 1
		float divider;
		#endif

		DotVectorToVector126_four(source, WGI, WGF, WGO, WCI, outputs);

		tmp_in_state = in_state;

		gix = outputs[0];
		gfx = outputs[1];
		gox = outputs[2];
		cix = outputs[3];

		if(currentColumn > 0)
		{
			gix = gix + WIP * tmp_in_state;
			gfx = gfx + WFP * tmp_in_state;
		}

		#if TRIGF_APPROX == 0
		gi = 1.0/(1.0 + expf(-(float)gix));
		gf = 1.0/(1.0 + expf(-(float)gfx));
		ci = (float)tanhf((float)cix);
		#elif TRIGF_APPROX == 1
		divider = 1.0 + tiny_expf(-gix);
		gi = 1.0/divider;
		divider = 1.0 + tiny_expf(-gfx);
		gf = 1.0/divider;
		ci = tiny_tanhf(cix);
		#elif TRIGF_APPROX == 2
		gi = divexpf_lookup(gix);
		gf = divexpf_lookup(gfx);
		ci = tanh_lookup(cix);
		#endif

		tmp_out_state = ci * gi;

		if(currentColumn > 0)
			{
				tmp_out_state = tmp_out_state + gf * tmp_in_state;
				gox = gox + WOP * tmp_out_state;
			}
		#if TRIGF_APPROX == 0
			go = (float)(1.0/(1.0 + expf(-(float)gox)));
			*output = (float)tanhf((float)tmp_out_state) * go;
		#elif TRIGF_APPROX == 1
			divider = 1.0 + tiny_expf(-gox);
			go = 1.0/divider;
			*output = tiny_tanhf(tmp_out_state) * go;
		#elif TRIGF_APPROX == 2
			go = divexpf_lookup(gox);
			*output = tanh_lookup(tmp_out_state) * go;
		#endif

		*out_state = tmp_out_state;
	}

	void Hidden_Layer(float *image,					// IN  // size: numberOfColumns * HIGHT_IN_PIX
					  unsigned int numberOfColumns,	// IN  //
					  float *WGI, 					// IN  // size: NUMBER_OF_NEURONS * NUMBER_OF_INPUTS
					  float *WGF, 					// IN  // size: NUMBER_OF_NEURONS * NUMBER_OF_INPUTS
					  float *WGO, 					// IN  // size: NUMBER_OF_NEURONS * NUMBER_OF_INPUTS
					  float *WCI, 					// IN  // size: NUMBER_OF_NEURONS * NUMBER_OF_INPUTS
					  float *WIP, 					// IN  // size: NUMBER_OF_NEURONS
					  float *WFP, 					// IN  // size: NUMBER_OF_NEURONS
					  float *WOP,					// IN  // size: NUMBER_OF_NEURONS
					  float *result)				// OUT // size: numberOfColumns * NUMBER_OF_NEURONS

	{

		float source[NUMBER_OF_INPUTS];
		float outputRegister[NUMBER_OF_NEURONS];
		float stateRegister[NUMBER_OF_NEURONS];

		float out_state, output;

		float *pWGI, *pWGF, *pWGO, *pWCI;

		for(unsigned int i = 0; i < NUMBER_OF_NEURONS; i++)
			outputRegister[i] = 0.0;

		for(unsigned int column = 0; column < numberOfColumns; column++)
		{
			//Concatinate 1.0 + image + previous output
			source[0] = 1.0;

			for(unsigned int k = 0; k < HIGHT_IN_PIX; k++) {
				source[k+1] = image[column*HIGHT_IN_PIX+k];

			}
			for(unsigned int k = 0; k < NUMBER_OF_NEURONS; k++) {
				source[k+1+HIGHT_IN_PIX] = outputRegister[k];
			}



			for(unsigned int n = 0; n < NUMBER_OF_NEURONS; n++)
			{

				pWGI = WGI + n * NUMBER_OF_INPUTS;
				pWGF = WGF + n * NUMBER_OF_INPUTS;
				pWGO = WGO + n * NUMBER_OF_INPUTS;
				pWCI = WCI + n * NUMBER_OF_INPUTS;

				HiddenLayerSingleMemoryCell(source,
										    column,
										    stateRegister[n],
										    pWGI,
										    pWGF,
										    pWGO,
										    pWCI,
										    WIP[n],
										    WFP[n],
										    WOP[n],
										    &out_state,
										    &output);


				stateRegister[n] = out_state;
				outputRegister[n] = output;
			}

			for(unsigned int i = 0; i < NUMBER_OF_NEURONS; i++)
				result[column * NUMBER_OF_NEURONS + i] = outputRegister[i];
		}
	}




	//====================================================================================================================================================================================================================
	// Connectionist Temporal Classification Layer (CTC layer)
	//====================================================================================================================================================================================================================

	// The dot product corresponding to a single neuron of the output layer operating on an concatinated output from the forward and the bakward hidden layers
	inline float DotVectorToVector201(float *W2,				// IN  // size: NUMBER_OF_NEURONS * 2 + 1
									  								float *input_fw, 	// IN  // size: NUMBER_OF_NEURONS
							        							float *input_bw)	// IN  // size: NUMBER_OF_NEURONS
	{
		float output = 0.0;
		float tmp;

		output = 1.0 * W2[0];

		for(unsigned int i = 0; i < NUMBER_OF_NEURONS; i++)
		{
			tmp = W2[1 + i] * input_fw[i];
			output += tmp;
		}

		for(unsigned int i = 0; i < NUMBER_OF_NEURONS; i++)
		{
			tmp = W2[1 + NUMBER_OF_NEURONS + i] * input_bw[i];
			output += tmp;
		}

		return output;
	}

	// The function of a single neuron of the output layer
	inline void OutputLayerSinlgleNeuron(	float *W2, 				// IN  // size: NUMBER_OF_NEURONS * 2 + 1
										 									 	float *input_fw,	// IN  // size: NUMBER_OF_NEURONS
										 								 		float *input_bw, 	// IN  // size: NUMBER_OF_NEURONS
										 										float *output)		// OUT //
	{
		*output = DotVectorToVector201(W2, input_fw, input_bw);
	}

	void Output_Layer(unsigned int numberOfColumns, // IN  //
					  				float *W2, 				// IN  // size: NUMBER_OF_CLASSES * (NUMBER_OF_NEURONS * 2 + 1)
					  				float *input_fw,	// IN  // size: numberOfColumns * NUMBER_OF_NEURONS
					  				float *input_bw,	// IN  // size: numberOfColumns * NUMBER_OF_NEURONS
					  				float *output)		// OUT // size: numberOfColumns * NUMBER_OF_CLASSES
	{

		float sum;

		// Compute the function of the output layer for each concatinated column
		for(unsigned int col = 0; col < numberOfColumns; col++)
		{
			sum = 0.0;

			float *pInput_fw  = input_fw  + col * NUMBER_OF_NEURONS;
			float *pInput_bw  = input_bw  + (numberOfColumns - col - 1) * NUMBER_OF_NEURONS;
			float *pOutput = output + col * NUMBER_OF_CLASSES;

			// Compute the function of each neuron of the output layer
			for(unsigned int cl = 0; cl < NUMBER_OF_CLASSES; cl++)
			{
				float *pW2 = W2 + cl * (NUMBER_OF_NEURONS * 2 + 1);

				OutputLayerSinlgleNeuron(pW2, pInput_fw, pInput_bw, pOutput+cl);
			}

			/*			union {
								uint32_t t;
								float f;
								char b[sizeof(float)];
						} u;
			*/

			// Softmax function
			for(unsigned int cl = 0; cl < NUMBER_OF_CLASSES; cl++)
			{
				#if TRIGF_APPROX == 0
					pOutput[cl] = (float)expf((float)pOutput[cl]);
				#elif TRIGF_APPROX == 1
					pOutput[cl] = tiny_expf(pOutput[cl]);
					//u.f = pOutput[cl];
					//printf("EXP(pOutput[%lu])        =%08lx\n", cl, (unsigned int)u.t);
				#elif TRIGF_APPROX == 2
					pOutput[cl] = expf_lookup(pOutput[cl]);
					//u.f = pOutput[cl];
					//printf("expf_lookup(pOutput[%lu])=%08lx\n", cl, (unsigned int)u.t);
				#endif
			}

			for(unsigned int cl = 0; cl < NUMBER_OF_CLASSES; cl++)
			{
				sum += *(pOutput+cl);
			}

			for(unsigned int cl = 0; cl < NUMBER_OF_CLASSES; cl++)
				*(pOutput+cl) /= sum;

		}
	}

	/**
	 *  @brief  Return the index of a maximum element in a 1D array.
	 *  @param  first  Start index of range.
	 *  @param  last   End index of range.
	 *  @return  Index referencing the first instance of the largest value.
	*/
		unsigned int max_element_syn_test(float *image, unsigned int __first, unsigned int __last)
		{
		if (__first == __last)
			return __first;

			unsigned int __result = __first;
			while (++__first != __last) {
			//i++;
				float check1 = image[__result];
				float check2 = image[__first];
				if (check1 < check2)
				__result = __first;
			}
			//std::cout << "i = " << i << std::endl;
			return __result;
		}

	// Reconstruct a line from the labels
	void TranslateBack( 				// IN  //
					   unsigned int numberOfColumns, 	// IN  //
					   float *input, 					// IN  // size: numberOfColumns * NUMBER_OF_CLASSES
					   unsigned int *output, 			// OUT //
						 unsigned int *str_len,
					   float threshold)					// IN  //
	{
		unsigned int left_limit = 0, right_limit;
		unsigned int offset, label;
		*str_len = 0;

		for(unsigned int col = 0; col < numberOfColumns-1; col++)
		{
			if (input[col * NUMBER_OF_CLASSES] > threshold && input[(col + 1) * NUMBER_OF_CLASSES] < threshold )
				left_limit = (col + 1) * NUMBER_OF_CLASSES;
			else if (input[col * NUMBER_OF_CLASSES] < threshold && input[(col + 1) * NUMBER_OF_CLASSES] > threshold )
			{
				right_limit = (col + 1) * NUMBER_OF_CLASSES;
				offset = max_element_syn_test(input, left_limit, right_limit);
				label = offset - (offset / NUMBER_OF_CLASSES) * NUMBER_OF_CLASSES;
				// Safe the overflow, since output is malloced up to MAX_PREDICTED_STRING_LENGTH-1
				if (*str_len < MAX_PREDICTED_STRING_LENGTH)
					output[*str_len] = label;
				//printf("DEBUG: *str_len=%u, label=%u, output[%u]=%u\n", *str_len, label, *str_len, output[*str_len]);
				*str_len = *str_len + 1;
			}
		}

	}



	void Single_Kernel_BLSTM(
			float *image_fw,
			float *image_bw,
			unsigned int numberOfColumns,
			unsigned int *vecPredictedStringInd,
			unsigned int *str_len)
	{

		float *pOutputFromtHiddenLayer_fw = (float *)malloc(numberOfColumns * NUMBER_OF_NEURONS * sizeof (float));
		assert (pOutputFromtHiddenLayer_fw != NULL);
		float *pOutputFromtHiddenLayer_bw = (float *)malloc(numberOfColumns * NUMBER_OF_NEURONS * sizeof (float));
		assert (pOutputFromtHiddenLayer_bw != NULL);
		float *poutputFromOutputLayer = (float *)malloc(COLS_PER_KERNEL_EXEC * NUMBER_OF_CLASSES * sizeof (float));
		assert (poutputFromOutputLayer != NULL);

		// Forward direction
		Hidden_Layer(image_fw,
				 numberOfColumns,
				 WGI_fw,
				 WGF_fw,
				 WGO_fw,
				 WCI_fw,
				 WIP_fw,
				 WFP_fw,
				 WOP_fw,
				 pOutputFromtHiddenLayer_fw);

		// Backward direction
		Hidden_Layer(image_bw,
				 numberOfColumns,
				 WGI_bw,
				 WGF_bw,
				 WGO_bw,
				 WCI_bw,
				 WIP_bw,
				 WFP_bw,
				 WOP_bw,
				 pOutputFromtHiddenLayer_bw);

		// CTC - Output Layer
		Output_Layer(numberOfColumns,
				 W2,
				 pOutputFromtHiddenLayer_fw,
				 pOutputFromtHiddenLayer_bw,
				 poutputFromOutputLayer);

		// Return the predicted string
		TranslateBack(numberOfColumns, poutputFromOutputLayer, vecPredictedStringInd, str_len, 0.7);

		free(pOutputFromtHiddenLayer_fw);
		free(pOutputFromtHiddenLayer_bw);
		free(poutputFromOutputLayer);
}
