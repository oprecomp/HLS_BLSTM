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
 * @file snap_blstm.cpp
 * @author Dionysios Diamantopoulos, did@zurich.ibm.com, FPGA and PULP porting
 * @author Vladimir Rybalkin, rybalkin@eit.uni-kl.de, BLSTM code
 * @date 10 Dec 2017
 * @brief Getting data from image datasets into the FPGA, process it using a SNAP
 * action and a BLSTM accelerator and move the data out of the FPGA back to host-DRAM.
 */


#include <getopt.h>
#include <sys/time.h>
#include <assert.h>

#include <iostream>     // std::cout, std::cerr
#include <fstream>      // std::ifstream std::ofstream
#include <vector>
#include <sys/types.h>
#include <algorithm>	// std::sort

/* Bypassing compiling of snap libs support for old systems (<el5) */
#ifndef CLOCK_MONOTONIC_RAW
#define CLOCK_MONOTONIC_RAW
#endif

/* SNAP related header functions */
#include <snap_tools.h>
#include <libsnap.h>
#include <action_blstm.h>
#include <snap_hls_if.h>
#include <dirent.h>

#include "third-party/xilinx/ap_int.h"

#include "../../actions/hls_blstm/include/common_def.h"

#include <errno.h>

#include "snap_blstm.hpp"
#include <sstream>



#define NAME_BUFF 512

#define MAX_PIXELS_PER_IMAGE 2 * MAX_NUMBER_COLUMNS_TEST_SET * HIGHT_IN_PIX


int verbose_flag = 0;

static const char *version = GIT_VERSION;

static const char *mem_tab[] = { "HOST_DRAM", "CARD_DRAM", "TYPE_NVME" };


/**
 * @brief	prints valid command line options
 * @param prog	BLSTM algorithm with CAPI support for OpenPOWER systems
 */
static void usage(const char *prog)
{
	printf("BLSTM algorithm with CAPI support for OpenPOWER systems\n"
	       "Usage: %s [-h] [-v, --verbose] [-V, --version]\n"
	       "  -C, --card <cardno> can be (0...3)\n"
	       "  -i, --input_img_dir <images dir>  input directory\n"
	       "  -g, --input_grt_dir <groundtruth dir>  input directory\n"
	       "  -o, --output <file.txt>   output file\n"
	       "  -A, --type-in <CARD_DRAM, HOST_DRAM, ...>.\n"
	       "  -a, --addr-in <addr>      address e.g. in CARD_RAM\n"
	       "  -D, --type-out <CARD_DRAM, HOST_DRAM, ...>.\n"
	       "  -d, --addr-out <addr>     address e.g. in CARD_RAM\n"
	       "  -n, --num_hw_threads <num> numbers of hw accelerators\n"
	       "  -t, --timeout             timeout in sec to wait for done\n"
	       "  -X, --verify              verify result if possible\n"
	       "  -N, --no-irq              disable Interrupts\n"
	       "\n"
	       "Example:\n"
	       "  snap_blstm -i in_dir -g gd_dir -o out.txt -n 1 ...\n"
	       "\n"
	       "Report bugs to did@zurich.ibm.com\n\n",
	       prog);
}

/**
 * @brief Preparing the execution of an action by initialing I/O and MMIO.
 * @param mjob Pointer to SNAP job struct.
 * @param addr_in The address of input buffer.
 * @param size_in The size of input buffer in bytes.
 * @param type_in The type of input buffer (host-DRAM etc.).
 * @param addr_out The address of output buffer.
 * @param size_out The size of output buffer in bytes.
 * @param type_out The type of output buffer (host-DRAM etc.).
 * @param cols An array storing the number of columns of the images of current action.
 */
static void snap_prepare_blstm(struct snap_job *cjob,
				 struct blstm_job *mjob,
				 void *addr_in,
				 uint32_t size_in,
				 uint8_t type_in,
				 void *addr_out,
				 uint32_t size_out,
				 uint8_t type_out,
				 uint16_t *cols)
{
	if (DEBUG_LEVEL >= LOG_ERROR) fprintf(stderr, "  prepare blstm job of %ld bytes size\n", sizeof(*mjob));

	assert(sizeof(*mjob) <= SNAP_JOBSIZE);
	memset(mjob, 0, sizeof(*mjob));

	snap_addr_set(&mjob->in, addr_in, size_in, type_in,
		      SNAP_ADDRFLAG_ADDR | SNAP_ADDRFLAG_SRC);
	snap_addr_set(&mjob->out, addr_out, size_out, type_out,
		      SNAP_ADDRFLAG_ADDR | SNAP_ADDRFLAG_DST |
		      SNAP_ADDRFLAG_END);

	/* copying columns */
	memcpy(&mjob->imgcols, cols, sizeof(cols)*sizeof(cols[0]));

	snap_job_set(cjob, mjob, sizeof(*mjob), NULL, 0);
}






/**
 * @brief The main function. It is used both for HW and SW action.
 */
int main(int argc, char *argv[])
{
	int ch, rc = 0;
	int card_no = 0;
	struct snap_card *card = NULL;
	struct snap_action *action = NULL;
	char device[128];
	struct snap_job cjob;
	struct blstm_job mjob;
	const char *input_img_dir = NULL, *input_grt_dir = NULL;
	//const char *input_grt_dir = NULL;
	const char *output = NULL;
	unsigned long timeout = 600;
	const char *space = "CARD_RAM";
	struct timeval etime[MAX_NUMBER_IMAGES_TEST_SET+1], stime[MAX_NUMBER_IMAGES_TEST_SET+1];
	long long snap_action_total_time = 0;
	ssize_t size_in, size_out;
	float *ibuff = NULL;
	unsigned int *obuff = NULL;
	char **filenames = NULL;
	uint8_t type_in = SNAP_ADDRTYPE_HOST_DRAM;
	uint64_t addr_in = 0x0ull;
	uint8_t type_out = SNAP_ADDRTYPE_HOST_DRAM;
	uint64_t addr_out = 0x0ull;
	int verify = 0;
	int exit_code = EXIT_SUCCESS;
	snap_action_flag_t action_irq = (snap_action_flag_t)(SNAP_ACTION_DONE_IRQ | SNAP_ATTACH_IRQ);
	unsigned int total_pixels_in_action = 0;
	std::string inputFileImageDir, inputFileGroundTruthDir;
	unsigned int hw_threads = HW_THREADS_PER_ACTION;

	gettimeofday(&stime[MAX_NUMBER_IMAGES_TEST_SET], NULL);

	while (1) {
		int option_index = 0;
		static struct option long_options[] = {
			{ "card",	 	required_argument, NULL, 'C' },
			{ "input_img_dir",	required_argument, NULL, 'i' },
			{ "input_grt_dir",	required_argument, NULL, 'g' },
			{ "output",		no_argument	 , NULL, 'o' },
			{ "src-type",		required_argument, NULL, 'A' },
			{ "src-addr",	 	required_argument, NULL, 'a' },
			{ "dst-type",	 	required_argument, NULL, 'D' },
			{ "dst-addr",	 	required_argument, NULL, 'd' },
			{ "num_hw_threads",	required_argument, NULL, 'n' },
			{ "timeout",	 	required_argument, NULL, 't' },
			{ "verify",	 	no_argument	 , NULL, 'X' },
			{ "no-irq",	 	no_argument	 , NULL, 'N' },
			{ "version",	 	no_argument	 , NULL, 'V' },
			{ "verbose",	 	no_argument	 , NULL, 'v' },
			{ "help",	 	no_argument	 , NULL, 'h' },
			{ 0,		 	no_argument	 , NULL, 0   },
		};

		ch = getopt_long(argc, argv,
				 "C:i:g:o:A:a:D:d:n:t:XNVvh",
				 long_options, &option_index);
		if (ch == -1)
			break;

		switch (ch) {
		case 'C':
			card_no = strtol(optarg, (char **)NULL, 0);
			break;
		case 'i':
			input_img_dir = optarg;
			break;
		case 'g':
			input_grt_dir = optarg;
			break;
		case 'o':
			output = optarg;
			break;
		case 'n':
			hw_threads = __str_to_num(optarg);
			break;
		case 't':
			timeout = strtol(optarg, (char **)NULL, 0);
			break;
			/* input data */
		case 'A':
			space = optarg;
			if (strcmp(space, "CARD_DRAM") == 0)
				type_in = SNAP_ADDRTYPE_CARD_DRAM;
			else if (strcmp(space, "HOST_DRAM") == 0)
				type_in = SNAP_ADDRTYPE_HOST_DRAM;
			else {
				usage(argv[0]);
				exit(EXIT_FAILURE);
			}
			break;
		case 'a':
			addr_in = strtol(optarg, (char **)NULL, 0);
			break;
			/* output data */
		case 'D':
			space = optarg;
			if (strcmp(space, "CARD_DRAM") == 0)
				type_out = SNAP_ADDRTYPE_CARD_DRAM;
			else if (strcmp(space, "HOST_DRAM") == 0)
				type_out = SNAP_ADDRTYPE_HOST_DRAM;
			else {
				usage(argv[0]);
				exit(EXIT_FAILURE);
			}
			break;
		case 'd':
			addr_out = strtol(optarg, (char **)NULL, 0);
			break;
		case 'X':
			verify++;
			break;
			/* service */
		case 'V':
			printf("%s\n", version);
			exit(EXIT_SUCCESS);
		case 'v':
			verbose_flag = 1;
			break;
		case 'h':
			usage(argv[0]);
			exit(EXIT_SUCCESS);
			break;
		case 'N':
			action_irq = (snap_action_flag_t)0;
			break;
		default:
			usage(argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	if (optind != argc) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	// Source directories for images and ground truth files
	if ((input_img_dir != NULL) &&  (input_grt_dir != NULL)) {
		inputFileImageDir = input_img_dir;
		inputFileGroundTruthDir = input_grt_dir;
	}
	else {
			usage(argv[0]);
			exit(EXIT_FAILURE);
	}

	assert (hw_threads == HW_THREADS_PER_ACTION); // FIXME

	// The initialization of the alphabet
	Alphabet alphabet;
	alphabet.Init("/tools/projects/snap/actions/hls_blstm/data/alphabet/alphabet.txt");
	//alphabet.Init("../data/alphabet/alphabet.txt");
	//alphabet.Print();
	// Return the list of images' file names
	std::vector<std::string> listOfImages = open(inputFileImageDir);
	unsigned int imgs = listOfImages.size();
	// Return the list of ground truth' file names
	std::vector<std::string> listOfGroundTruth = open(inputFileGroundTruthDir);
	unsigned int imgs_gd = listOfImages.size();

	/* Check that there are equal number of image files and groundtruth files */
	assert(((imgs == imgs_gd) != 0) && ("#Input Images / #Groundtruth shall be equal and at least one."));

	//----------------------------------------------------------------------
	// Read Images
	//----------------------------------------------------------------------

	filenames = (char**)malloc(listOfImages.size() * sizeof(char*));
	assert (filenames != NULL);
	std::vector<InputImage> vecInputImage;
	vecInputImage.resize(listOfImages.size());

	for(unsigned int i = 0; i < listOfImages.size(); i++)
	{
		std::string inputFileImage = inputFileImageDir + listOfImages.at(i);
		filenames[i] = (char*)malloc(NAME_BUFF * sizeof(char));
		assert (filenames[i] != NULL);
		vecInputImage.at(i).Init(inputFileImage);
        if (strlen(filenames[i]) <= NAME_BUFF) {
            strcpy(filenames[i], inputFileImage.c_str());
        }
        else {
            if (DEBUG_LEVEL >= LOG_CRITICAL) printf("ERROR: filename length is higher (%u) than limit (%u)\n", \
                (unsigned int)strlen(filenames[i]), NAME_BUFF);
            exit(EXIT_FAILURE);
        }
	}

	log(LOG_DEBUG) << "DEBUG: listOfImages.size() = " << listOfImages.size() << "\n";

	//----------------------------------------------------------------------
	// Read Ground Truth
	//----------------------------------------------------------------------

	std::vector<GroundTruth> vecGroundTruth;
	vecGroundTruth.resize(listOfImages.size());

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

	// Verify that we hold enough space for the loaded images of folder provided
	assert(listOfImages.size() <= MAX_NUMBER_IMAGES_TEST_SET);


	/* Allocate space for predicted length and string ids */
	unsigned int *vecPredictedStringLen = (unsigned int *)malloc(listOfImages.size()\
	 * sizeof(unsigned int ));
	assert (vecPredictedStringLen != NULL);
	unsigned int **vecPredictedStringInd = (unsigned int **)malloc(listOfImages.size()\
	 * sizeof(unsigned int *));
	assert (vecPredictedStringInd != NULL);
	for(unsigned int i = 0; i < listOfImages.size(); i++) {
		vecPredictedStringInd[i] = (unsigned int *)malloc(MAX_NUMBER_COLUMNS_TEST_SET\
	 * sizeof(unsigned int));
	 assert (vecPredictedStringInd[i] != NULL);
 	}

	double *error = new double [listOfImages.size()];
	double errorSum = 0.0;
	double accuracy = 0.0;


	short unsigned int numberOfColumnsVec[MAX_NUMBER_IMAGES_TEST_SET];


	/* if output file is defined, use that as output */
	if (output != NULL) {

		/* Erase file, since we append results later */
		FILE *fp = fopen(output, "w");
		if (!fp) {
		  if (DEBUG_LEVEL >= LOG_ERROR) fprintf(stderr, "err: Cannot open file %s for writing: %s\n",
                		output, strerror(errno));
		  return -ENODEV;
		}
		fclose(fp);
	}
	else
		if (DEBUG_LEVEL >= LOG_INFO) fprintf(stderr, "INFO: no output file provided, verification only with sw.\n");


	//====================================================================================================================================================================================================================
	// START
	//====================================================================================================================================================================================================================

	log(LOG_INFO) << "Start ..." << std::endl;

	// Starting point - the first time stemp
	time_t t1 = time(0);

	/* source buffer */
	size_in = ACC_CALLS_PER_ACTION * MAX_PIXELS_PER_IMAGE * sizeof(float); // for fw and bw
	ibuff = (float*)snap_malloc(size_in);
	if (ibuff == NULL) {
		log(LOG_ERROR) << "Error on allocating ibuf. Aborting...\n";
		exit(EXIT_FAILURE);
	}
	memset(ibuff, 0x0, size_in);
	type_in = SNAP_ADDRTYPE_HOST_DRAM;
	addr_in = (unsigned long)ibuff;


	/* output buffer */
	size_out = ACC_CALLS_PER_ACTION * MAX_PREDICTED_STRING_LENGTH * sizeof(uint32_t);
	obuff = (unsigned int*)snap_malloc(size_out);
	if (obuff == NULL) {
		log(LOG_ERROR) << "Error on allocating obuf. Aborting...\n";
		exit(EXIT_FAILURE);
	}
	type_out = SNAP_ADDRTYPE_HOST_DRAM;
	addr_out = (unsigned long)obuff;


	/* check that there are enough MMIO register entries to hold the columns of every image */
	assert(ACC_CALLS_PER_ACTION <= sizeof(mjob.imgcols)/sizeof(mjob.imgcols.cols[0]));

	/* check that inputs are multiples of what accelerator is waiting for every iteration */
	assert (ceil((float)listOfImages.size() / ACC_CALLS_PER_ACTION) == floor((float)listOfImages.size() / ACC_CALLS_PER_ACTION));
	/* previous assertion verifies that action_loops is integer FIXME: should update it to any case of reminder */

	uint16_t cols[8];


	snprintf(device, sizeof(device)-1, "/dev/cxl/afu%d.0s", card_no);
	card = snap_card_alloc_dev(device, SNAP_VENDOR_ID_IBM,
			 SNAP_DEVICE_ID_SNAP);
	if (card == NULL) {
		if (DEBUG_LEVEL >= LOG_CRITICAL) fprintf(stderr, "err: failed to open card /dev/cxl/afu%u.0s: %s\n",
				card_no, strerror(errno));
		__free(obuff);
		__free(ibuff);
		exit(EXIT_FAILURE);
	}

	action = snap_attach_action(card, BLSTM_ACTION_TYPE, action_irq, 60);
	if (action == NULL) {
		if (DEBUG_LEVEL >= LOG_CRITICAL) fprintf(stderr, "err: failed to attach action %u: %s\n",
				card_no, strerror(errno));
		snap_card_free(card);
		exit(EXIT_FAILURE);
	}


	/* Main loop over the provided image dataset. Images are processed in groups of ACC_CALLS_PER_ACTION */
	for(unsigned int i = 0; i < vecInputImage.size(); i += ACC_CALLS_PER_ACTION) {

		/* Write on MMIO register the number of columns of current image */
	    memset(cols, 0, sizeof(mjob.imgcols));

		/* Write on MMIO register the number of columns of current image */
	    memset(mjob.imgstrlen.cols, 0, sizeof(mjob.imgstrlen));

	    total_pixels_in_action = 0;

	    /* Loop over every single image of the current action */
	    for (unsigned int j = 0; j < ACC_CALLS_PER_ACTION; j++) {
	    	numberOfColumnsVec[i+j] = vecInputImage.at(i+j).numberOfColumns;
	    	cols[j] = numberOfColumnsVec[i+j];
	    	log(LOG_DEBUG) << "DEBUG: numberOfColumnsVec[" << i+j << "] = " << cols[j] << ", total_pixels_in_action = " <<  total_pixels_in_action << std::endl;
#if IMG_FLOAT_TO_FIXED_CASTING_IN_CPU == 1
				/* Ensure that the casting space is 8-bits FIXME: No-support so far for arbitrary fixed point type for image, when casting is done in SW. */
				assert(sizeof(DTYPE_IMG) == 1);
	    	/* Copy actual fw/bw data of every image and cast from float to DTYPE_IMG. Then move the casted value as a raw byte on 1st byte of a float and store to buffer.
				 * This snippet does not utilize the 2nd-4th bytes of float, leading to bandwidth underutilization at 75%. However this is only a workaround for Xilinx VHLS 2017.4
				 * which seems to have a bug with casting in HW (the generated RTL is producing 0s, compared to v2017.2 which was ok. Nornally, casting has to be done in HW.
				 */
	    	for(unsigned int k = 0 ; k < numberOfColumnsVec[i+j] * HIGHT_IN_PIX; k++) {
					float val_in_float_fw = vecInputImage.at(i+j).image_fw[k];
					float val_in_float_bw = vecInputImage.at(i+j).image_bw[k];
					DTYPE_IMG val_in_apfixed_fw = (DTYPE_IMG)val_in_float_fw;
					DTYPE_IMG val_in_apfixed_bw = (DTYPE_IMG)val_in_float_bw;
					float val_to_send_fw = 0;
					float val_to_send_bw = 0;
					*((DTYPE_IMG*)(&val_to_send_fw) + 0) = val_in_apfixed_fw;
					*((DTYPE_IMG*)(&val_to_send_bw) + 0) = val_in_apfixed_bw;
					ibuff[total_pixels_in_action + k] = val_to_send_fw;
					ibuff[total_pixels_in_action + numberOfColumnsVec[i+j] * HIGHT_IN_PIX + k] = val_to_send_bw;
	    		/* log(LOG_DEBUG) << "DEBUG: bw index = " << total_pixels + numberOfColumnsVec[i] * HIGHT_IN_PIX + j << ", vecInputImage.at(" << i << ").image_bw[" << j << "]=" << \
					vecInputImage.at(i).image_bw[j] << "\n";
	    		 */
	    	}
#else
				memcpy(ibuff + total_pixels_in_action, vecInputImage.at(i+j).image_fw, (numberOfColumnsVec[i+j] * HIGHT_IN_PIX) * sizeof(float));
				memcpy(ibuff + total_pixels_in_action + numberOfColumnsVec[i+j] * HIGHT_IN_PIX, vecInputImage.at(i+j).image_bw, (numberOfColumnsVec[i+j] * HIGHT_IN_PIX) * sizeof(float));
#endif
			/* Update the number of pixels */
	    	total_pixels_in_action += 2 * numberOfColumnsVec[i+j] * HIGHT_IN_PIX;
	    	log(LOG_INFO) << "INFO: numberOfColumnsVec[" << i+j << "] = " <<  numberOfColumnsVec[i+j] << std::endl;
	    }

	    size_in = total_pixels_in_action*sizeof(float);

	    if (DEBUG_LEVEL >= LOG_INFO) printf("ACTION PARAMETERS:\n");
        for (unsigned int j = 0; j < ACC_CALLS_PER_ACTION; j++)
		    if (DEBUG_LEVEL >= LOG_INFO) printf(	"  input image %u: %s, %u columns, %u fw-bw pixels, %u bytes\n", i+j, \
                filenames[i+j], numberOfColumnsVec[i+j], 2*numberOfColumnsVec[i+j]*HIGHT_IN_PIX,\
                (unsigned int)(2*numberOfColumnsVec[i+j]*HIGHT_IN_PIX*sizeof(float)));
		if (DEBUG_LEVEL >= LOG_INFO) printf(	"  output:      %s\n"
			"  type_in:     %x %s\n"
			"  addr_in:     %016llx\n"
			"  type_out:    %x %s\n"
			"  addr_out:    %016llx\n"
			"  size_in:     %u (0x%08lx)\n"
			"  size_out:    %u (0x%08lx)\n",
			output ? output : "not-provided",
					type_in,  mem_tab[type_in],  (long long)addr_in,
					type_out, mem_tab[type_out], (long long)addr_out,
					(unsigned int)size_in, size_in, (unsigned int)size_out, size_out);


		snap_prepare_blstm(&cjob, &mjob,
				(void *)addr_in,  size_in, type_in,
				(void *)addr_out, size_out, type_out,
				cols);

		if (DEBUG_LEVEL >= LOG_INFO) __hexdump(stderr, &mjob, sizeof(mjob));

		gettimeofday(&stime[i], NULL);
		rc = snap_action_sync_execute_job(action, &cjob, timeout);

		gettimeofday(&etime[i], NULL);
		if (rc != 0) {
			if (DEBUG_LEVEL >= LOG_CRITICAL) fprintf(stderr, "err: job execution %d: %s!\n", rc,
					strerror(errno));
			snap_detach_action(action);
			exit(EXIT_FAILURE);
		}

		if (DEBUG_LEVEL >= LOG_INFO) fprintf(stdout, "INFO: Accelerator returned code on MMIO (AXILite job struct field) : %u\n", mjob.status );

		if (DEBUG_LEVEL >= LOG_INFO) fprintf(stdout, "INFO: AXI transactions registered on MMIO : In: %u(0x%x), Out: %u(0x%x)  \n", \
				mjob.axitrans_in, mjob.axitrans_in, mjob.axitrans_out, mjob.axitrans_out);


		if (DEBUG_LEVEL >= LOG_INFO) __hexdump(stderr, &mjob, sizeof(mjob));


		unsigned int str_addr_index = 0;
		for (unsigned int j = 0; j < ACC_CALLS_PER_ACTION; j++) {
			vecPredictedStringLen[i+j] = mjob.imgstrlen.cols[j];
			log(LOG_DEBUG) << "DEBUG tb: vecPredictedStringLen[" << i+j << "] = " << mjob.imgstrlen.cols[j] << std::endl;
			for (unsigned int l = 0; l < vecPredictedStringLen[i+j]; l++) {
				vecPredictedStringInd[i+j][l] = obuff[str_addr_index];
				/* log(LOG_DEBUG) << "DEBUG tb: vecPredictedStringInd[" << i+j << "][" << l <<\
						"] = obuff["<< str_addr_index << "] = " << obuff[str_addr_index] << std::endl;
				*/
				str_addr_index++;
			}


		    /* If the output buffer is in host DRAM we can write it to a file */
		    if (output != NULL) {
			    if (DEBUG_LEVEL >= LOG_INFO) fprintf(stdout, "INFO: writing output data %p %u uintegers to %s\n",
					obuff, vecPredictedStringLen[i+j], output);

			    rc = file_write(output, filenames[i+j], vecPredictedStringInd[i+j], vecPredictedStringLen[i+j]*sizeof(unsigned int));
			    if (rc != (int)(vecPredictedStringLen[i+j])) {
			        log(LOG_ERROR) << "Error on writing the exact number of indexes to " << output << std::endl;
			    }
		    }
		}


		if (DEBUG_LEVEL >= LOG_INFO) fprintf(stdout, "INFO: RETC=%x\n", cjob.retc);
		if (cjob.retc != SNAP_RETC_SUCCESS) {
			if (DEBUG_LEVEL >= LOG_ERROR) fprintf(stderr, "err: Unexpected RETC=%x!\n", cjob.retc);
			snap_detach_action(action);
			exit(EXIT_FAILURE);
		}

		if (DEBUG_LEVEL >= LOG_INFO) fprintf(stdout, "INFO: SNAP run %u blstm took %lld usec\n",
				i, (long long)timediff_usec(&etime[i], &stime[i]));


	} /* for list of images += ACC_CALLS_PER_ACTION */

	snap_detach_action(action);
	snap_card_free(card);

	__free(ibuff);
	__free(obuff);

	/* free the filenames */
	for(unsigned int i = 0; i < listOfImages.size(); i++)
		free(filenames[i]);
	free(filenames);

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


	for(unsigned int i = 0; i < listOfImages.size(); i++) {

		//----------------------------------------------------------------------
		// Calculate Levenshtein Distance for each string and output result
		//----------------------------------------------------------------------
		std::string groundTruthstring = vecGroundTruth.at(i).ReturnString();
		error[i] = LevenshteinDistance(vecPredictedString.at(i), groundTruthstring);
		log(LOG_INFO) << i << " Expected: "<< groundTruthstring \
				  << "\n Predicted: " << vecPredictedString.at(i) << " Accuracy: " << (1-error[i])*100 << " %\n";

		log(LOG_INFO) << " Predicted id: ";
		for(unsigned int j = 0; j < vecPredictedStringLen[i]; j++)
			log(LOG_INFO) << vecPredictedStringInd[i][j] << " ";
		log(LOG_INFO) << std::endl;

		log(LOG_INFO) << "INFO: Accelerator status code on MMIO : " \
				  << mjob.status << " (0x" << std::hex << mjob.status << ")" << std::dec << std::endl;

		log(LOG_INFO) << "INFO: AXI transaction registered on MMIO : \nINFO: In: " \
				  << mjob.axitrans_in  << " (0x" << std::hex << mjob.axitrans_in  << ")\nINFO: Out:" << std::dec \
				  << mjob.axitrans_out << " (0x" << std::hex << mjob.axitrans_out << ")" << std::dec << std::endl;

    	//for (unsigned int j = 0; j < sizeof(act_reg.Data.imgcols.cols)/sizeof(uint16_t); j++)
    	//	log(LOG_INFO) << "act_reg->Data.imgcols.cols[" << j << "] = " << act_reg.Data.imgcols.cols[j] << std::endl;

		//error[i] = LevenshteinDistanceCStyle(predictedString.c_str(), groundTruthstring.c_str());

		#if PROFILE
			log(LOG_INFO) << vecPredictedString.at(i) << std::endl;
			log(LOG_INFO) << groundTruthstring << std::endl << std::endl;
		#endif

		vecPredictedString.at(i).clear();
		groundTruthstring.clear();
		if (i%ACC_CALLS_PER_ACTION == 0)
			snap_action_total_time += (long long)timediff_usec(&etime[i], &stime[i]);
		//groundTruth.Free();
	}

	for(unsigned int e = 0; e < listOfImages.size(); e++)
		errorSum += error[e];

	accuracy = (1.0 - errorSum / (float)listOfImages.size()) * 100.0;
	log(LOG_CRITICAL) << "Accuracy: " <<  accuracy << "%" << std::endl;

	errorSum = 0.0;

	//std::chrono::seconds time_span = std::chrono::duration_cast<std::chrono::seconds>(t2-t1);
	double time_span = difftime( t2, t1);

	for(unsigned int i = 0; i < listOfImages.size(); i++)
		free (vecPredictedStringInd[i]);
	free(vecPredictedStringInd);
	free(vecPredictedStringLen);

	gettimeofday(&etime[MAX_NUMBER_IMAGES_TEST_SET], NULL);


  log(LOG_CRITICAL) << "Measured time ... " << time_span << " seconds (" <<
  (long long)timediff_usec(&etime[MAX_NUMBER_IMAGES_TEST_SET], &stime[MAX_NUMBER_IMAGES_TEST_SET])
  << " us) for " << listOfImages.size() << " images. Action time " << snap_action_total_time << " us (" <<
  snap_action_total_time/(MAX_NUMBER_IMAGES_TEST_SET/ACC_CALLS_PER_ACTION) <<
  " us per action -> " << ACC_CALLS_PER_ACTION << " images, ~" <<
   snap_action_total_time/MAX_NUMBER_IMAGES_TEST_SET
   << " us / image)" << std::endl << std::endl;

	//----------------------------------------------------------------------
	// END
	//----------------------------------------------------------------------


    if (cjob.retc  == SNAP_RETC_FAILURE) {
	    if (DEBUG_LEVEL >= LOG_CRITICAL) fprintf(stderr, " ==> RETURN CODE FAILURE <==\n");
	    return 1;
    }



    delete[] error;


    exit(exit_code);

} /* END main */






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
		std::cerr << "ERROR: Failed to open Alphabet on path " << inputFileAlphabet
		<< " Aborting..." << std::endl;
		exit(EXIT_FAILURE);
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

	while(inputStream >> pix)
	{
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

	numberOfColumns = tmp.size() / HIGHT_IN_PIX;

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


//====================================================================================================================================================================================================================
// AUXILIARY
//====================================================================================================================================================================================================================

std::vector<std::string> open(std::string path)
{
	DIR*    dir;
	dirent* pdir;
	std::vector<std::string> files;

	dir = opendir(path.empty() ? "." : path.c_str());

	while ( (pdir = readdir(dir) )) {
			/* std::cout << "pushed " << pdir->d_name << std::endl; */
			files.push_back(pdir->d_name);
	}

	closedir(dir);

	std::sort(files.begin(), files.end());
	files.erase(files.begin(), files.begin()+2);

	log(LOG_INFO) << "INFO: Read " << (files.end() - files.begin()) << " files from path " <<  path << std::endl;

	if ((files.end() - files.begin()) > MAX_NUMBER_IMAGES_TEST_SET) {
		log(LOG_WARNING) << "WARNING: Max number of files reached (MAX_NUMBER_IMAGES_TEST_SET = " \
				  << MAX_NUMBER_IMAGES_TEST_SET << " )" << std::endl;
		files.erase((files.begin() + MAX_NUMBER_IMAGES_TEST_SET), files.end());
	}

	return files;
}



	/* from newron.cpp */

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





static inline ssize_t
file_write(const char *fname_out, const char *fname_in, const uint32_t *buff, size_t len)
{
	int rc=0;
	FILE *fp;
	unsigned int i;
	if ((fname_out == NULL) || (buff == NULL))
		return -EINVAL;
	fp = fopen(fname_out, "a");
	if (!fp) {
		if (DEBUG_LEVEL >= LOG_ERROR) fprintf(stderr, "err: Cannot open file %s: %s\n",
				fname_out, strerror(errno));
        return -ENODEV;
	}

	if (DEBUG_LEVEL >= LOG_INFO) rc = fprintf(fp, "Output from file : %s, %u characters\n", fname_in, (unsigned int)(len/sizeof(unsigned int)));

	if (rc == -1) {
              if (DEBUG_LEVEL >= LOG_ERROR) fprintf(stderr, "err: Cannot write to %s: %s\n",
            		  fname_out, strerror(errno));
              fclose(fp);
              return -EIO;
	}

	for (i = 0; i < len/sizeof(unsigned int); i++) {
		if (DEBUG_LEVEL >= LOG_INFO) rc = fprintf(fp, "%u\n", buff[i]);
		//rc = fwrite(buff, len, 1, fp);
		if (rc == -1) {
			if (DEBUG_LEVEL >= LOG_ERROR) fprintf(stderr, "err: Cannot write to %s: %s\n",
					fname_out, strerror(errno));
			fclose(fp);
			return -EIO;
		}
	}
	fclose(fp);

	if (len != 0)
		return i;
	else
		return 0;
}
