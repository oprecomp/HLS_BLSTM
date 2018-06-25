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
 * @file neuron.hpp
 * @author Dionysios Diamantopoulos, did@zurich.ibm.com, FPGA porting
 * @author Vladimir Rybalkin, rybalkin@eit.uni-kl.de, BLSTM code
 * @date 20 Feb 2017
 * @brief The header file of neuron.cpp file.
 * */


#ifndef NEURON_HPP
#define NEURON_HPP

#include <cstddef>
#include <string>       // std::string
#include <iostream>     // std::cout, std::cerr
#ifdef NO_SYNTH
#include <fstream>      // std::ifstream std::ofstream
#endif
#include <vector>
#include <math.h>       // tanh, log
#include <dirent.h>
#include <sys/types.h>
#include <algorithm>	// std::sort
#include <ctype.h>		// isspace()
#include <assert.h>
#ifndef FORMAT_IS_FLOAT
#include <ap_fixed.h>
#endif /* FORMAT_IS_FLOAT */
#include <cstring>		// std::memcpy()
#include <float.h>		// FLT_MAX etc.
#include <unistd.h>		// gethostname()
#ifdef NO_SYNTH
#include <omp.h>      // openmp
#endif
#include <hls_stream.h> // hls_stream
#include <hls_half.h>
#include <stdint.h>		// uint<>_t
#include "generate_luts.h" // LUTs size, limits etc.

#include "../include/common_def.h"


#define MIN3(a, b, c) ((a) < (b) ? ((a) < (c) ? (a) : (c)) : ((b) < (c) ? (b) : (c)))

/* Take care of min/max functions for ISO C programs - especially when it comes to FPGA world! */
#ifndef MAX
#define MAX(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })
#endif
#ifndef MIN
#define MIN(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })
#endif


#define CEILING_POS(X) ((X-(int)(X)) > 0 ? (int)(X+1) : (int)(X))
#define CEILING_NEG(X) ((X-(int)(X)) < 0 ? (int)(X-1) : (int)(X))
#define CEILING(X) ( ((X) > 0) ? CEILING_POS(X) : CEILING_NEG(X) )


//#ifdef NO_SYNTH

	//====================================================================================================================================================================================================================
	// NEURAL NETWORK - MODEL
	//====================================================================================================================================================================================================================

	class NeuralNetwork
	{
		public:

		NeuralNetwork();
		~NeuralNetwork();

		void Init(std::string inputFileModel);

		// NUMBER_OF_NEURONS * NUMBER_OF_INPUTS
		DTYPE *WGI;// Input gate
		DTYPE *WGF;// Forget gate
		DTYPE *WGO;// Output gate
		DTYPE *WCI;// Input to Memory Cell

		// NUMBER_OF_NEURONS
		DTYPE *WIP;// Input gate
		DTYPE *WFP;// Forget gate
		DTYPE *WOP;// Output gate

		// NUMBER_OF_CLASSES * (1 + NUMBER_OF_NEURONS * 2)
		DTYPE *W2; //Output layer

		protected:

		private:
	};




	//====================================================================================================================================================================================================================
	// ALPHABET
	//====================================================================================================================================================================================================================

	class Alphabet
	{
		public:

		Alphabet();
		~Alphabet();

		// NUMBER_OF_CLASSES
		std::vector<std::string> alphabet;
		//char alphabet[500];

		unsigned int size;
		void Init(std::string inputFileAlphabet);

		std::string ReturnSymbol(unsigned int lable);
		//char ReturnSymbol(unsigned int lable);

		void Print();

		protected:

		private:
	};




	//====================================================================================================================================================================================================================
	// INPUT IMAGE
	//====================================================================================================================================================================================================================

	class InputImage
	{
		public:

		InputImage();
		~InputImage();

		// NumberOfColumns * HIGHT_IN_PIX
		float *image_fw;
		float *image_bw;

		unsigned int numberOfColumns;

		void Init(std::string inputFileImage);

		void Print();

		void Free();

		protected:

		private:
	};




	//====================================================================================================================================================================================================================
	// GROUND TRUTH
	//====================================================================================================================================================================================================================

	class GroundTruth
	{
		public:

		GroundTruth();
		~GroundTruth();

		// NUMBER_OF_CLASSES
		std::string groundTruth;

		void Init(std::string inputFileGroundTruth);

		std::string ReturnString();

		void Free();

		protected:

		private:
	};

//#endif


	  /**
	   *  @brief  Return the maximum element in a range.
	   *  @ingroup sorting_algorithms
	   *  @param  first  Start of range.
	   *  @param  last   End of range.
	   *  @return  Iterator referencing the first instance of the largest value.
	  */
	  template<typename _ForwardIterator>
	    _ForwardIterator
	    max_element_syn(_ForwardIterator __first, _ForwardIterator __last)
	    {
		#pragma HLS INLINE
	      // concept requirements
	      __glibcxx_function_requires(_ForwardIteratorConcept<_ForwardIterator>)
	      __glibcxx_function_requires(_LessThanComparableConcept<
		    typename iterator_traits<_ForwardIterator>::value_type>)
	      __glibcxx_requires_valid_range(__first, __last);
		  const int min_tripcount = NUMBER_OF_CLASSES;
		  const int max_tripcount = MAX_NUMBER_COLUMNS_TEST_SET * NUMBER_OF_CLASSES;

	      if (__first == __last)
		return __first;
	      _ForwardIterator __result = __first;
	      while (++__first != __last) {
		  	  #pragma HLS LOOP_TRIPCOUNT min=min_tripcount max=max_tripcount
	    	  if (*__result < *__first)
	    		  __result = __first;
	      }
	      return __result;
	    }



	//====================================================================================================================================================================================================================
	// LSTM
	//====================================================================================================================================================================================================================

	// The dot product corresponding to a single gate of the LSTM memory cell
	inline float DotVectorToVector126(//float *source, 	// IN  // size: 1.0 + HIGHT_IN_PIX + NUMBER_OF_NEURONS = NUMBER_OF_INPUTS
									  hls::stream<DTYPE_IMG> &source,
									  DTYPE *weights);	// IN  // size: NUMBER_OF_INPUTS

	// The function of a single LSTM memory cell
	void HiddenLayerSingleMemoryCell(DTYPE_IMG source[NUMBER_OF_INPUTS],					// IN  // size: 1.0 + HIGHT_IN_PIX + NUMBER_OF_NEURONS = NUMBER_OF_INPUTS
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
									 DTYPE_IN *output);              	// OUT // A single output


	void Hidden_Layer_fw(
#ifdef INTERFACE_IS_STREAM
					  hls::stream<DTYPE_IMG> &image, // IN  // size: numberOfColumns * HIGHT_IN_PIX
#else
					  DTYPE_IMG *image, // IN // [MAX_NUMBER_COLUMNS_TEST_SET * HIGHT_IN_PIX],
#endif
					  unsigned int numberOfColumns,			// IN  //
					  hls::stream<DTYPE_LAYERS> &result);	// OUT // size: numberOfColumns * NUMBER_OF_NEURONS


	//====================================================================================================================================================================================================================
	// Connectionist Temporal Classification Layer (CTC layer)
	//====================================================================================================================================================================================================================

	// The dot product corresponding to a single neuron of the output layer operating on an concatinated output from the forward and the bakward hidden layers
	inline DTYPE_OUTPUT DotVectorToVector201(DTYPE *W2,		// IN  // size: NUMBER_OF_NEURONS * 2 + 1
									                         DTYPE_LAYERS *input_fw, 	// IN  // size: NUMBER_OF_NEURONS
							                             DTYPE_LAYERS *input_bw);	// IN  // size: NUMBER_OF_NEURONS

	// The function of a single neuron of the output layer
	inline void OutputLayerSinlgleNeuron(DTYPE_WEIGHTS_W2 *W2, 		// IN  // size: NUMBER_OF_NEURONS * 2 + 1
										 float *input_fw,	// IN  // size: NUMBER_OF_NEURONS
										 float *input_bw, 	// IN  // size: NUMBER_OF_NEURONS
										 float *output);	// OUT //

	void Output_Layer(unsigned int numberOfColumns, // IN  //
					  //float *input_fw,			 	// IN  // size: numberOfColumns * NUMBER_OF_NEURONS
					  //float *input_bw,			 	// IN  // size: numberOfColumns * NUMBER_OF_NEURONS
					  hls::stream<float> &input_fws,
					  hls::stream<float> &input_bws,
					  DTYPE_TRNLB *output); 				// OUT // size: numberOfColumns * NUMBER_OF_CLASSE

	// Reconstruct a line from the labels
	//void TranslateBack(char alphabet[500], unsigned int numberOfColumns, float *input, std::string &output, float threshold = 0.7);
	//void TranslateBack(Alphabet &alphabet, unsigned int numberOfColumns, float *input, std::string &output, float threshold = 0.7);
	void TranslateBack( unsigned int numberOfColumns,
								//float *input,
								hls::stream<DTYPE_TRNLB> &inputs,
								uint8_t output_ind[MAX_PREDICTED_STRING_LENGTH],
								uint8_t *str_len,
								DTYPE_TRNLB threshold = 0.7);

	double LevenshteinDistance(const std::string& s1, const std::string& s2);
	double LevenshteinDistanceCStyle(const char *s1, const char *s2);



	//====================================================================================================================================================================================================================
	// Single Instance of BLSTM
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
			uint8_t *str_len
	);


  //====================================================================================================================================================================================================================
	// Single Instance of BLSTM with splitted image feeding
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
			uint8_t *str_len);

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
			uint8_t vecPredictedStringLen[ACC_CALLS_PER_ACTION]);

	//====================================================================================================================================================================================================================
	// AUXILIARY
	//====================================================================================================================================================================================================================

	std::vector<std::string> open(std::string path);

  void dumpTrigonometricValues();

	unsigned int GetNumberOfThreads();

	void print_usage_and_exit(const char *argv0);


#endif
