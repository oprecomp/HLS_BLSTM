/*
 * Copyright 2017 International Business Machines
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * SNAP HLS_BLSTM APPLICATION
 *
 */

#ifndef NO_SYNTH
//#define NO_SYNTH
#endif

#ifdef NO_SYNTH

#include <cstddef>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include "ap_int.h"
#include "action_hls.H"
#include "neuron.hpp"

#include <sstream>

#define MAX_PIXELS_PER_IMAGE 2 * MAX_NUMBER_COLUMNS_TEST_SET * HIGHT_IN_PIX

/* helper functions for tb */
static inline void tbhexdump(FILE *fp, const void *buff, unsigned int size);


//-----------------------------------------------------------------------------
//--- TESTBENCH ---------------------------------------------------------------
//-----------------------------------------------------------------------------

int main(void)
{

		//dumpTrigonometricValues();

		omp_set_num_threads(GetNumberOfThreads());

		unsigned int numThreads = GetNumberOfThreads();
		std::cout << "Number of threads on the current machine: " << numThreads << std::endl;
		char hostname[256];
		gethostname(hostname, 256);
		std::cout << "Host name: " << hostname << std::endl;

		const int depth_in =  CEILING_POS((float)(ACC_CALLS_PER_ACTION * MAX_PIXELS_PER_IMAGE*sizeof(float))/sizeof(snap_membus_t));
		const int depth_out = CEILING_POS((float)(ACC_CALLS_PER_ACTION * MAX_NUMBER_COLUMNS_TEST_SET*sizeof(uint32_t))/sizeof(snap_membus_t));

    unsigned int total_pixels_in_action = 0;

    static snap_membus_t din_gmem[depth_in];  // 7 for 1, 2288 = for 732x25 x2 images bw fw
    static snap_membus_t dout_gmem[depth_out]; // 1

    action_reg act_reg;
    action_RO_config_reg Action_Config;

    /* Query ACTION_TYPE ... */
    act_reg.Control.flags = 0x0;
#ifndef VHLS_TB
    std::cout << "Discovery : calling action to get config data\n";
    hls_action(din_gmem, dout_gmem, &act_reg, &Action_Config);
    fprintf(stdout,
	    "ACTION_TYPE:   %08x\n"
	    "RELEASE_LEVEL: %08x\n"
	    "RETC:          %04x\n",
	    (unsigned int)Action_Config.action_type,
	    (unsigned int)Action_Config.release_level,
	    (unsigned int)act_reg.Control.Retc);
#endif

    memset(din_gmem,  'c', sizeof(din_gmem[0]));
    std::cout << "DEBUG: sizeof(din_gmem[0]) = " << sizeof(din_gmem[0]) << " bytes\n";

	//----------------------------------------------------------------------
	// Init main data structures
	//----------------------------------------------------------------------

	// Source directories for images and ground truth files
	std::string inputFileImageDir = "/tools/projects/snap/actions/hls_blstm/data/samples_c/";
	//std::string inputFileImageDir = "/tools/projects/oprecomp_local/mb/data/prepared/mb/blstm/fraktur_dataset/samples/";
	std::string inputFileGroundTruthDir = "/tools/projects/snap/actions/hls_blstm/data/gt_1/";
	//std::string inputFileGroundTruthDir = "/tools/projects/oprecomp_local/mb/data/prepared/mb/blstm/gt/";
	// The initialization of the alphabet
	Alphabet alphabet;
	alphabet.Init("/tools/projects/snap/actions/hls_blstm/data/alphabet/alphabet.txt");
	//alphabet.Print();
	// Return the list of images' file names
	std::vector<std::string> listOfImages = open(inputFileImageDir);
	unsigned int imgs = listOfImages.size();
	// Return the list of ground truth' file names
	std::vector<std::string> listOfGroundTruth = open(inputFileGroundTruthDir);
	unsigned int imgs_gd = listOfImages.size();

	/* Check that there are equal number of image files and groundtruth files */
	assert(imgs == imgs_gd);

	//----------------------------------------------------------------------
	// Read Images
	//----------------------------------------------------------------------

	std::vector<InputImage> vecInputImage;
	vecInputImage.resize(listOfImages.size());

	#pragma omp parallel
	#pragma omp for schedule(dynamic)
	for(unsigned int i = 0; i < listOfImages.size(); i++)
	{
		std::string inputFileImage = inputFileImageDir + listOfImages.at(i);

		vecInputImage.at(i).Init(inputFileImage);
	}

	std::cout << "DEBUG: listOfImages.size() = " << listOfImages.size() << "\n";

	//----------------------------------------------------------------------
	// Read Ground Truth
	//----------------------------------------------------------------------

	std::vector<GroundTruth> vecGroundTruth;
	vecGroundTruth.resize(listOfImages.size());

	#pragma omp parallel
	#pragma omp for schedule(dynamic)
	for(unsigned int i = 0; i < listOfImages.size(); i++)
	{
		std::string inputFileGroundTruth = inputFileGroundTruthDir + listOfGroundTruth.at(i);

		vecGroundTruth.at(i).Init(inputFileGroundTruth);
	}

	//----------------------------------------------------------------------
	// Predicted string
	//----------------------------------------------------------------------

	std::vector<std::string> vecPredictedString;
	vecPredictedString.resize(listOfImages.size());

	//----------------------------------------------------------------------
	// Allocation of the resources
	//----------------------------------------------------------------------

	// Predicted Length of String
	//unsigned int *vecPredictedStringLen = new unsigned int [listOfImages.size()];
	// Predicted index of alphabet for every string: 2D array, one row for every image/string,
	// several columns for predicted characters
	//unsigned int **vecPredictedStringInd = new unsigned int *[listOfImages.size()];
	//for(unsigned int i = 0; i < vecInputImage.size(); i++)
	//	vecPredictedStringInd[i] = new unsigned int [MAX_NUMBER_COLUMNS_TEST_SET];

	// Verify that we hold enough space for the loaded images of folder provided
	assert(listOfImages.size() <= MAX_NUMBER_IMAGES_TEST_SET);

	uint8_t *vecPredictedStringLen = new uint8_t [MAX_NUMBER_IMAGES_TEST_SET];
	uint8_t **vecPredictedStringInd = new uint8_t *[MAX_NUMBER_IMAGES_TEST_SET];
	for(unsigned int i = 0; i < MAX_NUMBER_IMAGES_TEST_SET; i++)
		vecPredictedStringInd[i] = new uint8_t [MAX_PREDICTED_STRING_LENGTH];

	double *error = new double [listOfImages.size()];
	double *accuracy_vec = new double [listOfImages.size()];
	double errorSum = 0.0;
	double accuracy = 0.0;


	//====================================================================================================================================================================================================================
	// START
	//====================================================================================================================================================================================================================

	std::cout << "Start ..." << std::endl;

	// Starting point - the first time stemp
	time_t t1 = time(0);

	short unsigned int *numberOfColumnsVec = new short unsigned int [MAX_NUMBER_IMAGES_TEST_SET];


#ifdef DATA_MOVE_OVER_AXI


	ssize_t sizef;
	float *ibuff = NULL;
	uint32_t *obuff = NULL;

	sizef = ACC_CALLS_PER_ACTION * MAX_PIXELS_PER_IMAGE * sizeof(float); // for fw and bw

	/* Allocating input/output buffers for a single action */
	ibuff = (float *)malloc(sizef);
	if (ibuff == NULL) {
			std::cout << "Error on allocating ibuf. Aborting...\n";
			return -1;
	}
	obuff = (uint32_t *)malloc(ACC_CALLS_PER_ACTION * MAX_NUMBER_COLUMNS_TEST_SET * sizeof(uint32_t));
	if (obuff == NULL) {
			std::cout << "Error on allocating obuf. Aborting...\n";
			return -1;
	}

	/* check that there are enough MMIO register entries to hold the columns of every image */
	assert(ACC_CALLS_PER_ACTION <= sizeof(act_reg.Data.imgcols)/sizeof(act_reg.Data.imgcols.cols[0]));


	/* check that inputs are multiples of what accelerator is waiting for every iteration */
	assert (ceil((float)listOfImages.size() / ACC_CALLS_PER_ACTION) == floor((float)listOfImages.size() / ACC_CALLS_PER_ACTION));
	/* previous assertion verifies that action_loops is integer FIXME: should update it to any case of reminder */

	for(unsigned int i = 0; i < vecInputImage.size(); i += ACC_CALLS_PER_ACTION) {

		/* Write on MMIO register the number of columns of current image */
	    memset(act_reg.Data.imgcols.cols, 0, sizeof(act_reg.Data.imgcols));

		/* Write on MMIO register the number of columns of current image */
	    memset(act_reg.Data.imgstrlen.cols, 0, sizeof(act_reg.Data.imgstrlen));

    	total_pixels_in_action = 0;

	    for (unsigned int j = 0; j < ACC_CALLS_PER_ACTION; j++) {
				/* Keep up to MAX_NUMBER_COLUMNS_TEST_SET columns */
	    	numberOfColumnsVec[i+j] = vecInputImage.at(i+j).numberOfColumns;
				//numberOfColumnsVec[i+j] = MIN(vecInputImage.at(i+j).numberOfColumns, MAX_NUMBER_COLUMNS_TEST_SET);

	    	act_reg.Data.imgcols.cols[j] = numberOfColumnsVec[i+j];
	    	std::cout << "DEBUGGG: numberOfColumnsVec[" << i+j << "] = " << numberOfColumnsVec[i+j] << ", total_pixels_in_action = " <<  total_pixels_in_action << std::endl;

	    	/* copy actual fw/bw data of every image */
			for(unsigned int k = 0; k <  numberOfColumnsVec[i+j] * HIGHT_IN_PIX; k++) {
				ibuff[total_pixels_in_action + k] = vecInputImage.at(i+j).image_fw[k];
				//std::cout << "DEBUG: fw index = " << total_pixels + j << ", vecInputImage.at(" << i << ").image_fw[" << j << "]=" << \
						     vecInputImage.at(i).image_fw[j] << "\n";
			}
			for(unsigned int k = 0 ; k < numberOfColumnsVec[i+j] * HIGHT_IN_PIX; k++) {
				ibuff[total_pixels_in_action + numberOfColumnsVec[i+j] * HIGHT_IN_PIX + k] = vecInputImage.at(i+j).image_bw[k];
				//std::cout << "DEBUG: bw index = " << total_pixels + numberOfColumnsVec[i] * HIGHT_IN_PIX + j << ", vecInputImage.at(" << i << ").image_bw[" << j << "]=" << \
							 vecInputImage.at(i).image_bw[j] << "\n";
			}
			/* Update the number of pixels */
	    	total_pixels_in_action += 2 * numberOfColumnsVec[i+j] * HIGHT_IN_PIX;
			std::cout << "INFO: numberOfColumnsVec[" << i+j << "] = " <<  numberOfColumnsVec[i+j] << std::endl;
	    }

		/*
		  std::cout << "Input is : " << (char *)((unsigned long)din_gmem + 0) << "\n";
		  std::cout << "Input is : \n";
		  for (unsigned int i = 0; i < total_pixels; i++) {
		  	std::cout << i << ":" <<  ibuff[i] << "\n";
		  }
		 */

		act_reg.Control.flags = 0x1; /* just not 0x0 */

		act_reg.Data.in.addr = 0;
		act_reg.Data.in.size = total_pixels_in_action*sizeof(float);
		act_reg.Data.in.type = SNAP_ADDRTYPE_HOST_DRAM;

		act_reg.Data.out.addr = 0;
		act_reg.Data.out.size = MAX_NUMBER_COLUMNS_TEST_SET*sizeof(unsigned int);
		act_reg.Data.out.type = SNAP_ADDRTYPE_HOST_DRAM;

		memcpy(din_gmem, ibuff, total_pixels_in_action*sizeof(float));

		tbhexdump(stdout, &act_reg.Data, sizeof(act_reg.Data));

		std::cout << "Action call \n";
		std::cout << "act_reg.Data.imgcols.cols[0] = " << act_reg.Data.imgcols.cols[0] << std::endl;
		hls_action(din_gmem, dout_gmem, &act_reg, &Action_Config);

		tbhexdump(stdout, &act_reg.Data, sizeof(act_reg.Data));

		memcpy(obuff, dout_gmem, ACC_CALLS_PER_ACTION * MAX_NUMBER_COLUMNS_TEST_SET*sizeof(uint32_t));
		unsigned int str_addr_index = 0;
		for (unsigned int j = 0; j < ACC_CALLS_PER_ACTION; j++) {
#ifdef VHLS_TB
			vecPredictedStringLen[i+j] = numberOfColumnsVec[i+j]; // emulate finding a single character per column
#else
			vecPredictedStringLen[i+j] = act_reg.Data.imgstrlen.cols[j];
#endif
			//std::cout << "DEBUG tb: vecPredictedStringLen[" << i+j << "] = " << act_reg.Data.imgstrlen.cols[j] << std::endl;
			for (unsigned int l = 0; l < vecPredictedStringLen[i+j]; l++) {
				vecPredictedStringInd[i+j][l] = obuff[str_addr_index++];
				//std::cout << "DEBUG tb: vecPredictedStringInd[" << i+j << "][" << l <<\
						"] = obuff["<< str_addr_index << "] = " << obuff[str_addr_index] << std::endl;
			}
		}
	} /* for list of images += ACC_CALLS_PER_ACTION */

    free(ibuff);
    free(obuff);

#else /* DATA_MOVE_OVER_AXI */

#ifdef MANY_STREAMS_FOR_MANY_ACCS
	#ifdef INTERFACE_IS_STREAM
		hls::stream<DTYPE_IMG> image_fw[MAX_NUMBER_IMAGES_TEST_SET];
		hls::stream<DTYPE_IMG> image_bw[MAX_NUMBER_IMAGES_TEST_SET];
	#else
		DTYPE_IMG **image_fw = (DTYPE_IMG**)malloc(listOfImages.size() * sizeof(DTYPE_IMG*));
		assert(image_fw != NULL);
		DTYPE_IMG **image_bw = (DTYPE_IMG**)malloc(listOfImages.size() * sizeof(DTYPE_IMG*));
		assert(image_bw != NULL);
	#endif
#else /* MANY_STREAMS_FOR_MANY_ACCS */
	hls::stream<DTYPE_IMG> image_fw;
	hls::stream<DTYPE_IMG> image_bw;
#endif /* MANY_STREAMS_FOR_MANY_ACCS */

	#pragma omp parallel
	#pragma omp for schedule(dynamic)
	for(unsigned int i = 0; i < listOfImages.size(); i += ACC_CALLS_PER_ACTION) {

		unsigned int threadID = omp_get_thread_num();
		std::cout << "Thread " <<  threadID << " working on image(s): " << i \
							<< " to " << i + ACC_CALLS_PER_ACTION -1 << std::endl;

		for (unsigned int j = 0; j < ACC_CALLS_PER_ACTION; j++) {
			numberOfColumnsVec[i+j] = vecInputImage.at(i+j).numberOfColumns;
			#ifndef INTERFACE_IS_STREAM
			image_fw[i+j] = (DTYPE_IMG*)malloc((numberOfColumnsVec[i+j] * HIGHT_IN_PIX) * sizeof(DTYPE_IMG));
			assert(image_fw[i+j] != NULL);
			image_bw[i+j] = (DTYPE_IMG*)malloc((numberOfColumnsVec[i+j] * HIGHT_IN_PIX) * sizeof(DTYPE_IMG));
			assert(image_bw[i+j] != NULL);
			#endif
			for(unsigned int k = 0; k < numberOfColumnsVec[i+j] * HIGHT_IN_PIX; k++) {
				#ifdef MANY_STREAMS_FOR_MANY_ACCS
				#ifdef INTERFACE_IS_STREAM
				image_fw[i+j].write((DTYPE_IMG)vecInputImage.at(i+j).image_fw[k]);
				image_bw[i+j].write((DTYPE_IMG)vecInputImage.at(i+j).image_bw[k]);
				#else /*INTERFACE_IS_STREAM*/
				image_fw[i+j][k] = (DTYPE_IMG)vecInputImage.at(i+j).image_fw[k];
				image_bw[i+j][k] = (DTYPE_IMG)vecInputImage.at(i+j).image_bw[k];
				#endif /* INTERFACE_IS_STREAM */
				#else /* MANY_STREAMS_FOR_MANY_ACCS */
				image_fw.write((DTYPE_IMG)vecInputImage.at(i).image_fw[k]);
				image_bw.write((DTYPE_IMG)vecInputImage.at(i).image_bw[k]);
				#endif /* MANY_STREAMS_FOR_MANY_ACCS */
			}
    	Single_Kernel_BLSTM_splited(
			 image_fw[i+j],
			 image_bw[i+j],
			 numberOfColumnsVec[i+j],
			 vecPredictedStringInd[i+j],
			 &vecPredictedStringLen[i+j]);

			#ifndef INTERFACE_IS_STREAM
			free(image_fw[i+j]);
	 		free(image_bw[i+j]);
			#endif
		}
	}

	#ifndef INTERFACE_IS_STREAM
	free(image_fw);
	free(image_bw);
	#endif

#endif /* DATA_MOVE_OVER_AXI */



	//std::cout << "Output is : \n";
	//for (unsigned int i = 0; i < 75; i++) {
	//	std::cout << i << ":" << obuff[i] << "\n";
	//}
	//std::cout << "\n";

	// Do the translation from alphabet indexers to actual characters
	// Since some special characters reserve 2-3 char positions, we do the translation
	// to the SW, using string vectors, i.e. dynamic alloc, (avoiding 2D buffers on HW)
	for(unsigned int i = 0; i < listOfImages.size(); i++) {
		for(unsigned int j = 0; j < vecPredictedStringLen[i]; j++) {
			std::string tmpSymbol = alphabet.ReturnSymbol(vecPredictedStringInd[i][j]);
			vecPredictedString.at(i).insert(vecPredictedString.at(i).end(), tmpSymbol.begin(), tmpSymbol.end() );
		}
	}

	// Ending point - the final time stemp
	//auto t2 = std::chrono::high_resolution_clock::now();
	time_t t2 = time(0);
	//====================================================================================================================================================================================================================
	// FINISH
	//====================================================================================================================================================================================================================


	//FILE *fd = NULL;
	//fd = fopen("/tmp/error_values.txt", "w");

	for(unsigned int i = 0; i < listOfImages.size(); i++) {

		//----------------------------------------------------------------------
		// Calculate Levenshtein Distance for each string and output result
		//----------------------------------------------------------------------
		std::string groundTruthstring = vecGroundTruth.at(i).ReturnString();
		error[i] = LevenshteinDistance(vecPredictedString.at(i), groundTruthstring);
		accuracy_vec[i] = (1-error[i])*100;
		std::cout << i << " Expected: "<< groundTruthstring \
				  << "\n Predicted: " << vecPredictedString.at(i) << " Accuracy: " << (1-error[i])*100 << " %\n";

		//fprintf(fd, "%u\t%f\n", i, (1-error[i])*100);

		std::cout << " Predicted id: ";
		for(unsigned int j = 0; j < vecPredictedStringLen[i]; j++)
			std::cout << (unsigned int)vecPredictedStringInd[i][j] << " ";
		std::cout << std::endl;

		#ifdef DATA_MOVE_OVER_AXI
		std::cout << "INFO: Accelerator status code on MMIO : " \
				  << act_reg.Data.status << " (0x" << std::hex << act_reg.Data.status << ")" << std::dec << std::endl;

		std::cout << "INFO: AXI transaction registered on MMIO : \nINFO: In: " \
				  << act_reg.Data.axitrans_in  << " (0x" << std::hex << act_reg.Data.axitrans_in  << ")\nINFO: Out:" << std::dec \
				  << act_reg.Data.axitrans_out << " (0x" << std::hex << act_reg.Data.axitrans_out << ")" << std::dec << std::endl;

    	//for (unsigned int j = 0; j < sizeof(act_reg.Data.imgcols.cols)/sizeof(uint16_t); j++)
    	//	std::cout << "act_reg->Data.imgcols.cols[" << j << "] = " << act_reg.Data.imgcols.cols[j] << std::endl;
		#endif /* DATA_MOVE_OVER_AXI */

		#if PROFILE
			std::cout << vecPredictedString.at(i) << std::endl;
			std::cout << groundTruthstring << std::endl << std::endl;
		#endif

		vecPredictedString.at(i).clear();
		groundTruthstring.clear();
		//groundTruth.Free();
	}

	//if (fd == NULL)
	//	fclose(fd);

	for(unsigned int e = 0; e < listOfImages.size(); e++)
		errorSum += error[e];

	accuracy = (1.0 - errorSum / (float)listOfImages.size()) * 100.0;
	float min_accuracy = *std::min_element(accuracy_vec,accuracy_vec+listOfImages.size());
	float max_accuracy = *std::max_element(accuracy_vec,accuracy_vec+listOfImages.size());

	std::cout << "Accuracy(min,max,avg): " << min_accuracy << " " << max_accuracy << " " << accuracy << "%" <<std::endl;

	errorSum = 0.0;

	//std::chrono::seconds time_span = std::chrono::duration_cast<std::chrono::seconds>(t2-t1);
	double time_span = difftime( t2, t1);

	std::cout << "Measured time ... " << time_span << " seconds" << std::endl << std::endl;

	//----------------------------------------------------------------------
	// END
	//----------------------------------------------------------------------




    if (act_reg.Control.Retc == SNAP_RETC_FAILURE) {
	    fprintf(stderr, " ==> RETURN CODE FAILURE <==\n");
	    return 1;
    }

    //std::cout << "Output is : " << (char *)((unsigned long)dout_gmem + 0) << "\n";

		#ifndef DATA_MOVE_OVER_AXI
    std::cout << ">> ACTION TYPE = "<< (unsigned int)Action_Config.action_type <<" - RELEASE_LEVEL =  " << (unsigned int)Action_Config.release_level << " <<\n";
		#endif /* DATA_MOVE_OVER_AXI */

    delete[] error;
		delete[] accuracy_vec;
    for(unsigned int i = 0; i < MAX_NUMBER_IMAGES_TEST_SET; i++)
    	delete[] vecPredictedStringInd[i];
    delete[] vecPredictedStringInd;
    delete[] vecPredictedStringLen;
    delete[] numberOfColumnsVec;
    return 0;
}


static inline void tbhexdump(FILE *fp, const void *buff, unsigned int size)
{
        unsigned int i;
        const uint8_t *b = (uint8_t *)buff;
        char ascii[17];
        char str[2] = { 0x0, };

        if (size == 0)
                return;

        for (i = 0; i < size; i++) {
                if ((i & 0x0f) == 0x00) {
                        fprintf(fp, " %08x:", i);
                        memset(ascii, 0, sizeof(ascii));
                }
                fprintf(fp, " %02x", b[i]);
                str[0] = isalnum(b[i]) ? b[i] : '.';
                str[1] = '\0';
                strncat(ascii, str, sizeof(ascii) - 1);

                if ((i & 0x0f) == 0x0f)
                        fprintf(fp, " | %s\n", ascii);
        }
        /* print trailing up to a 16 byte boundary. */
        for (; i < ((size + 0xf) & ~0xf); i++) {
                fprintf(fp, "   ");
                str[0] = ' ';
                str[1] = '\0';
                strncat(ascii, str, sizeof(ascii) - 1);

                if ((i & 0x0f) == 0x0f)
                        fprintf(fp, " | %s\n", ascii);
        }
        fprintf(fp, "\n");
}


#endif /* NO_SYNTH */
