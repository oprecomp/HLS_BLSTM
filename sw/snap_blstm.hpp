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
 * @file snap_blstm.hpp
 * @author Dionysios Diamantopoulos, did@zurich.ibm.com, FPGA and PULP porting
 * @author Vladimir Rybalkin, rybalkin@eit.uni-kl.de, BLSTM code
 * @date 10 Dec 2017
 * @brief Header file of  snap_blstm.cpp and definitions from ../hw/newron.cpp :
 * keeping only stuff for host code, avoiding VHLS-related.
 */



#define CEILING_POS(X) ((X-(int)(X)) > 0 ? (int)(X+1) : (int)(X))
#define CEILING_NEG(X) ((X-(int)(X)) < 0 ? (int)(X-1) : (int)(X))
#define CEILING(X) ( ((X) > 0) ? CEILING_POS(X) : CEILING_NEG(X) )

#define MIN3(a, b, c) ((a) < (b) ? ((a) < (c) ? (a) : (c)) : ((b) < (c) ? (b) : (c)))


/* Function prototypes */
static inline ssize_t file_write(const char *fname_out, const char *fname_in, const uint32_t *buff, size_t len);
std::vector<std::string> open(std::string path);


	//=================================================================================================================
	// ALPHABET
	//=================================================================================================================

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




	//=================================================================================================================
	// INPUT IMAGE
	//=================================================================================================================

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




	//=================================================================================================================
	// GROUND TRUTH
	//=================================================================================================================

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



	double LevenshteinDistance(const std::string& s1, const std::string& s2);
	double LevenshteinDistanceCStyle(const char *s1, const char *s2);

  class mystreambuf: public std::streambuf
  {
  };
  mystreambuf nostreambuf;
  std::ostream nocout(&nostreambuf);
  #define log(x) ((x <= DEBUG_LEVEL)? std::cout : nocout)
