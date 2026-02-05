import imageio.v2 as iio
import numpy as np
import sys
import matplotlib.pyplot as plt

img_size = 26

if __name__ == '__main__':
    file_name = sys.argv[1]
    file_path = file_name[:-4] # Remove .png
    img = iio.imread(file_name)
    with open('fonts.bin','wb') as f:
        for pixel in img:
            for pixel_dat in pixel:
                f.write(b'\xFF' if pixel_dat[3] == 255 else b'\x00')
                print(1 if pixel_dat[3] == 255 else 0,end='')
            print(end='\n')