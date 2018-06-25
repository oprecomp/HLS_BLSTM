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
 * @file generate_luts.h
 * @author Dionysios Diamantopoulos, did@zurich.ibm.com
 * @date 3 Oct 2017
 * @brief The header file of generator for Look-up-Tables for activation functions.
 * */


#ifndef GENERATE_LUTS_HPP
#define GENERATE_LUTS_HPP

#include "../include/common_def.h"

/*!
 * \def MIN_TARGET_DIVEXPF
 * The minimum value of divexpf() function.
 * */
#define MIN_TARGET_DIVEXPF -23

/*!
 * \def MAX_TARGET_DIVEXPF
 * The maximum value of divexpf() function.
 * */
#define MAX_TARGET_DIVEXPF 24

/*!
 * \def LUT_SIZE_DIVEXPF
 * The size in values of divexpf() function.
 * */
#define LUT_SIZE_DIVEXPF 256

/*!
 * \def MIN_TARGET_TANH
 * The minimum value of tanh() function.
 * */
#define MIN_TARGET_TANH -10

/*!
 * \def MAX_TARGET_TANH
 * The maximum value of tanh() function.
* */
#define MAX_TARGET_TANH 10

/*!
 * \def LUT_SIZE_TANH
 * The size in values of tanh() function.
 * */
#define LUT_SIZE_TANH 256

/*!
 * \def MIN_TARGET_EXPF
 * The minimum value of exp() function.
 * */
#define MIN_TARGET_EXPF -5

/*!
 * \def MAX_TARGET_EXPF
 * The maximum value of exp() function.
 * */
#define MAX_TARGET_EXPF 25

/*!
 * \def LUT_SIZE_EXPF
 * The size in values of expf() function.
  * */
#define LUT_SIZE_EXPF 256

/*!
 * \def PWL_SIZE_EXPF
 * The piecewise linear step size of exp() function.
 * */
#define PWL_SIZE_EXPF 10

#endif
