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

# @file evaluate_solution_bitacc.py
# @author Dionysios Diamantopoulos, did@zurich.ibm.com
# @date 15 Oct 2017
# @brief Script for fixed-point exploration for the BLSTM mb Uses the fixed-point
# library of Xilinx Vivado HLS. For a combination of fixed-point format,
# collects accuracy and synthesis results and plots them

import os
import subprocess
import numpy as np
import os.path
import fileinput
from shutil import copyfile

LOAD_SESSION = 1

PLOT_ENABLE = 1

savesessionfile = 'globalsave.npy'

if (LOAD_SESSION == 1):
    #dill.load_session(filename)
    [labels, confMat, mat_acc_min, mat_acc_max, [total_bits, frac_bits], mat_brams, mat_dsps, mat_ffs, mat_luts] = np.load(savesessionfile)

else:
    RUN_SIM = 1
    RUN_VHLS = 0

    filein = "./include/common_def.h"
    fileout = "./include/common_def_replaced.h"
    log_file_path = "/tmp/run.log"
    syn_log = "./hw/hlsBLSTM_xcku060-ffva1156-2-e/blstm/syn/report/Single_Kernel_BLSTM_csynth.rpt"
    f = open(filein,'r')
    filedata = f.read()
    f.close()

    if (RUN_SIM == 1):
        logfd = open("./run.log",'w')

        TRIGF_APROX_list = [2]
        DTYPE_int_min = 7
        DTYPE_int_max = 24
        DTYPE_int_step = 1
        DTYPE_fra_min = 5
        DTYPE_fra_max = 20

        #sstring_DTYPE_IMG = "#define DTYPE_IMG"
        #rstring_DTYPE_IMG = "#define DTYPE_IMG ap_fixed<"

        #sstring_DTYPE_IMG = "#define DTYPE_WEIGHTS"
        #rstring_DTYPE_IMG = "#define DTYPE_WEIGHTS ap_fixed<"

        #sstring_DTYPE_IMG = "#define DTYPE_IN"
        #rstring_DTYPE_IMG = "#define DTYPE_IN ap_fixed<"

        sstring_DTYPE_IMG = "#define DTYPE_OUTPUT"
        rstring_DTYPE_IMG = "#define DTYPE_OUTPUT ap_fixed<"

        #sstring_DTYPE_IMG = "#define DTYPE_LAYERS"
        #rstring_DTYPE_IMG = "#define DTYPE_LAYERS ap_fixed<"

        #sstring_DTYPE_IMG = "#define DTYPE_TRNLB"
        #rstring_DTYPE_IMG = "#define DTYPE_TRNLB ap_fixed<"


        #sstring_DTYPE_IMG = "#define COLS_PER_KERNEL_EXEC"
        #rstring_DTYPE_IMG = "#define COLS_PER_KERNEL_EXEC "

        for tr in TRIGF_APROX_list:
            search_string = "TRIGF_APPROX"
            replace_string = "TRIGF_APPROX " + str(tr) + " //"
            newdata = filedata.replace(search_string, replace_string)
            f = open(fileout,'w')
            f.write(newdata)
            f.close()
            confMat     = np.empty(shape=[0, DTYPE_fra_max-DTYPE_fra_min+1])
            mat_acc_min = np.empty(shape=[0, DTYPE_fra_max-DTYPE_fra_min+1])
            mat_acc_max = np.empty(shape=[0, DTYPE_fra_max-DTYPE_fra_min+1])
            mat_brams   = np.empty(shape=[0, DTYPE_fra_max-DTYPE_fra_min+1])
            mat_dsps    = np.empty(shape=[0, DTYPE_fra_max-DTYPE_fra_min+1])
            mat_ffs     = np.empty(shape=[0, DTYPE_fra_max-DTYPE_fra_min+1])
            mat_luts    = np.empty(shape=[0, DTYPE_fra_max-DTYPE_fra_min+1])
            labels      = np.empty(shape=[0,  DTYPE_fra_max-DTYPE_fra_min+1])
            bits_vec=[]
            for img_b in range(DTYPE_int_min, DTYPE_int_max+1, DTYPE_int_step):
                col_accuracy = []
                col_accuracy_min = []
                col_accuracy_max = []
                col_brams = []
                col_dsps  = []
                col_ffs   = []
                col_luts  = []
                cols_labels = []
                bits_vec.append(img_b)
                for img_f in range(DTYPE_fra_min, min(img_b+1, DTYPE_fra_max+1)):
                    replace_string = rstring_DTYPE_IMG + str(img_b)+","+str(img_f) + ">\n" # form bitwidth
                    #replace_string = rstring_DTYPE_IMG + str(img_b) + "\n" #for segmentation
                    fout = open(fileout,'w')
                    for line in fileinput.input(filein):
                        if sstring_DTYPE_IMG in line:
                            line = replace_string
                        fout.write(line)
                    fout.close()
                    copyfile(fileout, filein)
                    if (RUN_SIM == 1):
                        command = "cd hw/ && ./compile_gnu.sh && ./main > " + log_file_path + " && cd ../"
                        subprocess.getoutput(command)
                        if (not os.path.exists(log_file_path)):
                            print("log file " + log_file_path + " does not exist, skipping round")
                            continue
                        command = "cat " + log_file_path + " | grep Accuracy | tail -n 1 | awk '{print $2}'"
                        accuracy_min = subprocess.getoutput(command)
                        command = "cat " + log_file_path + " | grep Accuracy | tail -n 1 | awk '{print $3}'"
                        accuracy_max = subprocess.getoutput(command)
                        command = "cat " + log_file_path + " | grep Accuracy | tail -n 1 | awk '{print $4}' | sed 's/.$//'"
                        accuracy = subprocess.getoutput(command)
                        command = "cat " + log_file_path + " | grep Measured | awk '{print $4}'"
                        time = subprocess.getoutput(command)

                    if (RUN_VHLS == 1):
                        command = "cd hw/ && vivado_hls -f run_hls_script.tcl && cd ../"
                        subprocess.getoutput(command)
                        command = "cat " + syn_log + "  | grep -A 3 Latency | tail -n 6 | head -n 1  | awk '{print $3}' | sed 's/.$//'"
                        latency_min = subprocess.getoutput(command)
                        command = "cat " + syn_log + "  | grep -A 3 Latency | tail -n 6 | head -n 1  | awk '{print $4}' | sed 's/.$//'"
                        latency_max = subprocess.getoutput(command)
                        command = "cat " + syn_log + "  | grep -A 3 Total | head -n 1 | awk '{print $3}' | sed 's/.$//'"
                        brams = subprocess.getoutput(command)
                        command = "cat " + syn_log + "  | grep -A 3 Total | head -n 1 | awk '{print $4}' | sed 's/.$//'"
                        dsps = subprocess.getoutput(command)
                        command = "cat " + syn_log + "  | grep -A 3 Total | head -n 1 | awk '{print $5}' | sed 's/.$//'"
                        ffs = subprocess.getoutput(command)
                        command = "cat " + syn_log + "  | grep -A 3 Total | head -n 1 | awk '{print $6}' | sed 's/.$//'"
                        luts = subprocess.getoutput(command)
                    else:
                        latency_min='0'; latency_max='0'; brams='0'; dsps='0'; ffs='0'; luts='0';
                    line = "TRIGF_APROX=" + str(tr), str(img_b)+"."+str(img_f), time, accuracy_min, accuracy_max, accuracy, latency_min, latency_max, brams, dsps, ffs, luts
                    print(', '.join(line))
                    if (RUN_SIM == 1):
                        logfd.write(', '.join(line)+"\n")
                    col_accuracy = np.append(col_accuracy, float(accuracy))
                    col_accuracy_min = np.append(col_accuracy_min, float(accuracy_min))
                    col_accuracy_max = np.append(col_accuracy_max, float(accuracy_max))
                    col_brams = np.append(col_brams, float(brams))
                    col_dsps = np.append(col_dsps, float(dsps))
                    col_ffs = np.append(col_ffs, float(ffs))
                    col_luts = np.append(col_luts, float(luts))
                    text = "Q%d.%d\n%.2f" % (img_f, img_b-img_f, float(accuracy))
                    cols_labels = np.append(cols_labels, text )
                for i in range(max(img_b+1, DTYPE_fra_min), DTYPE_fra_max+1):
                    col_accuracy = np.append(col_accuracy, float(0))
                    col_accuracy_min = np.append(col_accuracy_min, float(0))
                    col_accuracy_max = np.append(col_accuracy_max, float(0))
                    col_brams = np.append(col_brams, float(0))
                    col_dsps = np.append(col_dsps, float(0))
                    col_ffs = np.append(col_ffs, float(0))
                    col_luts = np.append(col_luts, float(0))
                    cols_labels = np.append(cols_labels, "N/A" )
                confMat = np.append(confMat, [col_accuracy], axis=0)
                mat_acc_min = np.append(mat_acc_min, [col_accuracy_min], axis=0)
                mat_acc_max = np.append(mat_acc_max, [col_accuracy_max], axis=0)
                mat_brams = np.append(mat_brams, [col_brams], axis=0)
                mat_dsps = np.append(mat_dsps, [col_dsps], axis=0)
                mat_ffs = np.append(mat_ffs, [col_ffs], axis=0)
                mat_luts = np.append(mat_luts, [col_luts], axis=0)
                labels = np.append(labels, [cols_labels], axis=0)


    if (RUN_VHLS == 1):
        logfd.close()


    total_bits = np.arange(DTYPE_int_min, DTYPE_int_max+1, DTYPE_int_step)
    frac_bits = np.arange(DTYPE_fra_min, DTYPE_fra_max+1)

    # Saving the objects:
    if (LOAD_SESSION == 0):
        np.save(savesessionfile,  [labels, confMat, mat_acc_min, mat_acc_max, [total_bits, frac_bits], mat_brams, mat_dsps, mat_ffs, mat_luts])


if PLOT_ENABLE == 1:
    import seaborn as sns
    import matplotlib.pyplot as plt
    from mpl_toolkits.mplot3d import Axes3D

    # Plot for segmentation eperimentation
    #x = total_bits
    #y = confMat[:,0]
    #yerr = [y-mat_acc_min[:,0], mat_acc_max[:,0]-y]

    #fig = plt.figure()
    #plt.axvline(x=22, color='m', linestyle='--', label="Max columns for PULP cache")
    #plt.axvline(x=732, color='c', linestyle='--', label="Max columns on dataset")
    #plt.axhline(y=100, color='g', linestyle='--', label="Groundtruth Max Accuracy")
    #plt.axhline(y=98.2337, color='r', linestyle='--', label="Groundtruth Average Accuracy")
    #plt.axhline(y=57.8947, color='b', linestyle='--', label="Groundtruth Min Accuracy")
    #plt.errorbar(x, y, yerr=yerr, fmt='--o', label="Measured Accuracy (Min-Avg-Max)")
    #fig.suptitle('Columns spliting impact on contemporal information lost ')
    #plt.xlabel('Columns splitting size')
    #plt.ylabel('Accuracy (%)')
    #plt.legend(loc='lower center')
    #plt.gca().invert_xaxis()
    #plt.ylim((0,110))

    # Draw a heatmap with the numeric values in each cell

    # Generate a mask for the upper triangle
    mask = np.zeros_like(confMat, dtype=np.bool)
    mask[np.triu_indices_from(mask, k=4)] = True

    f, ax = plt.subplots(figsize=(7, 5))

    cmap = 'viridis'
    print(labels)
    ax = sns.heatmap(confMat, cmap=cmap, mask=mask, fmt='', annot_kws={"size": 8}, annot=labels,
        square=True, linewidths=.5, cbar_kws={"shrink": .5}, ax=ax, vmin=0, vmax=100)
    ax.set_yticklabels(total_bits, rotation=0)
    ax.set_xticklabels(frac_bits, rotation=0)
    ax.set(ylabel='Total bits', xlabel='Integer bits')
    ax.set_title('Output bitwdith prercision vs accuracy')

    # set up a figure twice as wide as it is tall
    fig = plt.figure(figsize=plt.figaspect(0.5))

    #===============
    #  First subplot
    #===============
    # set up the axes for the first plot
    ax = fig.add_subplot(2, 2, 1, projection='3d')
    X = frac_bits
    Y = total_bits
    X, Y = np.meshgrid(X, Y)
    surf = ax.plot_surface(X, Y, mat_brams, rstride=1, cstride=1, cmap=cmap,
                   linewidth=0, antialiased=False)
    ax.set_xlabel('Fractional bits'); ax.set_ylabel('Total bits'); ax.set_zlabel('BRAMs')

    #===============
    #  Second subplot
    #===============
    # set up the axes for the first plot
    ax = fig.add_subplot(2, 2, 2, projection='3d')
    surf = ax.plot_surface(X, Y, mat_dsps, rstride=1, cstride=1, cmap=cmap,
                   linewidth=0, antialiased=False)
    ax.set_xlabel('Fractional bits'); ax.set_ylabel('Total bits'); ax.set_zlabel('DSPs')

    #===============
    #  Third subplot
    #===============
    # set up the axes for the first plot
    ax = fig.add_subplot(2, 2, 3, projection='3d')
    surf = ax.plot_surface(X, Y, mat_ffs, rstride=1, cstride=1, cmap=cmap,
                   linewidth=0, antialiased=False)
    ax.set_xlabel('Fractional bits'); ax.set_ylabel('Total bits'); ax.set_zlabel('FFs')

    #===============
    #  Forth subplot
    #===============
    # set up the axes for the first plot
    ax = fig.add_subplot(2, 2, 4, projection='3d')
    surf = ax.plot_surface(X, Y, mat_luts, rstride=1, cstride=1, cmap=cmap,
                   linewidth=0, antialiased=False)
    ax.set_xlabel('Fractional bits'); ax.set_ylabel('Total bits'); ax.set_zlabel('LUTs')

    plt.show()
