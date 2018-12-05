import os
import numpy as np
from PIL import Image

PADDING = 16
MIN_CLIP = -1.75
MAX_CLIP = 3.75

img = Image.open('/tools/projects/LSTM-PYNQ/notebooks/Fraktur_images/010001.raw.lnrm.png').convert('L')
input_data = np.array(img.getdata())

fraktur_mean = np.mean(input_data)
fraktur_std_deviation = np.std(input_data)

input_data = input_data * 1.0 / np.amax(input_data)
input_data = np.amax(input_data) - input_data
input_data = input_data.T


#w = input_data.shape[0]
#input_data = np.vstack([np.zeros((PADDING, w)),input_data,np.zeros((PADDING, w))])
input_data = (input_data - fraktur_mean) / fraktur_std_deviation
input_data = input_data.reshape(-1,1)
#input_data = np.round(input_data * 4)/4
input_data = np.clip(input_data, a_min=float(MIN_CLIP), a_max=float(MAX_CLIP))
input_data = input_data.astype(np.float32) 
np.savetxt("010001.raw.lnrm.png.txt", input_data, newline="\n", fmt='%.13f')      
#img.show()