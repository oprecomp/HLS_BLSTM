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
 * @file neuron.cpp
 * @author Dionysios Diamantopoulos, did@zurich.ibm.com, FPGA porting
 * @author Vladimir Rybalkin, rybalkin@eit.uni-kl.de, BLSTM code
 * @date 20 Feb 2017
 * @brief The main BLSTM code, adapted to be synthesized woth Xilinx Vivado HLS
 * tool. It is optimized for the Alpha-data KU3 FPGA device.
 * */

#include <cstddef>
#include <stdio.h>

// for checking provided directory exists
#include <sys/stat.h>

#include "neuron.hpp"

#include "model.h"
#if DIMENSION_OF_WEIGHTS == 2
#include "model_2D.h"
#endif

#include "./include/tiny_math.h"

FILE *fp;


#ifdef NO_SYNTH

	//====================================================================================================================================================================================================================
	// NEURAL NETWORK - MODEL
	//====================================================================================================================================================================================================================

	// Constructor
	NeuralNetwork::NeuralNetwork()
	{
		// Hidden layer weigths
		WGI = new DTYPE [NUMBER_OF_NEURONS * NUMBER_OF_INPUTS];// Input Gate
		WGF = new DTYPE [NUMBER_OF_NEURONS * NUMBER_OF_INPUTS];// Forget Gate
		WGO = new DTYPE [NUMBER_OF_NEURONS * NUMBER_OF_INPUTS];// Output Gate
		WCI = new DTYPE [NUMBER_OF_NEURONS * NUMBER_OF_INPUTS];// Memory Cell Input

		// Peepholes
		WIP = new DTYPE [NUMBER_OF_NEURONS];// Input Gate
		WFP = new DTYPE [NUMBER_OF_NEURONS];// Forget Gate
		WOP = new DTYPE [NUMBER_OF_NEURONS];// Output Gate

		// Output layer weigths
 		W2 = new DTYPE [NUMBER_OF_CLASSES * (1 + NUMBER_OF_NEURONS * 2)];
	}

	// Destructor
	NeuralNetwork::~NeuralNetwork()
	{
		delete[] WGI;
		delete[] WGF;
		delete[] WGO;
		delete[] WCI;

		delete[] WIP;
		delete[] WFP;
		delete[] WOP;

		delete[] W2;
	}

	void NeuralNetwork::Init(std::string inputFileModel)
	{
		std::ifstream inputStream;
		inputStream.open(inputFileModel, std::ifstream::in);

		if(!inputStream.good())
		{
			std::cerr << "ERROR: Failed to open " << inputFileModel << std::endl;
			return;
		}

		float weight;
		unsigned int local_count = 0, global_count = 0;

		// WGI NUMBER_OF_NEURONS * NUMBER_OF_INPUTS
		while(inputStream >> weight)
		{
			WGI[local_count] = weight;

			local_count++;
			global_count++;

			if(local_count == NUMBER_OF_NEURONS * NUMBER_OF_INPUTS)
			{
				local_count = 0;
				break;
			}
		}

		// WGF NUMBER_OF_NEURONS * NUMBER_OF_INPUTS
		while(inputStream >> weight)
		{
			WGF[local_count] = weight;

			local_count++;
			global_count++;

			if(local_count == NUMBER_OF_NEURONS * NUMBER_OF_INPUTS)
			{
				local_count = 0;
				break;
			}
		}

		// WGO NUMBER_OF_NEURONS * NUMBER_OF_INPUTS
		while(inputStream >> weight)
		{
			WGO[local_count] = weight;

			local_count++;
			global_count++;

			if(local_count == NUMBER_OF_NEURONS * NUMBER_OF_INPUTS)
			{
				local_count = 0;
				break;
			}
		}

		// WCI NUMBER_OF_NEURONS
		while(inputStream >> weight)
		{
			WCI[local_count] = weight;

			local_count++;
			global_count++;

			if(local_count == NUMBER_OF_NEURONS * NUMBER_OF_INPUTS)
			{
				local_count = 0;
				break;
			}
		}

		// WIP NUMBER_OF_NEURONS
		while(inputStream >> weight)
		{
			WIP[local_count] = weight;

			local_count++;
			global_count++;

			if(local_count == NUMBER_OF_NEURONS)
			{
				local_count = 0;
				break;
			}

		}

		// WFP NUMBER_OF_NEURONS
		while(inputStream >> weight)
		{
			WFP[local_count] = weight;

			local_count++;
			global_count++;

			if(local_count == NUMBER_OF_NEURONS)
			{
				local_count = 0;
				break;
			}
		}

		// WOP NUMBER_OF_NEURONS
		while(inputStream >> weight)
		{
			WOP[local_count] = weight;

			local_count++;
			global_count++;

			if(local_count == NUMBER_OF_NEURONS)
			{
				local_count = 0;
				break;
			}
		}

		// W2 NUMBER_OF_CLASSES * (NUMBER_OF_NEURONS * 2 + 1)
		while(inputStream >> weight)
		{
			W2[local_count] = weight;

			local_count++;
			global_count++;

			if(local_count == NUMBER_OF_CLASSES * (NUMBER_OF_NEURONS * 2 + 1))
			{
				local_count = 0;
				break;
			}
		}

		inputStream.close();

		if(global_count != NUMBER_OF_NEURONS * NUMBER_OF_INPUTS * 4 + NUMBER_OF_NEURONS * 3 + NUMBER_OF_CLASSES * (NUMBER_OF_NEURONS * 2 + 1))
		{
			std::cerr << "ERROR: Incorrect number of weights...!" << std::endl;
			return;
		}

		/* Snippet to print header files of weights from text model file */
		/*
		std::cout << "Header files for ROM instantiating :\n";
		std::cout << "WOP_fw[NUMBER_OF_NEURONS] = {";
		for(unsigned int i = 0; i < NUMBER_OF_NEURONS; i++)
			std::cout << WOP[i] << ",";
		std::cout << "};\n";
		*/
	}



	//====================================================================================================================================================================================================================
	// ALPHABET
	//====================================================================================================================================================================================================================

	// Constructor
	Alphabet::Alphabet()
	{
	}

	// Destructor
	Alphabet::~Alphabet()
	{
		//alphabet.clear();
	}

	void Alphabet::Init(std::string inputFileAlphabet)
	{
		std::ifstream inputStream;
		inputStream.open(inputFileAlphabet, std::ifstream::in);

		if(!inputStream.good())
		{
			std::cerr << "ERROR: Failed to open " << inputFileAlphabet << std::endl;
			return;
		}

		std::string symbol;
		//unsigned int i=0;
		while (getline(inputStream, symbol)){
			alphabet.push_back(symbol);
		}
		inputStream.close();

		if(alphabet.size() != NUMBER_OF_CLASSES)
			std::cerr << "ERROR: Wrong Number of Aplhabetic Symbols...!" << std::endl;

	}

	std::string Alphabet::ReturnSymbol(unsigned int label)
	{
		if(label < NUMBER_OF_CLASSES)
			return alphabet.at(label);
		else
		{
			std::cerr << "ERROR: The Class Number is out of Range...!" << std::endl;
			exit(-1); /* Do not proceed if out of range occurred */
			return alphabet.at(80);
		}
	}

	void Alphabet::Print()
	{
		for(unsigned int s = 0; s < NUMBER_OF_CLASSES; s++ )
		{
			std::cout << s << "   " << alphabet.at(s) << "   size: " << alphabet.at(s).size() << "   ";

			for(unsigned int i = 0; i < alphabet.at(s).size(); i++ )
				std::cout << (int)alphabet.at(s).at(i) << "   ";

			std::cout << std::endl;
		}

	}



	//====================================================================================================================================================================================================================
	// INPUT IMAGES
	//====================================================================================================================================================================================================================

	// Constructor
	InputImage::InputImage()
	{
		numberOfColumns = 0;
	}

	// Destructor
	InputImage::~InputImage()
	{
		if (image_fw != NULL)
		{
			delete[] image_fw;
			image_fw = NULL;
		}

		if (image_bw != NULL)
		{
			delete[] image_bw;
			image_bw = NULL;
		}
	}

	void InputImage::Init(std::string inputFileImage)
	{
		std::ifstream inputStream;
		inputStream.open(inputFileImage, std::ifstream::in);

		if(!inputStream.good())
		{
			std::cerr << "ERROR: Failed to open " << inputFileImage << std::endl;
			exit(BLSTM_TB_FAILURE);
			return;
		}

		// Temporal structure to store image
		std::vector<float> tmp;
		float pix;

		unsigned int values = 0;
		while((inputStream >> pix) && (values++ < MAX_NUMBER_COLUMNS_TEST_SET * HIGHT_IN_PIX))
		{
			if (values > BYPASS_COLUMNS * HIGHT_IN_PIX)
			tmp.push_back(pix);
		}

		inputStream.close();

		// Number of columns of the image has to be a multiple of HIGHT_IN_PIX
		if(tmp.size() % HIGHT_IN_PIX != 0)
		{
			std::cerr << "ERROR: Incorrect number of pixels...!" << std::endl;
			exit(BLSTM_TB_FAILURE);
			return;
		}

		numberOfColumns = (tmp.size() / HIGHT_IN_PIX);

		image_fw = new float [numberOfColumns * HIGHT_IN_PIX];
		image_bw = new float [numberOfColumns * HIGHT_IN_PIX];

		for(unsigned int col = 0; col < numberOfColumns; col++)
		{
			for(unsigned int row = 0; row < HIGHT_IN_PIX; row++)
			{
				image_fw[col * HIGHT_IN_PIX + row] = tmp.at(col * HIGHT_IN_PIX + row);

				// Creat an image for backward processing: mirror the columns of the forward image
				//image_bw[col * HIGHT_IN_PIX + row] = tmp.at((numberOfColumns - col - 1) * HIGHT_IN_PIX + row);
				image_bw[col * HIGHT_IN_PIX + row] = tmp.at((numberOfColumns - col - 1) * HIGHT_IN_PIX + (row));
			}
		}

		tmp.clear();
	}

	void InputImage::Free()
	{
		if (image_fw != NULL)
		{
			delete[] image_fw;
			image_fw = NULL;
		}

		if (image_bw != NULL)
		{
			delete[] image_bw;
			image_bw = NULL;
		}
	}

	void InputImage::Print()
	{
		unsigned int numberOfSamples = 10;

		std::cout << "image: ";

		for(unsigned int i = 0; i < numberOfSamples; i++)
			std::cout << image_fw[i] << " ";
		std::cout << " ... " << "image[C * N]: " << image_fw[numberOfColumns * HIGHT_IN_PIX - 2] << " " << image_fw[numberOfColumns * HIGHT_IN_PIX - 1] << std::endl;
	}




	//====================================================================================================================================================================================================================
	// GROUND TRUTH
	//====================================================================================================================================================================================================================

	// Constructor
	GroundTruth::GroundTruth()
	{
	}

	// Destructor
	GroundTruth::~GroundTruth()
	{
		groundTruth.clear();
	}

	void GroundTruth::Free()
	{
		groundTruth.clear();
	}

	void GroundTruth::Init(std::string inputFileGroundTruth)
	{
		std::ifstream inputStream;
		inputStream.open(inputFileGroundTruth, std::ifstream::in);

		if(!inputStream.good())
		{
			std::cerr << "ERROR: Failed to open " << inputFileGroundTruth << std::endl;
			return;
		}

		std::string symbol;

		while (getline(inputStream, symbol))
		{
			groundTruth = symbol;
		}

		inputStream.close();

	}

	std::string GroundTruth::ReturnString()
	{
		return groundTruth;
	}

#endif

#ifndef EMULATING_IO_SINGLE_KERNEL_BLSTM

	  /**
	   *  @brief  Return the index of a maximum element in a 1D array.
	   *  @param  first  Start index of range.
	   *  @param  last   End index of range.
	   *  @return  Index referencing the first instance of the largest value.
	  */
	    unsigned int max_element_syn_test(DTYPE_TRNLB image[MAX_NUMBER_COLUMNS_TEST_SET][NUMBER_OF_CLASSES], unsigned int __first, unsigned int __last)
	    {
	    //std::cout << "max_element_syn_test" << std::endl;
	    //unsigned int i=0;
		#pragma HLS INLINE off
		  const int min_tripcount = NUMBER_OF_CLASSES;
		  const int avg_tripcount = MAX_NUMBER_COLUMNS_TEST_SET; // based on profiling 1759 was max;
		  const int max_tripcount = MAX_NUMBER_COLUMNS_TEST_SET;// * NUMBER_OF_CLASSES ;

		  if (__first == __last)
			  return __first;

	      unsigned int __result = __first;
	      while (++__first != __last) {
	    	//i++;
			#pragma HLS PIPELINE II=2 // FIXME: shall check with co-sim if enable_flush is needed, since dynamic bounds
			#pragma HLS LOOP_TRIPCOUNT min=min_tripcount avg=avg_tripcount max=max_tripcount
	    	  DTYPE_TRNLB check1 = image[__result/NUMBER_OF_CLASSES][__result%NUMBER_OF_CLASSES];
	    	  DTYPE_TRNLB check2 = image[__first/NUMBER_OF_CLASSES][__first%NUMBER_OF_CLASSES];
	    	  if (check1 < check2)
	    		__result = __first;
	      }
	      //std::cout << "i = " << i << std::endl;
	      return __result;
	    }


	//====================================================================================================================================================================================================================
	// LSTM
	//====================================================================================================================================================================================================================

	// The dot product corresponding to a single gate of the LSTM memory cell
	inline float DotVectorToVector126(//float source[NUMBER_OF_INPUTS],
									  hls::stream<float> &source,// IN  // size: 1.0 + HIGHT_IN_PIX + NUMBER_OF_NEURONS = NUMBER_OF_INPUTS
									  DTYPE weights[NUMBER_OF_INPUTS])	// IN  // size: NUMBER_OF_INPUTS
	{
	#pragma HLS INLINE off
		float output = 0.0;
		float tmp;
		for(unsigned int i = 0; i < NUMBER_OF_INPUTS; i++)
		{
	#pragma HLS PIPELINE II=1
			tmp = source.read() * (float)weights[i];
			output += tmp;
		}

		return output;
	}

	// 		DSP	FF		LUT		latency
	// 9 -> 36	3097	1365	5823110
	// 14->	56	4245	1827	5365610
	const int cyclic_factor = INPUT_LAYER_UNROLL_FACTOR;

	// The dot product corresponding to a four gates of the LSTM memory cell
	// OPS: 8 x NUMBER_OF_INPUTS = 1008
	void DotVectorToVector126_four_fw(DTYPE_IMG source[NUMBER_OF_INPUTS],
									  DTYPE_WEIGHTS_WGI_fw weights0[NUMBER_OF_INPUTS],
									  DTYPE_WEIGHTS_WGF_fw weights1[NUMBER_OF_INPUTS],
									  DTYPE_WEIGHTS_WGO_fw weights2[NUMBER_OF_INPUTS],
									  DTYPE_WEIGHTS_WCI_fw weights3[NUMBER_OF_INPUTS],
									  DTYPE_IN outputs[4])	// IN  // size: NUMBER_OF_INPUTS
	{
		#pragma HLS ARRAY_PARTITION variable=outputs complete dim=1

		#pragma HLS INLINE off
		outputs[0] = outputs[1] = outputs[2] = outputs[3] = 0.0;
		DTYPE_IN tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
		for(unsigned int i = 0; i < NUMBER_OF_INPUTS; i++)
		{
		#pragma HLS PIPELINE rewind

			DTYPE_IN src_fw1 = source[i];
			tmp0 = src_fw1 * (DTYPE_IN)weights0[i];
			tmp1 = src_fw1 * (DTYPE_IN)weights1[i];
			tmp2 = src_fw1 * (DTYPE_IN)weights2[i];
			tmp3 = src_fw1 * (DTYPE_IN)weights3[i];

			outputs[0] += tmp0 ;
			outputs[1] += tmp1 ;
			outputs[2] += tmp2 ;
			outputs[3] += tmp3 ;

		}
	}


	// The dot product corresponding to a four gates of the LSTM memory cell
	// OPS: 8 x NUMBER_OF_INPUTS = 1008
	void DotVectorToVector126_four_bw(DTYPE_IMG source[NUMBER_OF_INPUTS],
									  DTYPE_WEIGHTS_WGI_bw weights0[NUMBER_OF_INPUTS],
									  DTYPE_WEIGHTS_WGF_bw weights1[NUMBER_OF_INPUTS],
									  DTYPE_WEIGHTS_WGO_bw weights2[NUMBER_OF_INPUTS],
									  DTYPE_WEIGHTS_WCI_bw weights3[NUMBER_OF_INPUTS],
									  DTYPE_IN outputs[4])	// IN  // size: NUMBER_OF_INPUTS
	{
		#pragma HLS ARRAY_PARTITION variable=outputs complete dim=1

		#pragma HLS INLINE off
		outputs[0] = outputs[1] = outputs[2] = outputs[3] = 0.0;
		DTYPE_IN tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
		for(unsigned int i = 0; i < NUMBER_OF_INPUTS; i++)
		{
		#pragma HLS PIPELINE rewind

			DTYPE_IN src_bw1 = source[i];
			tmp0 = src_bw1 * (DTYPE_IN)weights0[i];
			tmp1 = src_bw1 * (DTYPE_IN)weights1[i];
			tmp2 = src_bw1 * (DTYPE_IN)weights2[i];
			tmp3 = src_bw1 * (DTYPE_IN)weights3[i];

			outputs[0] += tmp0 ;
			outputs[1] += tmp1 ;
			outputs[2] += tmp2 ;
			outputs[3] += tmp3 ;

		}
	}

	// The dot product corresponding to a four gates of the LSTM memory cell
	void OLD_DotVectorToVector126_four_bw(DTYPE_IMG source[NUMBER_OF_INPUTS],
									  //hls::stream<float> &source,// IN  // size: 1.0 + HIGHT_IN_PIX + NUMBER_OF_NEURONS = NUMBER_OF_INPUTS
									  DTYPE_WEIGHTS_WGI_bw weights0[NUMBER_OF_INPUTS],
									  DTYPE_WEIGHTS_WGF_bw weights1[NUMBER_OF_INPUTS],
									  DTYPE_WEIGHTS_WGO_bw weights2[NUMBER_OF_INPUTS],
									  DTYPE_WEIGHTS_WCI_bw weights3[NUMBER_OF_INPUTS],
									  DTYPE_IN outputs[4])	// IN  // size: NUMBER_OF_INPUTS
	{
		#pragma HLS ARRAY_PARTITION variable=outputs complete dim=1
		#pragma HLS INLINE off
		outputs[0] = outputs[1] = outputs[2] = outputs[3] = 0.0;
		DTYPE_IN tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
		for(unsigned int i = 0; i < NUMBER_OF_INPUTS/2; i++)
		{
		#pragma HLS PIPELINE II=1
			DTYPE_IN src_fw1 = source[i];
			DTYPE_IN src_fw2 = source[i+NUMBER_OF_INPUTS/2];
			tmp0 = src_fw1 * (DTYPE_IN)weights0[i];
			tmp1 = src_fw1 * (DTYPE_IN)weights1[i];
			tmp2 = src_fw1 * (DTYPE_IN)weights2[i];
			tmp3 = src_fw1 * (DTYPE_IN)weights3[i];

			tmp4 = src_fw2 * (DTYPE_IN)weights0[i+NUMBER_OF_INPUTS/2];
			tmp5 = src_fw2 * (DTYPE_IN)weights1[i+NUMBER_OF_INPUTS/2];
			tmp6 = src_fw2 * (DTYPE_IN)weights2[i+NUMBER_OF_INPUTS/2];
			tmp7 = src_fw2 * (DTYPE_IN)weights3[i+NUMBER_OF_INPUTS/2];

			outputs[0] += tmp0 + tmp4;
			outputs[1] += tmp1 + tmp5;
			outputs[2] += tmp2 + tmp6;
			outputs[3] += tmp3 + tmp7;

		}
	}

	// OPS: 5
	DTYPE_IN divexpf_lookup(DTYPE_IN x) {
	#pragma HLS INLINE
		const DTYPE_IN step = 1/((fabs(MAX_TARGET_DIVEXPF)+fabs(MIN_TARGET_DIVEXPF)) / (LUT_SIZE_DIVEXPF-1));
		if (x <= MIN_TARGET_DIVEXPF)
			return 0;
		else if (x >= MAX_TARGET_DIVEXPF)
			return 1;
		else {
			//Since the divisor is a constant, then the problem devolves into a multiplication by a constant (the reciprocal of the divisor).
			int index = int(((x-(MIN_TARGET_DIVEXPF))*step));
			//printf("DEBUG: divexpf_lut[%d]=%f, step=%f\n", index, divexpf_lut[index], step);
			return divexpf_lut[index];
		}
	}

	// OPS: 5
	DTYPE_IN tanh_lookup(DTYPE_IN x) {
	#pragma HLS INLINE
		const DTYPE_IN step = 1/((fabs(MAX_TARGET_TANH)+fabs(MIN_TARGET_TANH)) / (LUT_SIZE_TANH-1));
		if (x <= MIN_TARGET_TANH)
			return -1;
		else if (x >= MAX_TARGET_TANH)
			return 1;
		else {
			//Since the divisor is a constant, then the problem devolves into a multiplication by a constant (the reciprocal of the divisor).
			int index = int(((x-(MIN_TARGET_TANH))*step));
			//printf("DEBUG: tanh_lut[%d]=%f, step=%f\n", index, tanh_lut[index], step);
			return tanh_lut[index];
		}
	}

	// OPS: 5
	DTYPE_OUTPUT expf_lookup(DTYPE_OUTPUT x) {
	#pragma HLS INLINE
		const DTYPE_OUTPUT step = 1/((fabs(MAX_TARGET_EXPF)+fabs(MIN_TARGET_EXPF)) / (LUT_SIZE_EXPF-1));
		if (x <= MIN_TARGET_EXPF)
			return 0;
		else if (x >= MAX_TARGET_EXPF)
			return expf_lut[LUT_SIZE_EXPF-1];
			//return (DTYPE_OUTPUT)FLT_MAX;
		else {
			//Since the divisor is a constant, then the problem devolves into a multiplication by a constant (the reciprocal of the divisor).
			int index = int(((x-(MIN_TARGET_EXPF))*step));
			//printf("DEBUG: expf_lut[%d]=%f, step=%f\n", index, expf_lut[index], step);
			return expf_lut[index];
		}
	}

	// The function of a single LSTM memory cell
	// OPS: 10(self) + 1028(nested) = 1038
	void HiddenLayerSingleMemoryCell_fw(DTYPE_IMG source[NUMBER_OF_INPUTS],					// IN  // size: 1.0 + HIGHT_IN_PIX + NUMBER_OF_NEURONS = NUMBER_OF_INPUTS
									 unsigned int currentColumn,	// IN  // The current column of the image
									 DTYPE_IN in_state,				// IN  // A single input state
									 DTYPE_WEIGHTS_WGI_fw WGI[NUMBER_OF_INPUTS],   // IN  // size: NUMBER_OF_INPUTS
									 DTYPE_WEIGHTS_WGF_fw WGF[NUMBER_OF_INPUTS],   // IN  // size: NUMBER_OF_INPUTS
									 DTYPE_WEIGHTS_WGO_fw WGO[NUMBER_OF_INPUTS],   // IN  // size: NUMBER_OF_INPUTS
									 DTYPE_WEIGHTS_WCI_fw WCI[NUMBER_OF_INPUTS],   // IN  // size: NUMBER_OF_INPUTS
									 DTYPE_WEIGHTS_WIP_fw WIP,						// IN  // A single peephole weight
									 DTYPE_WEIGHTS_WFP_fw WFP,						// IN  // A single peephole weight
									 DTYPE_WEIGHTS_WOP_fw WOP,						// IN  // A single peephole weight
									 DTYPE_IN *out_state,				// OUT // A single output state
									 DTYPE_IN *output)              	// OUT // A single output

	{
//#pragma HLS DATAFLOW
#if TRIGF_APPROX == 2
		// Force not to create multiple ROMs for every call, i.e. latency overhead is only 1-2 cycles x number_of_calls,
		// but resources saving are more important here for LUTs
		#pragma HLS allocation instances=divexpf_lookup limit=1 function
		#pragma HLS allocation instances=tanh_lookup limit=1 function
#endif
		DTYPE_IN gix, gfx, gox, cix;
		DTYPE_IN gi, gf, go, ci;
		DTYPE_IN tmp_in_state;
		DTYPE_IN tmp_out_state;
		DTYPE_IN outputs[4];

		DotVectorToVector126_four_fw(source, WGI, WGF, WGO, WCI, outputs);

		tmp_in_state = in_state;

		gix = outputs[0];
		gfx = outputs[1];
		gox = outputs[2];
		cix = outputs[3];

		if(currentColumn > 0)
		{
			gix = gix + (DTYPE_IN)WIP * tmp_in_state;
			gfx = gfx + (DTYPE_IN)WFP * tmp_in_state;
		}
		#if TRIGF_APPROX == 0
		gi = 1.0/(1.0 + expf(-(float)gix));
		gf = 1.0/(1.0 + expf(-(float)gfx));
		ci = (DTYPE_IN)tanhf((float)cix);
		#elif TRIGF_APPROX == 1
		gi = 1.0/(1.0 + tiny_expf(-gix));
		gf = 1.0/(1.0 + tiny_expf(-gfx));
		ci = tiny_tanhf(cix);
		#elif TRIGF_APPROX == 2
		gi = divexpf_lookup(gix);
		gf = divexpf_lookup(gfx);
		ci = tanh_lookup(cix);
		#elif TRIGF_APPROX == 3
		gi = divexpf_pwl(gix);
		gf = divexpf_pwl(gfx);
		ci = tanh_pwl(cix);
		#endif

		tmp_out_state = ci * gi;

		//fprintf(fp, "%.10f\n%.10f\n", (float)gix, (float)gfx);
		//fprintf(fp, "%.10f\n", (float)cix);

		if(currentColumn > 0)
		{
			tmp_out_state = tmp_out_state + gf * tmp_in_state;
			gox = gox + (DTYPE_IN)WOP * tmp_out_state;
		}
		#if TRIGF_APPROX == 0
		go = (DTYPE_IN)(1.0/(1.0 + expf(-(float)gox)));
		*output = (DTYPE_IN)tanhf((float)tmp_out_state) * go;
		#elif TRIGF_APPROX == 1
		go = 1.0/(1.0 + tiny_expf(-gox));
		*output = tiny_tanhf(tmp_out_state) * go;
		#elif TRIGF_APPROX == 2
		go = divexpf_lookup(gox);
		*output =  tanh_lookup(tmp_out_state) * go;
		#elif TRIGF_APPROX == 3
		go = divexpf_pwl(gox);
		*output =  tanh_pwl(tmp_out_state) * go;
		#endif
		//
		*out_state = tmp_out_state;
	}


	// The function of a single LSTM memory cell
	// OPS: 10(self) + 1028(nested) = 1038
	void HiddenLayerSingleMemoryCell_bw(DTYPE_IMG source[NUMBER_OF_INPUTS],					// IN  // size: 1.0 + HIGHT_IN_PIX + NUMBER_OF_NEURONS = NUMBER_OF_INPUTS
									 unsigned int currentColumn,	// IN  // The current column of the image
									 DTYPE_IN in_state,				// IN  // A single input state
									 DTYPE_WEIGHTS_WGI_bw WGI[NUMBER_OF_INPUTS],   // IN  // size: NUMBER_OF_INPUTS
									 DTYPE_WEIGHTS_WGF_bw WGF[NUMBER_OF_INPUTS],   // IN  // size: NUMBER_OF_INPUTS
									 DTYPE_WEIGHTS_WGO_bw WGO[NUMBER_OF_INPUTS],   // IN  // size: NUMBER_OF_INPUTS
									 DTYPE_WEIGHTS_WCI_bw WCI[NUMBER_OF_INPUTS],   // IN  // size: NUMBER_OF_INPUTS
									 DTYPE_WEIGHTS_WIP_bw WIP,						// IN  // A single peephole weight
									 DTYPE_WEIGHTS_WFP_bw WFP,						// IN  // A single peephole weight
									 DTYPE_WEIGHTS_WOP_bw WOP,						// IN  // A single peephole weight
									 DTYPE_IN *out_state,				// OUT // A single output state
									 DTYPE_IN *output)              	// OUT // A single output

	{
//#pragma HLS DATAFLOW
#if TRIGF_APPROX == 2
		// Force not to create multiple ROMs for every call, i.e. latency overhead is only 1-2 cycles x number_of_calls,
		// but resources saving are more important here for LUTs
		#pragma HLS allocation instances=divexpf_lookup limit=1 function
		#pragma HLS allocation instances=tanh_lookup limit=1 function
#endif
		DTYPE_IN gix, gfx, gox, cix;
		DTYPE_IN gi, gf, go, ci;
		DTYPE_IN tmp_in_state;
		DTYPE_IN tmp_out_state;
		DTYPE_IN outputs[4];

		DotVectorToVector126_four_bw(source, WGI, WGF, WGO, WCI, outputs);

		tmp_in_state = in_state;

		gix = outputs[0];
		gfx = outputs[1];
		gox = outputs[2];
		cix = outputs[3];

		if(currentColumn > 0)
		{
			gix = gix + (DTYPE_IN)WIP * tmp_in_state;
			gfx = gfx + (DTYPE_IN)WFP * tmp_in_state;
		}
		#if TRIGF_APPROX == 0
		gi = 1.0/(1.0 + expf(-(float)gix));
		gf = 1.0/(1.0 + expf(-(float)gfx));
		ci = (DTYPE_IN)tanhf((float)cix);
		#elif TRIGF_APPROX == 1
		gi = 1.0/(1.0 + tiny_expf(-gix));
		gf = 1.0/(1.0 + tiny_expf(-gfx));
		ci = tiny_tanhf(cix);
		#elif TRIGF_APPROX == 2
		gi = divexpf_lookup(gix);
		gf = divexpf_lookup(gfx);
		ci = tanh_lookup(cix);
		#elif TRIGF_APPROX == 3
		gi = divexpf_pwl(gix);
		gf = divexpf_pwl(gfx);
		ci = tanh_pwl(cix);
		#endif

		tmp_out_state = ci * gi;

		if(currentColumn > 0)
		{
			tmp_out_state = tmp_out_state + gf * tmp_in_state;
			gox = gox + (DTYPE_IN)WOP * tmp_out_state;
		}
		#if TRIGF_APPROX == 0
		go = (DTYPE_IN)(1.0/(1.0 + expf(-(float)gox)));
		*output = (DTYPE_IN)tanhf((float)tmp_out_state) * go;
		#elif TRIGF_APPROX == 1
		go = 1.0/(1.0 + tiny_expf(-gox));
		*output = tiny_tanhf(tmp_out_state) * go;
		#elif TRIGF_APPROX == 2
		go = divexpf_lookup(gox);
		*output =  tanh_lookup(tmp_out_state) * go;
		#elif TRIGF_APPROX == 3
		go = divexpf_pwl(gox);
		*output =  tanh_pwl(tmp_out_state) * go;
		#endif
		//
		*out_state = tmp_out_state;
	}


	// OPS: 200+COLSx(125+400+100x1038) = 76366100
	void Hidden_Layer_fw(
						#ifdef INTERFACE_IS_STREAM
					  	  hls::stream<DTYPE_IMG> &image, 	// IN  // size: numberOfColumns * HIGHT_IN_PIX
						#else
						  DTYPE_IMG *image, 				// IN // [MAX_NUMBER_COLUMNS_TEST_SET * HIGHT_IN_PIX],
						#endif
						  unsigned int numberOfColumns,		// IN  //
						  hls::stream<DTYPE_LAYERS> &result)// OUT // size: numberOfColumns * NUMBER_OF_NEURONS
	{
		DTYPE_IMG source[NUMBER_OF_INPUTS];
		/*
		FILE *fd = fopen("/tmp/values.txt", "w");
		for(unsigned int i = 0; i < NUMBER_OF_NEURONS; i++)
			for(unsigned int j = 0; j < NUMBER_OF_INPUTS; j++)
			fprintf(fd, "%f %f %f %f\n", (float)WGI_fw_2d[i][j], (float)WGF_fw_2d[i][j], (float)WGO_fw_2d[i][j], (float)WCI_fw_2d[i][j]);
		fclose(fd);
		exit(-1);
		 */

		DTYPE_LAYERS outputRegister[NUMBER_OF_NEURONS];
		DTYPE_IN stateRegister[NUMBER_OF_NEURONS];

		for(unsigned int i = 0; i < NUMBER_OF_NEURONS; i++) {
		#pragma HLS UNROLL
			outputRegister[i] = 0.0;
		}

		//printf("DEBUG: numberOfColumns: %d\n", numberOfColumns);
		unsigned int column_index;
		for(unsigned int column = 0; column < numberOfColumns; column++)
		{
			const int max_col_test_set = MAX_NUMBER_COLUMNS_TEST_SET;
			#pragma HLS LOOP_TRIPCOUNT min=1 max=max_col_test_set
			//Concatinate 1.0 + image + previous output
			source[0] = 1.0;

			for(unsigned int k = 0; k < HIGHT_IN_PIX ; k++) {
#ifdef INTERFACE_IS_STREAM
			#pragma HLS pipeline II=1
				//if (k < HIGHT_IN_PIX)
					source[k+1] = image.read();
				//else
				//	source[k+1] = outputRegister[k-HIGHT_IN_PIX];
				//u.f = source[k+1];
				//printf("image[%u]=%08x\n", column*HIGHT_IN_PIX+k, (unsigned int)u.t);
				//std::cout << "image.read(" << global++ << ") = " << source[k+1] << std::endl;
#else
				source[k+1] = image[column*HIGHT_IN_PIX+k];
#endif
			}
			for(unsigned int k = 0; k < NUMBER_OF_NEURONS; k++) {
#pragma HLS pipeline II=1
				source[k+1+HIGHT_IN_PIX] = outputRegister[k];
			}

			for(unsigned int n = 0; n < NUMBER_OF_NEURONS; n++)
			{
//#pragma HLS UNROLL factor=4
//#pragma HLS pipeline II=1 // x14 FF-LUT resources, 5x speedup
#if DIMENSION_OF_WEIGHTS == 1
				DTYPE_WEIGHTS_WGI_bw *pWGI = WGI_fw + n * NUMBER_OF_INPUTS;
				DTYPE_WEIGHTS_WGF_bw *pWGF = WGF_fw + n * NUMBER_OF_INPUTS;
				DTYPE_WEIGHTS_WGO_bw *pWGO = WGO_fw + n * NUMBER_OF_INPUTS;
				DTYPE_WEIGHTS_WCI_bw *pWCI = WCI_fw + n * NUMBER_OF_INPUTS;
#else
				DTYPE_WEIGHTS_WGI_fw *pWGI = WGI_fw_2d[n];
				DTYPE_WEIGHTS_WGF_fw *pWGF = WGF_fw_2d[n];
				DTYPE_WEIGHTS_WGO_fw *pWGO = WGO_fw_2d[n];
				DTYPE_WEIGHTS_WCI_fw *pWCI = WCI_fw_2d[n];
#endif
				DTYPE_IN out_state, output;

				HiddenLayerSingleMemoryCell_fw(source,
											column,
										    stateRegister[n],
											pWGI,
										    pWGF,
										    pWGO,
										    pWCI,
										    WIP_fw[n],
										    WFP_fw[n],
										    WOP_fw[n],
											&out_state,
											&output);

				stateRegister[n] = out_state;
				outputRegister[n] = output;
				result.write(outputRegister[n]);
			}
		}
	}


	// OPS: 200+COLSx(125+400+100x1038) = 76366100
	void Hidden_Layer_bw(
						#ifdef INTERFACE_IS_STREAM
					  	  hls::stream<DTYPE_IMG> &image, 	// IN  // size: numberOfColumns * HIGHT_IN_PIX
						#else
						  DTYPE_IMG *image, 				// IN // [MAX_NUMBER_COLUMNS_TEST_SET * HIGHT_IN_PIX],
						#endif
						  unsigned int numberOfColumns,		// IN  //
						  hls::stream<DTYPE_LAYERS> &result)// OUT // size: numberOfColumns * NUMBER_OF_NEURONS
	{
		DTYPE_IMG source[NUMBER_OF_INPUTS];

		DTYPE_LAYERS outputRegister[NUMBER_OF_NEURONS];
		DTYPE_IN stateRegister[NUMBER_OF_NEURONS];

		for(unsigned int i = 0; i < NUMBER_OF_NEURONS; i++) {
		#pragma HLS UNROLL
			outputRegister[i] = 0.0;
		}

		//printf("DEBUG: numberOfColumns: %d\n", numberOfColumns);
		unsigned int column_index;
		for(unsigned int column = 0; column < numberOfColumns; column++)
		{
			const int max_col_test_set = MAX_NUMBER_COLUMNS_TEST_SET;
			#pragma HLS LOOP_TRIPCOUNT min=1 max=max_col_test_set
			//Concatinate 1.0 + image + previous output
			source[0] = 1.0;

			for(unsigned int k = 0; k < HIGHT_IN_PIX ; k++) {
#ifdef INTERFACE_IS_STREAM
			#pragma HLS pipeline II=1
				//if (k < HIGHT_IN_PIX)
					source[k+1] = image.read();
				//else
				//	source[k+1] = outputRegister[k-HIGHT_IN_PIX];
				//u.f = source[k+1];
				//printf("image[%u]=%08x\n", column*HIGHT_IN_PIX+k, (unsigned int)u.t);
				//std::cout << "image.read(" << global++ << ") = " << source[k+1] << std::endl;
#else
				source[k+1] = image[column*HIGHT_IN_PIX+k];
#endif
			}
			for(unsigned int k = 0; k < NUMBER_OF_NEURONS; k++) {
#pragma HLS pipeline II=1
				source[k+1+HIGHT_IN_PIX] = outputRegister[k];
			}

			for(unsigned int n = 0; n < NUMBER_OF_NEURONS; n++)
			{
//#pragma HLS UNROLL factor=4
//#pragma HLS pipeline II=1 // x14 FF-LUT resources, 5x speedup
#if DIMENSION_OF_WEIGHTS == 1
				DTYPE_WEIGHTS_WGI_bw *pWGI = WGI_bw + n * NUMBER_OF_INPUTS;
				DTYPE_WEIGHTS_WGF_bw *pWGF = WGF_bw + n * NUMBER_OF_INPUTS;
				DTYPE_WEIGHTS_WGO_bw *pWGO = WGO_bw + n * NUMBER_OF_INPUTS;
				DTYPE_WEIGHTS_WCI_bw *pWCI = WCI_bw + n * NUMBER_OF_INPUTS;
#else
				DTYPE_WEIGHTS_WGI_bw *pWGI = WGI_bw_2d[n];
				DTYPE_WEIGHTS_WGF_bw *pWGF = WGF_bw_2d[n];
				DTYPE_WEIGHTS_WGO_bw *pWGO = WGO_bw_2d[n];
				DTYPE_WEIGHTS_WCI_bw *pWCI = WCI_bw_2d[n];
#endif
				DTYPE_IN out_state, output;

				HiddenLayerSingleMemoryCell_bw(source,
											column,
										    stateRegister[n],
											pWGI,
										    pWGF,
										    pWGO,
										    pWCI,
										    WIP_bw[n],
										    WFP_bw[n],
										    WOP_bw[n],
											&out_state,
											&output);

				stateRegister[n] = out_state;
				outputRegister[n] = output;
				result.write(outputRegister[n]);
			}
		}
	}



	void OLD_Hidden_Layer_bw(
#ifdef INTERFACE_IS_STREAM
					  hls::stream<DTYPE_IMG> &image, // IN  // size: numberOfColumns * HIGHT_IN_PIX
#else
					  DTYPE_IMG *image, // IN // [MAX_NUMBER_COLUMNS_TEST_SET * HIGHT_IN_PIX],
#endif
					  unsigned int numberOfColumns,			// IN  //
					  DTYPE_WEIGHTS_WGI_bw WGI[NUMBER_OF_INPUTS], 	// IN  // size: NUMBER_OF_NEURONS * NUMBER_OF_INPUTS
					  DTYPE_WEIGHTS_WGF_bw WGF[NUMBER_OF_INPUTS], 	// IN  // size: NUMBER_OF_NEURONS * NUMBER_OF_INPUTS
					  DTYPE_WEIGHTS_WGO_bw WGO[NUMBER_OF_INPUTS],	// IN  // size: NUMBER_OF_NEURONS * NUMBER_OF_INPUTS
					  DTYPE_WEIGHTS_WCI_bw WCI[NUMBER_OF_INPUTS], 	// IN  // size: NUMBER_OF_NEURONS * NUMBER_OF_INPUTS
					  DTYPE_WEIGHTS_WIP_bw WIP[NUMBER_OF_INPUTS], 	// IN  // size: NUMBER_OF_NEURONS
					  DTYPE_WEIGHTS_WFP_bw WFP[NUMBER_OF_INPUTS], 	// IN  // size: NUMBER_OF_NEURONS
					  DTYPE_WEIGHTS_WOP_bw WOP[NUMBER_OF_INPUTS],	// IN  // size: NUMBER_OF_NEURONS
					  hls::stream<DTYPE_LAYERS> &result)	// OUT // size: numberOfColumns * NUMBER_OF_NEURONS
	{

		DTYPE_IMG source[NUMBER_OF_INPUTS];
		DTYPE_LAYERS outputRegister[NUMBER_OF_NEURONS];
		DTYPE_IN stateRegister[NUMBER_OF_NEURONS];

		for(unsigned int i = 0; i < NUMBER_OF_NEURONS; i++) {
		#pragma HLS UNROLL
			outputRegister[i] = 0.0;
		}

		//printf("DEBUG: numberOfColumns: %d\n", numberOfColumns);
		unsigned int column_index;
		for(unsigned int column = 0; column < numberOfColumns; column++)
		{

			const int max_col_test_set = MAX_NUMBER_COLUMNS_TEST_SET;
			#pragma HLS LOOP_TRIPCOUNT min=1 max=max_col_test_set
			//Concatinate 1.0 + image + previous output
			source[0] = 1.0;


			for(unsigned int k = 0; k < HIGHT_IN_PIX; k++) {
#ifdef INTERFACE_IS_STREAM
				source[k+1] = image.read();
				//u.f = source[k+1];
				//printf("image[%u]=%08x\n", column*HIGHT_IN_PIX+k, (unsigned int)u.t);
				//std::cout << "image.read(" << global++ << ") = " << source[k+1] << std::endl;
#else
				source[k+1] = image[column*HIGHT_IN_PIX+k];
#endif
			}
			for(unsigned int k = 0; k < NUMBER_OF_NEURONS; k++) {
				source[k+1+HIGHT_IN_PIX] = outputRegister[k];
			}

			for(unsigned int n = 0; n < NUMBER_OF_NEURONS; n++)
			{
//#pragma HLS UNROLL factor=4
//#pragma HLS pipeline II=1 // x14 FF-LUT resources, 5x speedup
				DTYPE_WEIGHTS_WGI_bw *pWGI = WGI + n * NUMBER_OF_INPUTS;
				DTYPE_WEIGHTS_WGF_bw *pWGF = WGF + n * NUMBER_OF_INPUTS;
				DTYPE_WEIGHTS_WGO_bw *pWGO = WGO + n * NUMBER_OF_INPUTS;
				DTYPE_WEIGHTS_WCI_bw *pWCI = WCI + n * NUMBER_OF_INPUTS;

				DTYPE_IN out_state, output;

				HiddenLayerSingleMemoryCell_bw(source,
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
				result.write(outputRegister[n]);
			}
		}
	}


	//====================================================================================================================================================================================================================
	// Connectionist Temporal Classification Layer (CTC layer)
	//====================================================================================================================================================================================================================
const int cyclic_factor_out = OUTPUT_LAYER_UNROLL_FACTOR;
	// The dot product corresponding to a single neuron of the output layer operating on an concatinated output from the forward and the bakward hidden layers
	// OPS: 800
	inline void DotVectorToVector201(DTYPE_WEIGHTS_W2 W2[NUMBER_OF_NEURONS],		// IN  // size: NUMBER_OF_NEURONS * 2 + 1
									  DTYPE_LAYERS input_fw[NUMBER_OF_NEURONS], 	// IN  // size: NUMBER_OF_NEURONS
									  DTYPE_LAYERS input_bw[NUMBER_OF_NEURONS],		// IN  // size: NUMBER_OF_NEURONS
									  DTYPE_OUTPUT* poutput)
	{

#pragma HLS INLINE off
		DTYPE_OUTPUT output = 0.0;
		DTYPE_OUTPUT tmp1, tmp2;

		output = W2[0];

		for(unsigned int i = 0; i < NUMBER_OF_NEURONS; i++)
		{
#pragma HLS PIPELINE II=1
			// tmp1 tmp2 were in different loops, merged for latency opt.
			tmp1 = (DTYPE_LAYERS)W2[1 + i] * input_fw[i];
			tmp2 = (DTYPE_LAYERS)W2[1 + NUMBER_OF_NEURONS + i] * input_bw[i];
			output += tmp1 + tmp2;
		}

		*poutput = output;
	}


// The dot product corresponding to a single neuron of the output layer operating on an concatinated output from the forward and the bakward hidden layers
	inline void DotVectorToVector201_dual_port_RAM(DTYPE_WEIGHTS_W2 W2[NUMBER_OF_NEURONS],	// IN  // size: NUMBER_OF_NEURONS * 2 + 1
									  DTYPE_LAYERS input_fw[NUMBER_OF_NEURONS], 			// IN  // size: NUMBER_OF_NEURONS
									  DTYPE_LAYERS input_bw[NUMBER_OF_NEURONS],				// IN  // size: NUMBER_OF_NEURONS
									  DTYPE_OUTPUT* poutput)
	{
#pragma HLS INLINE off
		DTYPE_OUTPUT output = 0.0;
		DTYPE_OUTPUT tmp1, tmp2, tmp3, tmp4;

		output = W2[0];

		for(unsigned int i = 0; i < NUMBER_OF_NEURONS/2; i++)
		{
#pragma HLS PIPELINE II=1
			tmp1 = (DTYPE_LAYERS)W2[1 + i] * input_fw[i];
			tmp2 = (DTYPE_LAYERS)W2[1 + NUMBER_OF_NEURONS + i] * input_bw[i];

			tmp3 = (DTYPE_LAYERS)W2[1 + i + NUMBER_OF_NEURONS/2] * input_fw[i + NUMBER_OF_NEURONS/2];
			tmp4 = (DTYPE_LAYERS)W2[1 + NUMBER_OF_NEURONS + i + NUMBER_OF_NEURONS/2] * input_bw[i+ NUMBER_OF_NEURONS/2];

			output += tmp1 + tmp2 + tmp3 + tmp4;
		}

		*poutput = output;
	}

	// The function of a single neuron of the output layer
	inline void OutputLayerSinlgleNeuron(DTYPE_WEIGHTS_W2 *W2, 		// IN  // size: NUMBER_OF_NEURONS * 2 + 1
										 DTYPE_LAYERS *input_fw,	// IN  // size: NUMBER_OF_NEURONS
										 DTYPE_LAYERS *input_bw, 	// IN  // size: NUMBER_OF_NEURONS
										 DTYPE_OUTPUT *output)		// OUT //
	{
	//#pragma HLS INLINE
		//DotVectorToVector201_dual_port_RAM(W2, input_fw, input_bw, output);
		DotVectorToVector201(W2, input_fw, input_bw, output);
	}

	// OPS:  = 732*201 COLSx(200+110x(800+5+1+4)) = 65367600
	void Output_Layer(unsigned int numberOfColumns, // IN  //
	          //float input_fw[MAX_NUMBER_COLUMNS_TEST_SET * NUMBER_OF_NEURONS],	// IN  // size: numberOfColumns * NUMBER_OF_NEURONS
	          //float input_bw[MAX_NUMBER_COLUMNS_TEST_SET * NUMBER_OF_NEURONS],	// IN  // size: numberOfColumns * NUMBER_OF_NEURONS
	          hls::stream<DTYPE_LAYERS> &input_fws,
	          hls::stream<DTYPE_LAYERS> &input_bws,
	          //float output[MAX_NUMBER_COLUMNS_TEST_SET * NUMBER_OF_CLASSES]) 	// OUT // size: numberOfColumns * NUMBER_OF_CLASSES
	          hls::stream<DTYPE_TRNLB> &output
	#if SHARED_MEM == 1
	          ,DTYPE_LAYERS *input_bw)
	#else
	          )
	#endif
	  {
	//#pragma HLS DATAFLOW
	  DTYPE_OUTPUT sum;

	  DTYPE_LAYERS input_fw[NUMBER_OF_NEURONS];

	#if SHARED_MEM == 0
	  DTYPE_LAYERS input_bw[MAX_NUMBER_COLUMNS_TEST_SET][NUMBER_OF_NEURONS];

	#endif
	  const int max_col_test_set = MAX_NUMBER_COLUMNS_TEST_SET;

	  for(unsigned int  i = 0; i < numberOfColumns; i++)
		#pragma HLS LOOP_TRIPCOUNT min=1 max=max_col_test_set
		//#pragma HLS pipeline II=1
	    for(unsigned int  j = 0; j < NUMBER_OF_NEURONS; j++) {
	    input_bw[i][j] = input_bws.read();
	  }

	  // Compute the function of the output layer for each concatinated column
	  for(unsigned int col = 0; col < numberOfColumns; col++)
	  {
	  #pragma HLS LOOP_TRIPCOUNT min=1 max=max_col_test_set

	    // pre-calculating bw address for relaxing timing closure (sub-mul-fetch)
	    unsigned int input_bw_addr = (numberOfColumns - col - 1); //* NUMBER_OF_NEURONS;

	    for(unsigned int i = 0; i < NUMBER_OF_NEURONS; i++)
	    {
	      input_fw[i] = input_fws.read();
	      //input_bw[i] = input_bws.read();
	    }

	    sum = 0.0;
	    DTYPE_LAYERS *pInput_fw  = input_fw;// + col * NUMBER_OF_NEURONS;
	    //std::cout << "DEBUG: *pInput_bw:" << (numberOfColumns - col - 1) * NUMBER_OF_NEURONS << "\n";
	    DTYPE_LAYERS *pInput_bw  = input_bw[input_bw_addr];
	    //float *pOutput = output + col * NUMBER_OF_CLASSES;
	    DTYPE_OUTPUT pOutput[NUMBER_OF_CLASSES];
	    // Compute the function of each neuron of the output layer
	    //DTYPE_WEIGHTS_W2 *pW2 = W2_local; // FIXME: huge memory
	    //#pragma HLS RESOURCE variable=pW2 core=ROM_nP_BRAM


	    for(unsigned int cl = 0; cl < NUMBER_OF_CLASSES; cl++)
	    {
	    //#pragma HLS PIPELINE II=1 // -> splits huge memory to many huge memories, leading to insane bram utilization
#if DIMENSION_OF_WEIGHTS == 1
		  DTYPE_WEIGHTS_W2 *pW2 = W2 + cl * (NUMBER_OF_NEURONS * 2 + 1);
#else
	      DTYPE_WEIGHTS_W2 *pW2 = W2_2d[cl];
#endif
	      OutputLayerSinlgleNeuron(pW2, pInput_fw, pInput_bw, &pOutput[cl]);
	      // Softmax function
	      #if TRIGF_APPROX == 0
	      pOutput[cl] = (DTYPE_OUTPUT)expf((float)pOutput[cl]);
	      #elif TRIGF_APPROX == 1
	      pOutput[cl] = tiny_expf(pOutput[cl]);
	      #elif TRIGF_APPROX == 2
	      pOutput[cl] = expf_lookup(pOutput[cl]);//+expf_pwl(pOutput[cl]);
		    #elif TRIGF_APPROX == 3
	      pOutput[cl] = expf_pwl(pOutput[cl]);
	      #endif

	      //printf("pOutput[%u] = %f\n", cl, pOutput[cl]);

	      sum += pOutput[cl];
	    }
	    DTYPE_TRNLB tmpdiv;
	    for(unsigned int cl = 0; cl < NUMBER_OF_CLASSES; cl++) {
	    #pragma HLS PIPELINE II=1
	      if (sum != 0) // Fixed point seg.faluts when dividing by zero, i.e. bits shall allow any representation of accumulated sum
	    	  tmpdiv = pOutput[cl] / sum;
	      else
	    	  tmpdiv = pOutput[cl];
	      output.write(tmpdiv);

	/*
	      // DEBUGGING for host
	      union {
	          uint32_t t;
	          float f;
	          char b[sizeof(float)];
	      } u;
	        u.f = pOutput[cl];
	        printf("DEBUG host: output[%u]=%08x\n", col*NUMBER_OF_CLASSES + cl, (unsigned int)u.t);
	*/
	    }
	  }
	}


	// Reconstruct a line from the labels
	// OPS: 732x112x2=732*(5+10+7320*7) = 37518660
void TranslateBack(
					   unsigned int numberOfColumns, 	// IN  //
					   //float input[MAX_NUMBER_COLUMNS_TEST_SET * NUMBER_OF_CLASSES], // IN  // size: numberOfColumns * NUMBER_OF_CLASSES
					   hls::stream<DTYPE_TRNLB> &inputs,
					   uint8_t output_ind[MAX_PREDICTED_STRING_LENGTH],
					   uint8_t *str_len, // OUT //
					   //float *input,
					   DTYPE_TRNLB threshold  // IN  //
					  #if SHARED_MEM == 1
					  ,DTYPE_TRNLB *input)
					  #else
					  )
					  #endif
	{
		unsigned int left_limit = 0, right_limit;
		unsigned int offset, label;
		const int max_col_test_set = MAX_NUMBER_COLUMNS_TEST_SET;
		const int max_lat = MAX_NUMBER_COLUMNS_TEST_SET*NUMBER_OF_CLASSES;

		*str_len=0;

		#if SHARED_MEM == 0
		DTYPE_TRNLB input[MAX_NUMBER_COLUMNS_TEST_SET][NUMBER_OF_CLASSES];
		const int factor = NUMBER_OF_CLASSES;
		//#pragma HLS ARRAY_RESHAPE variable=input cyclic factor=factor dim=1
		#endif
		for(unsigned int col = 0; col < numberOfColumns; col++)
		#pragma HLS LOOP_TRIPCOUNT min=1 max=max_col_test_set
			for(unsigned int cl = 0; cl < NUMBER_OF_CLASSES; cl++) {
				input[col][cl] = inputs.read(); // read all steam but keep only columns for every 100 classes
		}

		for(unsigned int col = 0; col < numberOfColumns-1; col++) // FIXME: check algorithmic validity of -1
		{
			DTYPE_TRNLB rep1 = input[col][0];
			DTYPE_TRNLB rep2 = input[(col + 1)][0];

			#pragma HLS LOOP_TRIPCOUNT min=1 max=max_col_test_set
			if (rep1 > threshold && rep2 < threshold ) {
				left_limit = (col + 1) * NUMBER_OF_CLASSES;
			}
			else if (rep1 < threshold && rep2 > threshold )
			{
				right_limit = (col + 1) * NUMBER_OF_CLASSES;
				offset = max_element_syn_test(input, left_limit, right_limit);
				//std::cout << MAX_NUMBER_COLUMNS_TEST_SET * NUMBER_OF_CLASSES << " right_limit: " << right_limit
				//		<< "(" << right_limit/NUMBER_OF_CLASSES << ") left_limit: " << left_limit
				//		<< "(" << left_limit/NUMBER_OF_CLASSES << ") diff: " << right_limit-left_limit
				//		<< "(" << (right_limit-left_limit)/NUMBER_OF_CLASSES << ") offset " << offset << "\n";
				label = offset - (offset / NUMBER_OF_CLASSES) * NUMBER_OF_CLASSES;
				if (*str_len < MAX_PREDICTED_STRING_LENGTH) {
					output_ind[*str_len] = (uint8_t)label;
					*str_len = *str_len + 1;
				}
			}
		}
	}


#endif /* EMULATING_IO_SINGLE_KERNEL_BLSTM */

	double LevenshteinDistance(const std::string& s1, const std::string& s2)
	{
		const std::size_t len1 = s1.size(), len2 = s2.size();
		std::vector<unsigned int> col(len2+1), prevCol(len2+1);

		for (unsigned int i = 0; i < prevCol.size(); i++)
			prevCol[i] = i;
		for (unsigned int i = 0; i < len1; i++)
		{
			col[0] = i+1;
			for (unsigned int j = 0; j < len2; j++)
			{
				//col[j+1] = std::min({ prevCol[1 + j] + 1, col[j] + 1, prevCol[j] + (s1[i]==s2[j] ? 0 : 1) });

				col[j+1] = std::min(prevCol[1 + j] + 1, std::min(col[j] + 1, prevCol[j] + (s1[i]==s2[j] ? 0 : 1)));
			}

			col.swap(prevCol);
		}
		#if PROFILE
			std::cout << prevCol[len2] << std::endl;
		#endif

		return (double)prevCol[len2] / (double)s2.size();
	}

	double LevenshteinDistanceCStyle(const char *s1, const char *s2)
	{
	    unsigned int s1len, s2len, x, y, lastdiag, olddiag;
	    s1len = strlen(s1);
	    s2len = strlen(s2);
	    unsigned int column[s1len+1];

	    for (y = 1; y <= s1len; y++)
			column[y] = y;

	    for (x = 1; x <= s2len; x++)
	    {
			column[0] = x;

			for (y = 1, lastdiag = x-1; y <= s1len; y++)
			{
				olddiag = column[y];
				column[y] = MIN3(column[y] + 1, column[y-1] + 1, lastdiag + (s1[y-1] == s2[x-1] ? 0 : 1));
				lastdiag = olddiag;
			}
	    }

	    return (double)(column[s1len]) / (double)s1len;
	}




	//====================================================================================================================================================================================================================
	// Single Instance of BLSTM
	// OPS: 76366100x2 + 65367600 + 37518660 = 255618460
	//====================================================================================================================================================================================================================
	void Single_Kernel_BLSTM(
#ifdef INTERFACE_IS_STREAM
			hls::stream<DTYPE_IMG> &image_fw,
			hls::stream<DTYPE_IMG> &image_bw,
#else
			DTYPE_IMG image_fw[MAX_NUMBER_COLUMNS_TEST_SET * HIGHT_IN_PIX],
			DTYPE_IMG image_bw[MAX_NUMBER_COLUMNS_TEST_SET * HIGHT_IN_PIX],
#endif
			unsigned int numberOfColumns,
			uint8_t vecPredictedStringInd[MAX_PREDICTED_STRING_LENGTH],
			uint8_t *str_len) {

#pragma HLS DATAFLOW
#pragma HLS INLINE off

#ifdef EMULATING_IO_SINGLE_KERNEL_BLSTM

		DTYPE_IMG tmp = 0;
		/* Emulating reading all input  */
		for(unsigned int column = 0; column < numberOfColumns; column++) {
			#pragma HLS LOOP_TRIPCOUNT min=2 max=2
			for(unsigned int i = 0; i < HIGHT_IN_PIX; i++) {
				#ifdef INTERFACE_IS_STREAM
				tmp += image_fw.read() - image_bw.read();
				#else
				tmp += image_fw[column*HIGHT_IN_PIX+i] - image_bw[column*HIGHT_IN_PIX+i];
				#endif
			}

		}

		/* Emulating output data */
		for(unsigned int column = 0; column < MAX_PREDICTED_STRING_LENGTH; column++) {
			if (((int)tmp >= 0) && ((int)tmp < NUMBER_OF_CLASSES))
				vecPredictedStringInd[column] = (unsigned int)tmp;
			else
				vecPredictedStringInd[column] = MIN(column, NUMBER_OF_CLASSES-1);
		}

		/* Emulating founding a character on each column for all columns of inputs */
		*str_len = MAX_PREDICTED_STRING_LENGTH;

#else /* EMULATING_IO_SINGLE_KERNEL_BLSTM */


		hls::stream<DTYPE_LAYERS> pOutputFromtHiddenLayer_fw("pOutputFromtHiddenLayer_fw"); // MAX_NUMBER_COLUMNS_TEST_SET * NUMBER_OF_NEURONS
		hls::stream<DTYPE_LAYERS> pOutputFromtHiddenLayer_bw("pOutputFromtHiddenLayer_bw"); // MAX_NUMBER_COLUMNS_TEST_SET * NUMBER_OF_NEURONS
		hls::stream<DTYPE_TRNLB> poutputFromOutputLayer("poutputFromOutputLayer"); //MAX_NUMBER_COLUMNS_TEST_SET * NUMBER_OF_CLASSES

		const int stream_size = STREAM_KERNEL_SIZE_IN;
		#pragma HLS STREAM variable=pOutputFromtHiddenLayer_fw depth=stream_size dim=1
		#pragma HLS STREAM variable=pOutputFromtHiddenLayer_bw depth=stream_size dim=1

		const int stream_size_out = STREAM_KERNEL_SIZE_OUT;
		#pragma HLS STREAM variable=poutputFromOutputLayer depth=stream_size_out dim=1


//#pragma HLS array_map variable=expf_lut instance=W horizontal
//#pragma HLS array_map variable=divexpf_lut instance=W horizontal
//#pragma HLS array_map variable=tanh_lut instance=W horizontal

		//fp = fopen ("/tmp/dumpvars_1.txt", "w");
#pragma HLS allocation instances=Hidden_Layer_fw limit=2 function

#if SHARED_MEM == 1
		// Since DTYPE_LAYERS->ap_fixed<8,2> and DTYPE_TRNLB->ap_fixed<10,2>,
		// we choose the 'higher' format for their shared memory, i.e. DTYPE_TRNLB.
		DTYPE_TRNLB shared_mem[MAX_NUMBER_COLUMNS_TEST_SET*NUMBER_OF_CLASSES];
#endif /* SHARED_MEM == 1 */
		//std::cout << "DEBUG: Single_Kernel_BLSTM: numberOfColumns = " << numberOfColumns << "\n";
/**/	Hidden_Layer_fw(image_fw,
					 numberOfColumns,
					 pOutputFromtHiddenLayer_fw);

		// Backward direction
		Hidden_Layer_bw(image_bw,
					 numberOfColumns,
					 pOutputFromtHiddenLayer_bw);

		// CTC - Output Layer
		Output_Layer(numberOfColumns,
					 pOutputFromtHiddenLayer_fw,
					 pOutputFromtHiddenLayer_bw,
					 poutputFromOutputLayer
					 #if SHARED_MEM == 1
					 ,shared_mem);
					 #else
					 );
					 #endif

		// Return the predicted string
		TranslateBack(numberOfColumns,
				      poutputFromOutputLayer,
					  vecPredictedStringInd,
					  str_len,
					  0.7
					  #if SHARED_MEM == 1
					  ,shared_mem);
					  #else
					  );
					  #endif
/**/
#endif /* EMULATING_IO_SINGLE_KERNEL_BLSTM */
		//std::cout << "str_len = " << str_len << std::endl;
		//for(unsigned int i = 0; i < str_len; i++)
		//	std::cout << "vecPredictedStringInd[" << i << "] = " << vecPredictedStringInd[i] << std::endl;

		//return str_len;


	}


	//====================================================================================================================================================================================================================
	// Single Instance of BLSTM
	//====================================================================================================================================================================================================================
	void Single_Kernel_BLSTM_splited(
#ifdef INTERFACE_IS_STREAM
			hls::stream<DTYPE_IMG> &image_fw,
			hls::stream<DTYPE_IMG> &image_bw,
#else
			DTYPE_IMG image_fw[MAX_NUMBER_COLUMNS_TEST_SET * HIGHT_IN_PIX],
			DTYPE_IMG image_bw[MAX_NUMBER_COLUMNS_TEST_SET * HIGHT_IN_PIX],
#endif
			unsigned int numberOfColumns,
			uint8_t vecPredictedStringInd[MAX_NUMBER_COLUMNS_TEST_SET],
			uint8_t *str_len) {

				unsigned int numberOfColumnsRemain = numberOfColumns;
				uint8_t cur_str_len;
				*str_len = 0;

				for(unsigned int col = 0; col < numberOfColumns; col += COLS_PER_KERNEL_EXEC) {

					unsigned int curNumberOfColumns = MIN(numberOfColumnsRemain, COLS_PER_KERNEL_EXEC);

					//printf("DEBUG NEW: col=%u, curNumberOfColumns=%u, numberOfColumnsRemain=%u\n",
					//	col, curNumberOfColumns, numberOfColumnsRemain);
					#ifndef INTERFACE_IS_STREAM
					DTYPE_IMG *pimage_fw = image_fw + (col * HIGHT_IN_PIX);
					DTYPE_IMG *pimage_bw = image_bw + (numberOfColumnsRemain - curNumberOfColumns) * HIGHT_IN_PIX;
					#endif
					/* FIXME: verify that pimage_bw points exactly to correct location */
					Single_Kernel_BLSTM(
						#ifndef INTERFACE_IS_STREAM
						pimage_fw,
						pimage_bw,
						#else
						image_fw,
						image_bw,
						#endif
						curNumberOfColumns,
						vecPredictedStringInd+(*str_len),
						&cur_str_len);

						numberOfColumnsRemain -= curNumberOfColumns;
						*str_len = *str_len + cur_str_len;

						//printf("DEBUG NEW: cur_str_len=%u, *str_len=%u\n", cur_str_len, *str_len);
						//for(unsigned int i = 0; i<*str_len; i++)
						//	printf("vecPredictedStringInd[%u]=%u\n", i, vecPredictedStringInd[i]);

				}

			}


	//====================================================================================================================================================================================================================
	// Many Instances of BLSTM
	//====================================================================================================================================================================================================================
	void Many_Kernels_BLSTM(
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
#endif /* INTERFACE_IS_STREAM */
			short unsigned int numberOfColumnsVec[ACC_CALLS_PER_ACTION],
			uint8_t vecPredictedStringInd[ACC_CALLS_PER_ACTION][MAX_PREDICTED_STRING_LENGTH],
			uint8_t vecPredictedStringLen[ACC_CALLS_PER_ACTION]) {

#pragma HLS INLINE off
			const int stream_size = MAX_NUMBER_COLUMNS_TEST_SET * HIGHT_IN_PIX;
#ifdef INTERFACE_IS_STREAM
#pragma HLS STREAM variable=image_fw depth=stream_size dim=2
#pragma HLS STREAM variable=image_bw depth=stream_size dim=2
#endif /* INTERFACE_IS_STREAM */

//#pragma HLS ARRAY_PARTITION variable=image_fw complete dim=2
//#pragma HLS ARRAY_PARTITION variable=image_bw complete dim=2
#pragma HLS ARRAY_PARTITION variable=numberOfColumnsVec complete dim=1
			const int partition_factor = ACC_CALLS_PER_ACTION;
#pragma HLS ARRAY_PARTITION variable=vecPredictedStringInd block factor=partition_factor dim=1
#pragma HLS ARRAY_PARTITION variable=vecPredictedStringLen complete dim=1

		/* Check that hw physical accelerators are less or equal than single_kernel calls per action.
		 * This assertion makes sense only for synhtesis, not for simulation.
		 * */
		assert (HW_THREADS_PER_ACTION <= ACC_CALLS_PER_ACTION);

		for(unsigned int i = 0; i < ACC_CALLS_PER_ACTION; i++) {
		//#pragma HLS PIPELINE II=1
		#pragma HLS UNROLL
		const int limit = HW_THREADS_PER_ACTION;
		#pragma HLS allocation instances=Single_Kernel_BLSTM limit=limit function

			Single_Kernel_BLSTM(
				#ifdef MANY_STREAMS_FOR_MANY_ACCS
				image_fw[i],
				image_bw[i],
				#else /* MANY_STREAMS_FOR_MANY_ACCS */
				image_fw,
				image_bw,
				#endif /* MANY_STREAMS_FOR_MANY_ACCS */
				numberOfColumnsVec[i],
				vecPredictedStringInd[i],
				&vecPredictedStringLen[i]);
		}
	}


	//====================================================================================================================================================================================================================
	// AUXILIARY
	//====================================================================================================================================================================================================================

	bool directory_exists( const std::string &directory )
	{
	    if( !directory.empty() )
	    {
	        if( access(directory.c_str(), 0) == 0 )
	        {
	            struct stat status;
	            stat( directory.c_str(), &status );
	            if( status.st_mode & S_IFDIR )
	                return true;
	        }
	    }
	    // if any condition fails
	    return false;
	}


	std::vector<std::string> open(std::string path)
	{
		DIR*    dir;
		dirent* pdir;
		std::vector<std::string> files;

		std::cout << "Info: Opening path " << path << std::endl;

		if (!directory_exists(path)) {
			std::cout << "ERROR: Path " << path << " does not exist. Aborting" << std::endl;
			exit(-1);
		}

		dir = opendir(path.empty() ? "." : path.c_str());

		while ( (pdir = readdir(dir)) ) {
			if ((strncmp(pdir->d_name, "..", 2) !=0) && (strncmp(pdir->d_name, ".", 1) !=0)) {
				//std::cout << "pushed " << pdir->d_name << std::endl;
				files.push_back(pdir->d_name);
			}
		}

		closedir(dir);

		if ((files.end() - files.begin()) <= 0) {
			std::cout << "No input files provided in path " << path << " Aborting..." << std::endl;
			exit(-1);
		}

		std::sort(files.begin(), files.end());
		//files.erase(files.begin(), files.begin()+2);

		std::cout << "INFO: Read " << (files.end() - files.begin()) << " files from path " <<  path << std::endl;

		if ((files.end() - files.begin()) > MAX_NUMBER_IMAGES_TEST_SET) {
			std::cout << "WARNING: Max number of files reached (MAX_NUMBER_IMAGES_TEST_SET = " \
					  << MAX_NUMBER_IMAGES_TEST_SET << " )" << std::endl;
			files.erase((files.begin() + MAX_NUMBER_IMAGES_TEST_SET), files.end());
			std::cout << "INFO: Reduced to " << (files.end() - files.begin()) << " files from path " <<  path << std::endl;
		}

		return files;
	}


	unsigned int GetNumberOfThreads()
	{
		unsigned int numThreads;
		#ifdef NO_SYNTH

		#pragma omp parallel
		{
			//int ID = omp_get_thread_num();
			//std::cout << ID << std::endl;

			numThreads = omp_get_num_threads();

		}
		//std::cout << "Number of threads on the current machine: " << numThreads << std::endl;
		#endif
		return numThreads;
	}

#if DUMP_TRIG_VALUES == 1
static long get_nanos(void) {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (long)ts.tv_sec * 1000000000L + ts.tv_nsec;
}
#endif

#ifndef EMULATING_IO_SINGLE_KERNEL_BLSTM
	void dumpTrigonometricValues() {
	#define MAX_PLOT_TARGET 5
	#define MIN_PLOT_TARGET -5
	#define PRINT_ELEMENTS 1000
	#define TIME_ELEMENTS 100000
			FILE *fd = fopen("/tmp/values.txt", "w");
			assert (fd != NULL);

			const double step = (fabs(MIN_PLOT_TARGET)+fabs(MAX_PLOT_TARGET)) / (PRINT_ELEMENTS-1);
			#if 0
			fprintf(fd, "# \n");
			fprintf(fd, "# MAX_PLOT_TARGET = %lf\n", (double)MAX_PLOT_TARGET);
			fprintf(fd, "# MIN_PLOT_TARGET = %lf\n", (double)MIN_PLOT_TARGET);
			fprintf(fd, "# PRINT_ELEMENTS  = %u\n", (unsigned int)PRINT_ELEMENTS);
			fprintf(fd, "# PRINT_STEP  = lf\n", step);
			fprintf(fd, "# <i> <x> <y>\n\n");
			#endif
			double y1, y2, y3, y2e, y3e;
			long nanos_start, nanos_stop;

			for (unsigned int i = 0; i < PRINT_ELEMENTS; i++) {
				double x = MIN_PLOT_TARGET + (double)i * step;
				y1 = expf(x);
				y2 = (double)tiny_expf((float)x);

				y3 = (double)expf_lookup((float)x);
				y2e = fabs(y2 - y1);
				y3e = fabs(y3 - y1);
				fprintf(fd, "%u %lf %lf %lf %lf %lf %lf\n", i, x, y1, y2, y3, y2e, y3e);
			}
			#if 0
			fprintf(fd, "# Elapsed ns\t");
			nanos_start = get_nanos();
			for (unsigned int i = 0; i < TIME_ELEMENTS; i++)
				y1 = expf(0.5);
			nanos_stop = get_nanos();
			fprintf(fd, "%lu ", (nanos_stop-nanos_start)/TIME_ELEMENTS);

			nanos_start = get_nanos();
			for (unsigned int i = 0; i < TIME_ELEMENTS; i++)
				y2 = tiny_expf(0.5);
			nanos_stop = get_nanos();
			fprintf(fd, "%lu ", (nanos_stop-nanos_start)/TIME_ELEMENTS);

			nanos_start = get_nanos();
			for (unsigned int i = 0; i < TIME_ELEMENTS; i++)
				y3 = expf_lookup(0.5);
			nanos_stop = get_nanos();
			fprintf(fd, "%lu ", (nanos_stop-nanos_start)/TIME_ELEMENTS);

			fprintf(fd, "\n");
			#endif
			fclose(fd);
			exit(0);
}
#endif

	void print_usage_and_exit(const char *argv0)
	{
		std::cerr << "Usage: " << argv0 << " <parallelization> <path_data> <path_gt>" << std::endl;
		std::cerr << "    <parallelization>    : -s, -m" << std::endl;
		std::cerr << "    <path_data>          : ../../samples/" << std::endl;
		std::cerr << "    <path_gt>            : ../gt/" << std::endl;


		exit(1);
	}
