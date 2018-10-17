#
# Copyright 2017 International Business Machines
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

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

all: clean dirs copy_snap_files copy_vivado_files

dirs: 
	mkdir -p ../sw/third-party
	mkdir -p ../sw/third-party/xilinx

copy_snap_files:
	cp -f ./ActionTypes.md ${SNAP_ROOT}/ActionTypes.md
	cp -f ./hls.mk ${SNAP_ROOT}/actions/hls.mk
	cp -f ./Kconfig ${SNAP_ROOT}/scripts/Kconfig
	cp -f ./snap_env ${SNAP_ROOT}/snap_env

copy_vivado_files:
	cp -f ${XILINX_VIVADO}/include/ap_int.h ../sw/third-party/xilinx/ | true
	cp -f ${XILINX_VIVADO}/include/ap_int_sim.h ../sw/third-party/xilinx/ | true
	cp -f ${XILINX_VIVADO}/include/ap_common.h ../sw/third-party/xilinx/ | true
	cp -f ${XILINX_VIVADO}/include/ap_int_special.h ../sw/third-party/xilinx/ | true
	cp -f ${XILINX_VIVADO}/include/floating_point_v7_0_bitacc_cmodel.h ../sw/third-party/xilinx/ | true
	cp -f ${XILINX_VIVADO}/include/gmp.h ../sw/third-party/xilinx/ | true
	cp -f ${XILINX_VIVADO}/include/hls_fpo.h ../sw/third-party/xilinx/ | true
	cp -f ${XILINX_VIVADO}/include/hls_half.h ../sw/third-party/xilinx/ | true
	cp -f ${XILINX_VIVADO}/include/mpfr.h ../sw/third-party/xilinx/ | true
	cp -f ${XILINX_VIVADO}/include/ap_int.h ../sw/third-party/xilinx/ | true
	cp -f ${XILINX_VIVADO}/include/ap_int.h ../sw/third-party/xilinx/ | true
	cp -f -r ${XILINX_VIVADO}/include/etc/ ../sw/third-party/xilinx/ | true

clean:
