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
# @brief Makefile for building SW and HW acion of hls_blstm.


subdirs += snap_modification_files sw hw

all: $(subdirs)

# Only build if the subdirectory is existent and if Makefile is there
.PHONY: $(subdirs)
$(subdirs):
	@if [ -d $@ -a -f $@/Makefile ]; then			\
		$(MAKE) -C $@ || exit 1;			\
	else							\
		echo "INFO: No Makefile available in $@ ...";	\
	fi

# Run Doxygen documentation
dox:
	doxygen ./doc/dox.config

# Cleanup for all subdirectories.
# Only dive into subdirectory if existent and if Makefile is there.
clean:
	@for dir in $(subdirs); do	\
		if [ -d $$dir -a -f $$dir/Makefile ]; then	\
			$(MAKE) -C $$dir $@ || exit 1;		\
		fi						\
	done
	@find . -depth -name '*~'  -exec rm -rf '{}' \; -print
	@find . -depth -name '.#*' -exec rm -rf '{}' \; -print
