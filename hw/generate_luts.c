/****************************************************************************
   Copyright 2018 - The OPRECOMP Project Consortium,
                    IBM Research GmbH. All rights reserved.

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
 * @file generate_luts.c
 * @author Dionysios Diamantopoulos, did@zurich.ibm.com
 * @date 3 Oct 2017
 * @brief The generator for Look-up-Tables for activation functions.
 * */


#include <stdio.h>
#include <math.h>
#include <assert.h>

#include "generate_luts.h"

int main(int argc, const char* argv[])
{

  FILE *fp;
  char *filen = "./lut.h";
  if (!(fp = fopen (filen, "w")))
    perror("Failed to create file") ;

  assert(MAX_TARGET_EXPF>MIN_TARGET_EXPF);
  assert(MAX_TARGET_DIVEXPF>MIN_TARGET_DIVEXPF);
  assert(MAX_TARGET_TANH>MIN_TARGET_TANH);

  unsigned int i;
  double step, x, x1, x2, y, y1, y2;

#if TRIGF_APPROX <= 2

  // DIVEXPF LUT
  fprintf(fp, "\n/* Look-up table for the approximation of 1.0/(1.0 + expf(-x)) */\n");

  fprintf(fp, "\n#ifndef LUT_H\n#define LUT_H\n");

  fprintf(fp, "\n/* Min:%f, Max:%f, Size:%d */\n", (float)MIN_TARGET_DIVEXPF, (float)MAX_TARGET_DIVEXPF, (int)LUT_SIZE_DIVEXPF);
  fprintf(fp, " static DTYPE_IN divexpf_lut[LUT_SIZE_DIVEXPF] = {");
  step = (fabs(MAX_TARGET_DIVEXPF)+fabs(MIN_TARGET_DIVEXPF))/(LUT_SIZE_DIVEXPF-1);
  for(i = 0; i < LUT_SIZE_DIVEXPF; i++) {
    x = MIN_TARGET_DIVEXPF+i*step;
    y =  1.0/(1.0 + expf(-x));
    //printf("DEBUG: %d: x=%.10lf, y=%.10lf\n", i, x, y);
    fprintf(fp, "%.15f", (float)y);
    if (i != LUT_SIZE_DIVEXPF-1)
      fprintf(fp, ", ");
    else
      fprintf(fp, "};\n");
  }
  //printf("DEBUG: step for divexpf : %.10lf\n", step);

  // TANH LUT
  fprintf(fp, "\n\n/* Look-up table for the approximation of tanhf(x) */\n");
  fprintf(fp, "\n/* Min:%f, Max:%f, Size:%d */\n", (float)MIN_TARGET_TANH, (float)MAX_TARGET_TANH, (int)LUT_SIZE_TANH);
  fprintf(fp, " static DTYPE_IN tanh_lut[LUT_SIZE_TANH] = {");
  step = (fabs(MAX_TARGET_TANH)+fabs(MIN_TARGET_TANH))/(LUT_SIZE_TANH-1);
  for(i = 0; i < LUT_SIZE_TANH; i++) {
    x = MIN_TARGET_TANH+i*step;
    y = tanhf(x);
    //printf("DEBUG: %d: x=%.10lf, y=%.10lf\n", i, x, y);
    fprintf(fp, "%.15f", (float)y);
    if (i != LUT_SIZE_TANH-1)
      fprintf(fp, ", ");
    else
      fprintf(fp, "};\n");
  }
  //printf("DEBUG: step for expf : %.10lf\n", step);

  // EXPF LUT
  fprintf(fp, "\n/* Look-up table for the approximation of expf(x)) */\n");
  fprintf(fp, "\n/* Min:%f, Max:%f, Size:%d */\n", (float)MIN_TARGET_EXPF, (float)MAX_TARGET_EXPF, (int)LUT_SIZE_EXPF);
  fprintf(fp, " static DTYPE_OUTPUT expf_lut[LUT_SIZE_EXPF] = {");
  step = (fabs(MAX_TARGET_EXPF)+fabs(MIN_TARGET_EXPF))/(LUT_SIZE_EXPF-1);
  for(i = 0; i < LUT_SIZE_EXPF; i++) {
    x = MIN_TARGET_EXPF+i*step;
    y =  expf(x);
    //printf("DEBUG: %d: x=%.10lf, y=%.10lf\n", i, x, y);
    fprintf(fp, "%.15f", (float)y);
    if (i != LUT_SIZE_EXPF-1)
      fprintf(fp, ", ");
    else
      fprintf(fp, "};\n");
  }
  //printf("DEBUG: step for expf : %.10lf\n", step);

  fprintf(fp, "\n#endif\n");

#elif TRIGF_APPROX == 3

  step = (fabs(MAX_TARGET_EXPF)+fabs(MIN_TARGET_EXPF))/(PWL_SIZE_EXPF-1);
  fprintf(fp, "\n/* Piecewise linear approximation of expf(x)) */\n");
  fprintf(fp, "\n/* Min:%f, Max:%f, Size:%d, Step:%f */\n", (float)MIN_TARGET_EXPF, (float)MAX_TARGET_EXPF, (int)PWL_SIZE_EXPF, step);
  fprintf(fp, "inline DTYPE_OUTPUT expf_pwl(DTYPE_OUTPUT x) {\n\tDTYPE_OUTPUT ret;\n\n");
  fprintf(fp, "#pragma HLS INLINE off\n");
  fprintf(fp, "const DTYPE_OUTPUT step = 1/((fabs(MAX_TARGET_EXPF)+fabs(MIN_TARGET_EXPF)) / (PWL_SIZE_EXPF));\n");
  fprintf(fp, "int select = int(((x-(MIN_TARGET_EXPF))*step));\n");
  fprintf(fp, "switch(select)\n\t{\n");

  for(i = 0; i <= PWL_SIZE_EXPF; i++) {
    x1 = MIN_TARGET_EXPF+(i-1)*step;
    x2 = MIN_TARGET_EXPF+i*step;
    y1 =  expf(x1);
    y2 =  expf(x2);
    fprintf(fp, "\tcase %d: ret = (x * (DTYPE_OUTPUT)%.15f); break;\n", i, (float)y2/x2); //(y2-y1)/(x2-x1)
  }
  fprintf(fp, "\tdefault : ret = (x * (DTYPE_OUTPUT)%.15f); break;\n", (float)y2/x2); //(y2-y1)/(x2-x1)
  fprintf(fp, "}\nif (ret > 0)\n\treturn ret;\nelse\n\treturn -(ret);\n}\n");

#endif




  if (fp!=NULL)
	  fclose(fp);

	return 0;
}
