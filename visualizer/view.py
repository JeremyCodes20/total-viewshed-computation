import matplotlib.pyplot as pyplot
import matplotlib.image as mpimg
import numpy as np

def main():
    dem_gpu = np.fromfile("../gpu/viewshed-gpu.raw", dtype=np.short, count=6000*6000)
    dem_gpu = dem_gpu.reshape(6000, 6000)

    pyplot.imshow(dem_gpu)
    pyplot.colorbar()
    pyplot.show()

    dem_dist = np.fromfile("../distributed/viewshed-64.raw", dtype=np.short, count=6000*6000)
    dem_dist = dem_dist.reshape(6000, 6000)

    pyplot.imshow(dem_dist)
    pyplot.colorbar()
    pyplot.show()

main()