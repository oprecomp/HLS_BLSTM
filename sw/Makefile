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
# @date 15 Oct 2017
# @brief Makefile for building the SW-action of hls_blstm.


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
# check if we are in sw folder of an action (three directories below snap root)
ifneq ("$(wildcard ../../../ActionTypes.md)","")
SNAP_ROOT=$(abspath ../../../)
else
$(info You are not building your software from the default directory (/path/to/snap/actions/<action_name>/sw) or specified a wrong $$SNAP_ROOT.)
$(error Please make sure that $$SNAP_ROOT is set up correctly.)
endif
endif

# This is solution specific. Check if we can replace this by generics too.

#snap_blstm: action_lowercase.o
#snap_blstm_objs = action_lowercase.o

#projs += snap_blstm

SNAP_CONFIG?=CPU

CFLAGS += "-fopenmp"
CXXFLAGS += "-fopenmp"

ifeq ($(SNAP_CONFIG),CPU)
LIBCXL=$(PSLSE_ROOT)/libcxl/libcxl.a
$(info Simulation solution: linking with $(LIBCXL))
else
LIBCXL="-lcxl"
$(info Hardware solution : linking with $(LIBCXL))
endif

# dirty way of compiling C++ code of top snap sw. Have to found a seamless intergration to snap building process
all: action_blstm_cpu.o neuron.o
	rm -f snap_blstm
	$(CXX) -W -Wall -Wno-unused-parameter -fpermissive -fopenmp -Wwrite-strings -std=c++0x -Wextra -O2 -g -DGIT_VERSION=\"$(git --version | awk '{print $3}')\" -I$(SNAP_ROOT)/software/include -I../include -I./third-party/xilinx/ -o snap_blstm neuron.o action_blstm_cpu.o snap_blstm.cpp $(SNAP_ROOT)/software/lib/libsnap.a $(LIBCXL)  -lpthread



# If you have the host code outside of the default snap directory structure,
# change to /path/to/snap/actions/software.mk
include $(SNAP_ROOT)/actions/software.mk
