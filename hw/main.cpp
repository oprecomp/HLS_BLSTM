//
// Copyright (C) 2017 University of Kaiserslautern
// Microelectronic Systems Design Research Group
//
// Vladimir Rybalkin (rybalkin@eit.uni-kl.de)
// 20. February 2017
//



#include "neuron.hpp"
#include <sstream>


int main(int argc, const char* argv[])
{

	// Modifications to allow correct usage.
	if( argc != 1)
	{
		print_usage_and_exit(argv[0]);
	}


	unsigned int numThreads = GetNumberOfThreads();
	std::cout << "Number of threads on the current machine: " << numThreads << std::endl;
	char hostname[256];
	gethostname(hostname, 256);
	std::cout << "Host name: " << hostname << std::endl;

	//----------------------------------------------------------------------
	// Init main data structures
	//----------------------------------------------------------------------

	// Source directories for images and ground truth files
	std::string inputFileImageDir = "/home/projects/oprecomp/fpga/hls/mb/blstm/data/samples_sm/"; //argv[2];
	std::string inputFileGroundTruthDir = "/home/projects/oprecomp/fpga/hls/mb/blstm/data/gt_sm/"; //argv[3];
	//std::string inputFileGroundTruthDir = "../gt/";

	// The initialization of the NN for processing image in the forward direction
	//NeuralNetwork neuralNetwork_fw;
	//neuralNetwork_fw.Init("/home/projects/oprecomp/fpga/hls/mb/blstm/data/model/model_fw.txt");

	// The initialization of the NN for processing image in the backward direction
	//NeuralNetwork neuralNetwork_bw;
	//neuralNetwork_bw.Init("/home/projects/oprecomp/fpga/hls/mb/blstm/data/model/model_bw.txt");

	// The initialization of the alphabet
	Alphabet alphabet;
	alphabet.Init("/home/projects/oprecomp/fpga/hls/mb/blstm/data/alphabet/alphabet.txt");
	//alphabet.Print();
	// Return the list of images' file names
	std::vector<std::string> listOfImages = open(inputFileImageDir);

	// Return the list of ground truth' file names
	std::vector<std::string> listOfGroundTruth = open(inputFileGroundTruthDir);

	//----------------------------------------------------------------------
	// Read Images
	//----------------------------------------------------------------------

	std::vector<InputImage> vecInputImage;
	vecInputImage.resize(listOfImages.size());

	for(unsigned int i = 0; i < listOfImages.size(); i++)
	{
		std::string inputFileImage = inputFileImageDir + listOfImages.at(i);

		vecInputImage.at(i).Init(inputFileImage);
	}

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

	// Predicted Length of String
	//unsigned int *vecPredictedStringLen = new unsigned int [listOfImages.size()];
	// Predicted index of alphabet for every string: 2D array, one row for every image/string,
	// several columns for predicted characters
	//unsigned int **vecPredictedStringInd = new unsigned int *[listOfImages.size()];
	//for(unsigned int i = 0; i < vecInputImage.size(); i++)
	//	vecPredictedStringInd[i] = new unsigned int [MAX_NUMBER_COLUMNS_TEST_SET];

	// Verify that we hold enough space for the loaded images of folder provided
	assert(listOfImages.size() <= MAX_NUMBER_IMAGES_TEST_SET);

	unsigned int vecPredictedStringLen[MAX_NUMBER_IMAGES_TEST_SET];
	unsigned int vecPredictedStringInd[MAX_NUMBER_IMAGES_TEST_SET][MAX_NUMBER_COLUMNS_TEST_SET];

	double *error = new double [listOfImages.size()];
	double errorSum = 0.0;
	double accuracy = 0.0;


	//====================================================================================================================================================================================================================
	// START
	//====================================================================================================================================================================================================================

	std::cout << "Start ..." << std::endl;

	// Starting point - the first time stemp
	//auto t1 = std::chrono::high_resolution_clock::now();
	time_t t1 = time(0);

	hls::stream<DTYPE_IMG> image_fw[vecInputImage.size()];
	hls::stream<DTYPE_IMG> image_bw[vecInputImage.size()];

	unsigned int numberOfColumnsVec[MAX_NUMBER_IMAGES_TEST_SET];

	for(unsigned int i = 0; i < vecInputImage.size(); i++)
	{

		//const unsigned int numberOfColumns = vecInputImage.at(i).numberOfColumns;
		numberOfColumnsVec[i] = vecInputImage.at(i).numberOfColumns;

		// Hidden layer weigths
		float WGI_fw[NUMBER_OF_NEURONS * NUMBER_OF_INPUTS];// Input Gate
		float WGF_fw[NUMBER_OF_NEURONS * NUMBER_OF_INPUTS];// Forget Gate
		float WGO_fw[NUMBER_OF_NEURONS * NUMBER_OF_INPUTS];// Output Gate
		float WCI_fw[NUMBER_OF_NEURONS * NUMBER_OF_INPUTS];// Memory Cell Input

		// Peepholes
		float WIP_fw[NUMBER_OF_NEURONS];// Input Gate
		float WFP_fw[NUMBER_OF_NEURONS];// Forget Gate
		float WOP_fw[NUMBER_OF_NEURONS];// Output Gate

		//float image_fw[MAX_NUMBER_COLUMNS_TEST_SET * HIGHT_IN_PIX];
		//float image_bw[MAX_NUMBER_COLUMNS_TEST_SET * HIGHT_IN_PIX];



		for(unsigned int j = 0; j < MAX_NUMBER_COLUMNS_TEST_SET * HIGHT_IN_PIX; j++) {
			if (j < numberOfColumnsVec[i] * HIGHT_IN_PIX) { // save segfault
				image_fw[i].write(vecInputImage.at(i).image_fw[j]);
				image_bw[i].write(vecInputImage.at(i).image_bw[j]);
			}
		}
#if SIM_SINGLE == 1
		vecPredictedStringLen[i] = Single_Kernel_BLSTM(
				 image_fw[i],
				 image_bw[i],
				 numberOfColumnsVec[i],
				 vecPredictedStringInd[i]);
#endif
	}

#if SIM_SINGLE == 0
	Many_Kernels_BLSTM(
			 image_fw,
			 image_bw,
			 numberOfColumnsVec,
			 vecPredictedStringInd,
			 vecPredictedStringLen);
#endif
	// Do the translation from alphabet indexers to actual characters
	// Since some special characters reserve 2-3 char positions, we do the translation
	// to the SW, using string vectors, i.e. dynamic alloc, (avoiding 2D buffers on HW)
	for(unsigned int i = 0; i < vecInputImage.size(); i++) {
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

	for(unsigned int i = 0; i < vecInputImage.size(); i++)
	{

		//----------------------------------------------------------------------
		// Calculate Levenshtein Distance for each string and output result
		//----------------------------------------------------------------------
		std::string groundTruthstring = vecGroundTruth.at(i).ReturnString();
		error[i] = LevenshteinDistance(vecPredictedString.at(i), groundTruthstring);
		std::cout << i << " Expected: "<< groundTruthstring << "\n Predicted: " << vecPredictedString.at(i) << " Accuracy: " << (1-error[i])*100 << " %\n";
		//error[i] = LevenshteinDistanceCStyle(predictedString.c_str(), groundTruthstring.c_str());

		#if PROFILE
			std::cout << vecPredictedString.at(i) << std::endl;
			std::cout << groundTruthstring << std::endl << std::endl;
		#endif

		vecPredictedString.at(i).clear();
		groundTruthstring.clear();
		//groundTruth.Free();
	}

	for(unsigned int e = 0; e < listOfImages.size(); e++)
		errorSum += error[e];

	accuracy = (1.0 - errorSum / (float)listOfImages.size()) * 100.0;
	std::cout << "Accuracy: " <<  accuracy << "%" << std::endl;

	errorSum = 0.0;

	//std::chrono::seconds time_span = std::chrono::duration_cast<std::chrono::seconds>(t2-t1);
	double time_span = difftime( t2, t1);

	std::cout << "Measured time ... " << time_span << " seconds" << std::endl << std::endl;

	//----------------------------------------------------------------------
	// END
	//----------------------------------------------------------------------

	//delete[] vecPredictedStringLen;
	//for(unsigned int i = 0; i < vecInputImage.size(); i++)
	//	delete[] vecPredictedStringInd[i];
	//delete[] vecPredictedStringInd;
	delete[] error;

	return 0;
}
