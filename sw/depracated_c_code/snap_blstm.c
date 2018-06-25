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

/**
 * SNAP BLSTM (OCR application based on NN)
 *
 * Getting data from image datasets into the FPGA, process it using a SNAP
 * action and a BLSTM accelerator and move the data out of the FPGA back to host-DRAM.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>
#include <malloc.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <assert.h>

#include <snap_tools.h>
#include <libsnap.h>
#include <action_blstm.h>
#include <snap_hls_if.h>
#include <dirent.h>
#include "../../actions/hls_blstm/include/common_def.h"

#define NAME_BUFF 512

int verbose_flag = 0;

static const char *version = GIT_VERSION;

static const char *mem_tab[] = { "HOST_DRAM", "CARD_DRAM", "TYPE_NVME" };

/**
 * @brief	prints valid command line options
 *
 * @param prog	current program's name
 */
static void usage(const char *prog)
{
	printf("Usage: %s [-h] [-v, --verbose] [-V, --version]\n"
	       "  -C, --card <cardno> can be (0...3)\n"
	       "  -i, --input <images dir>  input directory.\n"
	       "  -o, --output <file.txt>   output file.\n"
	       "  -A, --type-in <CARD_DRAM, HOST_DRAM, ...>.\n"
	       "  -a, --addr-in <addr>      address e.g. in CARD_RAM.\n"
	       "  -D, --type-out <CARD_DRAM, HOST_DRAM, ...>.\n"
	       "  -d, --addr-out <addr>     address e.g. in CARD_RAM.\n"
	       "  -s, --size <size>         size of data.\n"
	       "  -t, --timeout             timeout in sec to wait for done.\n"
	       "  -X, --verify              verify result if possible\n"
	       "  -N, --no-irq              disable Interrupts\n"
	       "\n"
	       "Example:\n"
	       "  snap_blstm -i in_dir -o out.txt ...\n"
	       "\n",
	       prog);
}

static void snap_prepare_blstm(struct snap_job *cjob,
				 struct blstm_job *mjob,
				 void *addr_in,
				 uint32_t size_in,
				 uint8_t type_in,
				 void *addr_out,
				 uint32_t size_out,
				 uint8_t type_out)
{
	fprintf(stderr, "  prepare blstm job of %ld bytes size\n", sizeof(*mjob));

	assert(sizeof(*mjob) <= SNAP_JOBSIZE);
	memset(mjob, 0, sizeof(*mjob));

	snap_addr_set(&mjob->in, addr_in, size_in, type_in,
		      SNAP_ADDRFLAG_ADDR | SNAP_ADDRFLAG_SRC);
	snap_addr_set(&mjob->out, addr_out, size_out, type_out,
		      SNAP_ADDRFLAG_ADDR | SNAP_ADDRFLAG_DST |
		      SNAP_ADDRFLAG_END);

	snap_job_set(cjob, mjob, sizeof(*mjob), NULL, 0);
}



static inline ssize_t
alphabet_read(const char *fname, char *buff, size_t len)
{
	int rc=0;
	FILE *fp;

	if ((fname == NULL) || (buff == NULL) || (len == 0))
		return -EINVAL;

	fp = fopen(fname, "r");
	if (!fp) {
		fprintf(stderr, "err: Cannot open file %s: %s\n",
			fname, strerror(errno));
		return -ENODEV;
	}



	for (unsigned int i = 0; i < len; i++, rc++) {
		 if (fscanf(fp,"%c\n",&buff[i]) == EOF)
			 break;

		 buff[i] = fgetc(fp);
	}

	for (unsigned int i = 0; i < (size_t)rc; i++)
			 fprintf(stdout, "%d: %d %c \n", i, buff[i], buff[i]);

	printf("DEBUG: read %d things for alphabet\n", rc);
	if (rc == -1) {
		fprintf(stderr, "err: Cannot read from %s: %s\n",
			fname, strerror(errno));
		fclose(fp);
		return -EIO;
	}
	fclose(fp);

	return rc;
}


static inline ssize_t
file_read(const char *fname, float *buff, size_t len)
{
	struct timeval etime, stime;
	gettimeofday(&stime, NULL);

	int rc=0;
	FILE *fp;

	if ((fname == NULL) || (buff == NULL) || (len == 0))
		return -EINVAL;

	fp = fopen(fname, "r");
	if (!fp) {
		fprintf(stderr, "err: Cannot open file %s: %s\n",
			fname, strerror(errno));
		return -ENODEV;
	}

	for (unsigned int i = 0; i < len/sizeof(float); i++, rc++)
		 if (fscanf(fp,"%f",&buff[i]) == EOF)
			 break;
	/* fseek(fp, offset, SEEK_SET); buff[i] = fgetc(fp); */

	/* fread is 3x quicker that fscanf loop, but requires an exact number of bytes per line */
	/* assuming a single line in .txt input file consumes 4x4bytes (1-sign, 1-dot and up to .13f) */
	//rc = fread(buff, 4*sizeof(float), 100, fp);
	printf("DEBUG: read %d things\n", rc);
	if (rc == -1) {
		fprintf(stderr, "err: Cannot read from %s: %s\n",
			fname, strerror(errno));
		fclose(fp);
		return -EIO;
	}
	fclose(fp);
	gettimeofday(&etime, NULL);

	fprintf(stdout, "WARNING: file_read took %lld usec\n",
		(long long)timediff_usec(&etime, &stime));

	/* check that read pixels are aligned to HIGHT_IN_PIX, thus int cols are caclulated */
	assert (rc%HIGHT_IN_PIX == 0);

	return rc;
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
		fprintf(stderr, "err: Cannot open file %s: %s\n",
				fname_out, strerror(errno));
        return -ENODEV;
	}

	rc = fprintf(fp, "Output from file : %s, %u characters\n", fname_in, (unsigned int)(len/sizeof(unsigned int)));

	if (rc == -1) {
              fprintf(stderr, "err: Cannot write to %s: %s\n",
            		  fname_out, strerror(errno));
              fclose(fp);
              return -EIO;
	}

	for (i = 0; i < len/sizeof(unsigned int); i++) {
		rc = fprintf(fp, "%u\n", buff[i]);
		//rc = fwrite(buff, len, 1, fp);
		if (rc == -1) {
			fprintf(stderr, "err: Cannot write to %s: %s\n",
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

void copyImgToBw (float *ibuff, float *ibuff_bw, unsigned int numberOfColumns);
void copyImgToBw (float *ibuff, float *ibuff_bw, unsigned int numberOfColumns) {


	//for(unsigned int col = 0; col < numberOfColumns; col++)
	//	memcpy(ibuff_bw+col*HIGHT_IN_PIX, ibuff+(numberOfColumns - col - 1)*HIGHT_IN_PIX, HIGHT_IN_PIX*sizeof(float));

	for(unsigned int col = 0; col < numberOfColumns; col++) {
		for(unsigned int row = 0; row < HIGHT_IN_PIX; row++) {
			ibuff_bw[col * HIGHT_IN_PIX + row] = ibuff[(numberOfColumns - col - 1) * HIGHT_IN_PIX + (row)];
			/* printf("DEBUG: ibuff[%d]=%f, ibuff_bw[%d]=%f\n", col * HIGHT_IN_PIX + row, ibuff[col * HIGHT_IN_PIX + row], \
														     col * HIGHT_IN_PIX + row, ibuff_bw[col * HIGHT_IN_PIX + row]);
			*/
		}
	}
}




int main(int argc, char *argv[])
{
	int ch, rc = 0, col;
	int card_no = 0;
	struct snap_card *card = NULL;
	struct snap_action *action = NULL;
	char device[128];
	struct snap_job cjob;
	struct blstm_job mjob;
	const char *input_img_dir = NULL;
	//const char *input_grt_dir = NULL;
	const char *output = NULL;
	unsigned long timeout = 600;
	const char *space = "CARD_RAM";
	struct timeval etime[MAX_NUMBER_IMAGES_TEST_SET], stime[MAX_NUMBER_IMAGES_TEST_SET];
	ssize_t size, size_in, size_out;
	size = size_in = size_out = 1024 * 1024;
	float *ibuff = NULL;
	float *ibuff_bw = NULL;
	float *tmpibuff = NULL;
	unsigned int *obuff = NULL;
	char filenames[MAX_NUMBER_IMAGES_TEST_SET][NAME_BUFF];
	char input_img_fil[NAME_BUFF];
	unsigned int filenum = 0;
	DIR *dir;
	struct dirent *ent;
	uint8_t type_in = SNAP_ADDRTYPE_HOST_DRAM;
	uint64_t addr_in = 0x0ull;
	uint8_t type_out = SNAP_ADDRTYPE_HOST_DRAM;
	uint64_t addr_out = 0x0ull;
	int verify = 0;
	int exit_code = EXIT_SUCCESS;
	uint8_t trailing_zeros[1024] = { 0, };
	snap_action_flag_t action_irq = (SNAP_ACTION_DONE_IRQ | SNAP_ATTACH_IRQ);

	while (1) {
		int option_index = 0;
		static struct option long_options[] = {
			{ "card",	 required_argument, NULL, 'C' },
			{ "input_img_dir",	 required_argument, NULL, 'i' },
			{ "input_grt_dir",	 no_argument, NULL, 'g' },
			{ "output",	 required_argument, NULL, 'o' },
			{ "src-type",	 required_argument, NULL, 'A' },
			{ "src-addr",	 required_argument, NULL, 'a' },
			{ "dst-type",	 required_argument, NULL, 'D' },
			{ "dst-addr",	 required_argument, NULL, 'd' },
			{ "size",	 required_argument, NULL, 's' },
			{ "timeout",	 required_argument, NULL, 't' },
			{ "verify",	 no_argument,	    NULL, 'X' },
			{ "no-irq",	 no_argument,	    NULL, 'N' },
			{ "version",	 no_argument,	    NULL, 'V' },
			{ "verbose",	 no_argument,	    NULL, 'v' },
			{ "help",	 no_argument,	    NULL, 'h' },
			{ 0,		 no_argument,	    NULL, 0   },
		};

		ch = getopt_long(argc, argv,
				 "A:C:i:o:a:S:D:d:x:s:t:XVNvh",
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
		//case 'g':
		//	input_grt_dir = optarg;
		//	break;
		case 'o':
			output = optarg;
			break;
		case 's':
			size = __str_to_num(optarg);
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
			action_irq = 0;
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




	/* if input file is defined, use that as input */
	if (input_img_dir != NULL) {

		if ((dir = opendir (input_img_dir)) != NULL) {
		  /* print all the files and directories within directory */
		  while ((ent = readdir (dir)) != NULL) {
			  if ((strncmp(ent->d_name, ".", 1) == 0) || (strncmp(ent->d_name, "..", 2) == 0))
			  	  continue;

			if (strlen(input_img_dir) + strlen(ent->d_name) < NAME_BUFF) {
				if (filenum < MAX_NUMBER_IMAGES_TEST_SET) {
					sprintf(input_img_fil, "%s/%s", input_img_dir, ent->d_name);
					printf ("Storing filename %s\n", input_img_fil);
					strcpy(filenames[filenum++], input_img_fil);
				}
				else
					printf("Warning: Skipping file %s : Reached maximum number of files (MAX_NUMBER_IMAGES_TEST_SET = %d)\n", \
							input_img_fil, (unsigned int)MAX_NUMBER_IMAGES_TEST_SET);
			}
		    else
		    	printf("Warning: Skipping file %s : larger filename (%u) than maximum allowed (%d)\n", \
		    			input_img_fil, (unsigned int)(strlen(ent->d_name) + strlen(ent->d_name)), NAME_BUFF-1);
		  }
		  closedir (dir);
		} else {
		  /* could not open directory */
		  perror ("");
		  return EXIT_FAILURE;
		}
	}
	else {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}



	/* if output file is defined, use that as output */
	if (output != NULL) {

		/* Erase file, since we append results later */
		FILE *fp = fopen(output, "w");
        if (!fp) {
                fprintf(stderr, "err: Cannot open file %s for writing: %s\n",
                		output, strerror(errno));
                return -ENODEV;
        }
        fclose(fp);

		size_out = MAX_NUMBER_COLUMNS_TEST_SET * sizeof(unsigned int);
		size_t set_size = size_out + (verify ? sizeof(trailing_zeros) : 0);

		obuff = snap_malloc(set_size);
		if (obuff == NULL)
			goto out_error;
		memset(obuff, 0x0, set_size);
		type_out = SNAP_ADDRTYPE_HOST_DRAM;
		addr_out = (unsigned long)obuff;
	}
	else {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

		/* Loop over all files  */
		for (unsigned int i = 0; i < filenum; i++) {


			size_in = MAX_NUMBER_COLUMNS_TEST_SET * HIGHT_IN_PIX * sizeof(float); //__file_size(input_img_dir);
			if (size_in < 0)
				goto out_error;

			/* source buffer */
			ibuff = snap_malloc(size_in);
			ibuff_bw = snap_malloc(size_in);
			if (ibuff == NULL)
				goto out_error;
			memset(ibuff, 0x0, size_in);
			memset(ibuff_bw, 0x0, size_in);

			rc = file_read(filenames[i], ibuff, size_in);
			if (rc < 0)
				goto out_error;
			else
				col = rc/HIGHT_IN_PIX;

			/* Create image_bw buffer using image_fw */
			copyImgToBw(ibuff, ibuff_bw, rc/HIGHT_IN_PIX);

			/* Reallocating ibuff to actual size */
			size_in = 2*rc*sizeof(float); // FIXME: fw and bw
			tmpibuff = snap_malloc(size_in);
			if (tmpibuff == NULL)
				goto out_error;
			memset(tmpibuff, 0x0, size_in);

			/* Creating a single buffer with image_fw followed by image_bw */
			memcpy(tmpibuff, ibuff, size_in/2);
			memcpy(tmpibuff+rc, ibuff_bw, size_in/2);
			__free(ibuff);
			__free(ibuff_bw);
			ibuff = tmpibuff;
			//for (int i = 0; i < 2*rc; i++) fprintf(stdout, "DEBUG: ibuff[%d]=%f\n", i, ibuff[i]);

			fprintf(stdout, "INFO: reading input data %d float numbers stored in %d bytes from %s\n",
					rc, (int)size_in/2, input_img_fil);

			type_in = SNAP_ADDRTYPE_HOST_DRAM;
			addr_in = (unsigned long)ibuff;

/*
		const char *alphabet_file = "/home/diaman/projects/snap/actions/hls_blstm/data/alphabet/alphabet.txt";
		char alphabet[NUMBER_OF_CLASSES-1];
		rc = alphabet_read(alphabet_file, alphabet, NUMBER_OF_CLASSES-1);
		if (rc < 0)
			goto out_error;
*/

			printf("PARAMETERS:\n"
					"  input:       %s\n"
					"  output:      %s\n"
					"  type_in:     %x %s\n"
					"  addr_in:     %016llx\n"
					"  type_out:    %x %s\n"
					"  addr_out:    %016llx\n"
					"  size_in:     %08lx\n"
					"  size_out:    %08lx\n"
					"  col:			%x\n",
					filenames[i], output ? output : "unknown",
							type_in,  mem_tab[type_in],  (long long)addr_in,
							type_out, mem_tab[type_out], (long long)addr_out,
							size_in, size_out, col);

			snprintf(device, sizeof(device)-1, "/dev/cxl/afu%d.0s", card_no);
			card = snap_card_alloc_dev(device, SNAP_VENDOR_ID_IBM,
				   SNAP_DEVICE_ID_SNAP);
			if (card == NULL) {
				fprintf(stderr, "err: failed to open card %u: %s\n",
						card_no, strerror(errno));
				goto out_error;
			}

			action = snap_attach_action(card, BLSTM_ACTION_TYPE, action_irq, 60);
			if (action == NULL) {
				fprintf(stderr, "err: failed to attach action %u: %s\n",
						card_no, strerror(errno));
				goto out_error1;
			}

			snap_prepare_blstm(&cjob, &mjob,
					(void *)addr_in,  size_in, type_in,
					(void *)addr_out, size_out, type_out);

			/* Write on MMIO register the number of columns of current image */
			memset(mjob.imgcols.cols, 0, sizeof(mjob.imgcols));

			/* Write on MMIO register the number of columns of current image */
			memset(mjob.imgstrlen.cols, 0, sizeof(mjob.imgcols));

			mjob.imgcols.cols[0] = col;
			mjob.imgcols.cols[1] = 0xff;
			mjob.imgcols.cols[2] = 0x10;
			mjob.imgcols.cols[3] = 0x20;
			mjob.imgcols.cols[4] = 0x30;
			mjob.imgcols.cols[5] = 0x40;
			mjob.imgcols.cols[6] = 0x50;
			mjob.imgcols.cols[7] = 0xaa;

			__hexdump(stderr, &mjob, sizeof(mjob));

			gettimeofday(&stime[i], NULL);
			rc = snap_action_sync_execute_job(action, &cjob, timeout);
			gettimeofday(&etime[i], NULL);
			if (rc != 0) {
				fprintf(stderr, "err: job execution %d: %s!\n", rc,
						strerror(errno));
				goto out_error2;
			}

			fprintf(stdout, "INFO: Accelerator returned code on MMIO (AXILite job struct field) : %u\n", mjob.status );

			fprintf(stdout, "INFO: AXI transaction registered on MMIO : In: %u(0x%x), Out: %u(0x%x)  ", \
					mjob.axitrans_in, mjob.axitrans_in, mjob.axitrans_out, mjob.axitrans_out);

			//rc = snap_mmio_read32(card, 0x1B0, &tmp); // strlen is registered at 0x1B0
			//fprintf(stdout, "INFO: Found %u characters on MMIO (AXILite) reg 0x1B0\n", tmp);
			fprintf(stdout, "INFO: Found %u characters on MMIO (AXILite job struct field)\n", mjob.strlen);

			__hexdump(stderr, &mjob, sizeof(mjob));

			//if (rc != SNAP_OK)
			//	fprintf(stderr, "err: Unexpected snap_mmio_read32 returned value %d!\n", rc);

			/* Reassign size_out to the actual number of characters found (each character is integer -> index of alphabet) */
			size_out = mjob.strlen * sizeof(unsigned int);

			/* If the output buffer is in host DRAM we can write it to a file */
			if (output != NULL) {
				fprintf(stdout, "INFO: writing output data %p %d bytes to %s\n",
						obuff, (int)size_out, output);

				rc = file_write(output, filenames[i], obuff, size_out);
				if (rc != (int)(size_out/sizeof(unsigned int)))
					goto out_error2;
			}

			fprintf(stdout, "INFO: RETC=%x\n", cjob.retc);
			if (cjob.retc != SNAP_RETC_SUCCESS) {
				fprintf(stderr, "err: Unexpected RETC=%x!\n", cjob.retc);
				goto out_error2;
			}

			if (verify) {
				if ((type_in  == SNAP_ADDRTYPE_HOST_DRAM) &&
						(type_out == SNAP_ADDRTYPE_HOST_DRAM)) {
					rc = memcmp(ibuff, obuff, size);
					if (rc != 0)
						exit_code = EX_ERR_VERIFY;

					rc = memcmp(obuff + size, trailing_zeros, 1024);
					if (rc != 0) {
						fprintf(stderr, "err: trailing zero "
								"verification failed!\n");
						__hexdump(stderr, obuff + size, 1024);
						exit_code = EX_ERR_VERIFY;
					}

				} else
					fprintf(stderr, "warn: Verification works currently "
							"only with HOST_DRAM\n");
			}



			fprintf(stdout, "INFO: SNAP run %u blstm took %lld usec\n",
					i, (long long)timediff_usec(&etime[i], &stime[i]));

			snap_detach_action(action);
			snap_card_free(card);

			__free(ibuff);

	} /* for loop over files */



	__free(obuff);

	exit(exit_code);

 out_error2:
	snap_detach_action(action);
 out_error1:
	snap_card_free(card);
 out_error:
	__free(obuff);
	__free(ibuff);
	exit(EXIT_FAILURE);
}
