from matplotlib import pyplot as PLT
import numpy as NP
import os
import shutil
import glob

dir_src = ("/tmp/")
dir_dst = ("/tmp/")
prefix = "dumpvars_"
prefixsm = "dumpvars_sm_"
#fig = PLT.figure()

f, axarr = PLT.subplots(2, 2)
axarr = axarr.ravel()

tifCounter = len(glob.glob1(dir_src,"prefix*.txt"))
minv = []
maxv = []
v = []

cmd = 'rm -rf ' + dir_src + prefixsm + '*.txt'
print(cmd)
os.system(cmd)

for filename in os.listdir(dir_src):
    if filename.startswith(prefix):
        print(filename)
        filename_sm = dir_dst + filename.replace(prefix, prefixsm)
        # from bash: head -10000000 /tmp/dumpvars_internal.txt | sort | uniq > /tmp/dumpvars_internal_sm.txt 
        cmd = 'cat ' + dir_src + filename + ' | sort | uniq >  ' + filename_sm
        print(cmd)
        os.system(cmd)
        #shutil.copy( dir_src + filename, dir_dst + filename.replace(prefix, prefixsm))

        v.append([])
        print(len(v))
        inf = open(filename_sm)
        for line in inf:
            v[len(v)-1].append(float(line))
        inf.close()

        minv.append(NP.min(v[len(v)-1]))
        maxv.append(NP.max(v[len(v)-1]))

        print (minv)
        print (maxv)
        val=10.0
        #axarr[len(v)-1].plot(v[len(v)-1], len(v[len(v)-1]) * [1] , 'x', label=filename_sm)
        axarr[len(v)-1].stem(v[len(v)-1], v[len(v)-1], ' ', label=filename_sm)
        
        axarr[len(v)-1].set_title(filename_sm)
        #axarr[len(v)-1].set_xscale('log')
        #axarr[len(v)-1].set_yscale('log')
        axarr[len(v)-1].set_xlabel('Values (double-IEEE754)')
        axarr[len(v)-1].set_ylabel('Values frequency')
        #axarr[len(v)-1].set_ylim([0, 10000])
        #axarr[len(v)-1].set_xlim(left=-10)


minv = NP.min(minv)
maxv = NP.max(maxv)
print ("FINALS")
print (minv)
print (maxv)
bins = NP.linspace(minv, maxv, 1000)
#print (bins)
#for i in range(len(v)):
#    PLT.hist((v[i]), alpha=0.5, bins=bins, normed=False, weights=None, stacked=True, label=filename_sm)
#     axarr[i].hist((v[i]), alpha=0.5, bins=bins, normed=False, weights=None, stacked=True, label=filename_sm)
#     axarr[i].set_title('Axis [0,0]')

#PLT.legend(loc='upper right')
#PLT.xscale('log')
#PLT.axis([-1000, 12000, 1, 100000])
#PLT.yscale('log')
#PLT.xlabel('Values of variables (double-IEEE754) in 10000 equal bins')
#PLT.ylabel('Variables frequency in each bin')


PLT.show()
