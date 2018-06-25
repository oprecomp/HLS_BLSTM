##############################################################################
#   Copyright 2018 - The OPRECOMP Project Consortium,
#                    IBM Research GmbH. All rights reserved.
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
##############################################################################

# @file Makefile
# @author Dionysios Diamantopoulos, did@zurich.ibm.com
# @date 7 Oct 2017
# @brief Makefile for building the HW-action of hls_blstm.

#
# Generate HDL version of the HLS sources
#
# The generated HDL depends on the chip which is used and
# therefore must match what is being used to build the
# toplevel SNAP bitstream.
#
# FIXME Pass part_number and other parameters from toplevel
#      build-system as required.
#

# Finding $SNAP_ROOT
ifndef SNAP_ROOT
# check if we are in hw folder of an action (three directories below snap root)
ifneq ("$(wildcard ../../../ActionTypes.md)","")
SNAP_ROOT=$(abspath ../../../)
else
$(info You are not building your software from the default directory (/path/to/snap/actions/<action_name>/sw) or specified a wrong $$SNAP_ROOT.)
$(error Please make sure that $$SNAP_ROOT is set up correctly.)
endif
endif

# This is solution specific. Check if we can replace this by generics too.
SOLUTION_NAME ?= blstm
SOLUTION_DIR ?= hlsBLSTM
srcs += action_hls.cpp
srcs += neuron.cpp
srcs += action_blstm_tb.cpp
#cflags += -std=c++0x -DVHLS_TB
TARGET_CLOCK ?= 2.7

all: luts

# If you have the action code outside of the default snap directory structure,
# change to /path/to/snap/actions/hls.mk
include $(SNAP_ROOT)/actions/hls.mk

luts:
	gcc -o generate_luts generate_luts.c -lm
	./generate_luts
