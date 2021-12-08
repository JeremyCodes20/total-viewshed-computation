#include <stdio.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <cmath>
#include <limits>
#include <float.h>
#include <string>
#include <chrono>

using namespace std::chrono;

const int map_width = 6000;
const int range = 15; // the width of the box shaped range one can see from the origin
const int range_radius = (range - 1) / 2;
const char* dem_location = "../data/srtm_14_04_6000x6000_short16.raw";
const char* output_location = "./viewshed-gpu.raw";

__host__ int openDem(const char* location, std::vector<short> &dem);
__host__ int writeViewshed(const char* location, std::vector<short> &vshed);
__host__ void printRows(std::vector<short> &rows);
__device__ void bLineDown(int x0, int y0, int x1, int y1, short* dem_d, float& max_slope, short& origin_height, int& map_width_d);
__device__ void bLineUp(int x0, int y0, int x1, int y1, short* dem_d, float& max_slope, short& origin_height, int& map_width_d);
__device__ void bLine(int x0, int y0, int x1, int y1, short* dem_d, float& max_slope, short& origin_height, int& map_width_d);
__global__ void singleViewshedCount(short* dem_d, short* vshed_d, int map_width_d, int range_radius_d);
__device__ void toGridCoords(int& index, int& x, int& y, int& map_width_d);
__device__ void toFlatCoords(int& x, int& y, int& index, int& map_width_d);
__host__ void compareOutput(std::string filename1, std::string filename2);
__host__ void printRowsPrim(short* rows);
__host__ int writeViewshedPrim(const char* location, short* &vshed);

inline
cudaError_t checkCuda(cudaError_t result)
{
#if defined(DEBUG) || defined(_DEBUG)
  if (result != cudaSuccess) {
    fprintf(stderr, "CUDA Runtime Error: %s\n", cudaGetErrorString(result));
    assert(result == cudaSuccess);
  }
#endif
  return result;
}

int main()
{
    auto start_execution = high_resolution_clock::now();

    std::vector<short> dem;
    if (openDem(dem_location, dem))
    {
        printf("Failed to open input file: %s\n", dem_location);
        return 1; // File failed to open, exit program
    }
    printf("Opened input file: %s\nVector size: %d\n", dem_location, dem.size());

    // small test dem
    // dem = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    //        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    //        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    //        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    //        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    //        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    //        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    //        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    //        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    //        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    //        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    //        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

    short* vshed = new short[dem.size()];

    // Allocate memory on device
    short *dem_d, *vshed_d;
    cudaMalloc(&dem_d, dem.size() * sizeof(short));
    cudaMalloc(&vshed_d, dem.size() * sizeof(short));
    cudaError_t error = cudaMemcpy(dem_d, &dem[0], dem.size() * sizeof(short), cudaMemcpyHostToDevice);
    if (error != cudaSuccess)
    {
        printf("Host to device failed.\n");
        return 1;
    }

    // Execute kernel
    int tpb = 600;
    int bpg = ((map_width * map_width) / tpb) + 1;
    dim3 dimGrid(bpg, 1, 1);
    dim3 dimBlock(tpb, 1, 1);

    printf("tpb: %d\nbpg: %d\n", tpb, bpg);
    printf("Launching the kernel...");
    auto start_kernel = high_resolution_clock::now();
    singleViewshedCount<<<dimGrid, dimBlock>>>(dem_d, vshed_d, map_width, range_radius);
    // singleViewshedCount<<<12, 12>>>(dem_d, vshed_d, map_width, range_radius);

    cudaDeviceSynchronize();
    printf("Kernel done!\n");
    auto kernel_time = duration_cast<milliseconds>(high_resolution_clock::now() - start_kernel);

    // Copy array from device to host
    error = cudaMemcpy(vshed, vshed_d, dem.size() * sizeof(short), cudaMemcpyDeviceToHost);
    if (error != cudaSuccess)
    {
        printf("Device to host failed.\n");
        return 1;
    }

    // Output
    // writeViewshed(output_location, vshed);
    
    // printRows(vshed);
    writeViewshedPrim(output_location, vshed);
    // printRowsPrim(vshed);

    // compareOutput("./viewshed-gpu.raw", "../distributed/viewshed-64.raw");

    // Free device memory
    cudaFree(dem_d);
    cudaFree(vshed_d);

    auto execution_time = duration_cast<milliseconds>(high_resolution_clock::now() - start_execution);
    printf("Kernel time: %d\nTotal Execution: %d\n", kernel_time, execution_time);
}

/*
    Compare two RAW files and count differences
*/
__host__ void compareOutput(std::string filename1, std::string filename2)
{
    std::ifstream file1(filename1, std::ios::binary);
    std::ifstream file2(filename2, std::ios::binary);

    if (!file1.is_open() || !file2.is_open())
    {
        printf("Failed to open one of two files\n");
        return;
    }

    short buf1, buf2;
    file1.seekg(0, std::ios::beg);
    file2.seekg(0, std::ios::beg);
    int count = 0;
    while (file1.read((char*)&buf1, sizeof(short)) && file2.read((char*)&buf2, sizeof(short)))
    {
        if (buf1 != buf2)
        {
            ++count;
        }
    }

    printf("Number of errors: %d\n", count);
}

/*
    Print contents of vector by row
*/
__host__ void printRows(std::vector<short> &rows)
{
    for (std::size_t row = 0; row < rows.size() / map_width; ++row)
    {
        for (int cell = 0; cell < map_width; ++cell)
        {
            printf("%4d", rows[row * map_width + cell]);
        }
        printf("\n");
    }
}

/*
    Print contents of array by row
*/
__host__ void printRowsPrim(short* rows)
{
    for (int row = 0; row < map_width * map_width / map_width; ++row)
    {
        for (int cell = 0; cell < map_width; ++cell)
        {
            printf("%4d", rows[row * map_width + cell]);
        }
        printf("\n");
    }
}

/*
    Open DEM from RAW file and store in 1-dimensional short array
*/
__host__ int openDem(const char* location, std::vector<short> &dem)
{
    std::ifstream input_raw(location, std::ios::binary);
    if (!input_raw.is_open())
    {
        return 1;
    }

    int i = 0;
    short buf;
    input_raw.seekg(0, std::ios::beg);
    while (input_raw.read((char *)&buf, sizeof(short)))
    {
        dem.push_back(buf);
        ++i;
    }

    input_raw.close();
    return 0;
}

/*
    Write computed viewshed counts to raw file
*/
__host__ int writeViewshed(const char* location, std::vector<short> &vshed)
{
    std::ofstream output_raw(location, std::ios::binary | std::ios::trunc);
    if (!output_raw.is_open())
    {
        return 1;
    }

    printf("Writing output to file %s...  ", location);
    output_raw.write((char const*)&vshed[0], map_width * map_width * sizeof(short));
    printf("Done!\n");

    output_raw.close();
    return 0;
}

/*
    Write computed viewshed counts to raw file
*/
__host__ int writeViewshedPrim(const char* location, short* &vshed)
{
    std::ofstream output_raw(location, std::ios::binary | std::ios::trunc);
    if (!output_raw.is_open())
    {
        return 1;
    }

    printf("Writing output to file %s...  ", location);
    output_raw.write((char*)vshed, map_width * map_width * sizeof(short));
    printf("Done!\n");

    output_raw.close();
    return 0;
}

/*
    Calculate the viewshed for a given origin
*/
__global__ void singleViewshedCount(short* dem_d, short* vshed_d, int map_width_d, int range_radius_d)
{
    int origin = blockIdx.x * blockDim.x + threadIdx.x;
    int map_size = map_width_d * map_width_d;
    if (origin >= map_size) return; // check threads out of bounds
    short count = 0;
    short origin_height = dem_d[origin];
    short p_height;
    float dx, dy, d, slope, ox, oy, px, py;
    ox = static_cast<float>(origin % map_width_d);
    oy = static_cast<float>(origin / map_width_d);

    int p;
    for (int i = -range_radius_d; i <= range_radius_d; ++i)
    {
        for (int j = -range_radius_d; j <= range_radius_d; ++j)
        {
            p = origin + (i * map_width_d + j);
            if (p < 0 || p > map_size || p == origin) continue; // top/bottom bounds and self
            p_height = dem_d[p];
            px = static_cast<float>(p % map_width_d);
            py = static_cast<float>(p / map_width_d);
            dx = ox - px;
            dy = oy - py;
            if ((j < 0 && dx < -j) || (j > 0 && dx > j)) continue; // left/right bounds
            d = hypot(dx, dy);
            slope = (p_height - origin_height) / d;
            float max_slope = -FLT_MAX;
            bLine(ox, oy, px, py, dem_d, max_slope, origin_height, map_width_d);
            if (slope >= max_slope)
            {
                // printf("I see %d(%d) from %d(%d). Slope is %f\n", p, p_height, origin, origin_height, slope);
                ++count;
            }
        }
    }

    vshed_d[origin] = count;
}

/*
    Convert 1-dimensional index to 2-dimensional indices
*/
__device__ void toGridCoords(int& index, int& x, int& y, int& map_width_d)
{
    x = index % map_width_d;
    y = index / map_width_d;
}

/*
    Convert 2-dimensional indices to 1-dimensional index
*/
__device__ void toFlatCoords(int& x, int& y, int& index, int& map_width_d)
{
    index = (y * map_width_d) + x;
}

/*
    Calculate line when the slope is negative (or 0!!)
    Source: https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm
*/
__device__ void bLineDown(int x0, int y0, int x1, int y1, short* dem_d, float& max_slope, short& origin_height, int& map_width_d)
{
    float diffx, diffy, diff;
    float slope = -FLT_MAX;
    int m;
    int dx = x1 - x0;
    int dy = y1 - y0;
    int yi = 1;
    if (dy < 0)
    {
        yi = -1;
        dy = -dy;
    }
    int D = (2 * dy) - dx;
    int y = y0;

    for (int x = x0 + 1; x < x1; ++x)
    {
        // flatten x,y coordinates to 1D -> m
        toFlatCoords(x, y, m, map_width_d);
        diffx = x - x0;
        diffy = y - y0;
        diff = hypot(diffx, diffy);
        slope = (dem_d[m] - origin_height) / diff;
        if (slope > max_slope) max_slope = slope;
        if (D > 0)
        {
            y += yi;
            D += (2 * (dy - dx));
        }
        else
        {
            D += 2 * dy;
        }
    }
}

/*
    Calculate line when the slope is positive
    Source: https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm
*/
__device__ void bLineUp(int x0, int y0, int x1, int y1, short* dem_d, float& max_slope, short& origin_height, int& map_width_d)
{
    float diffx, diffy, diff, slope;
    int m;
    int dx = x1 - x0;
    int dy = y1 - y0;
    int xi = 1;
    if (dx < 0)
    {
        xi = -1;
        dx = -dx;
    }
    int D = (2 * dx) - dy;
    int x = x0;

    for (int y = y0 + 1; y < y1; ++y)
    {
        toFlatCoords(x, y, m, map_width_d);
        diffx = x - x0;
        diffy = y - y0;
        diff = hypot(diffx, diffy);
        slope = (dem_d[m] - origin_height) / diff;
        if (slope > max_slope) max_slope = slope;
        if (D > 0)
        {
            x += xi;
            D += (2 * (dx - dy));
        }
        else
        {
            D += 2 * dx;
        }
    }
}

/*
    Calculate what points are intersected by a line between (x0,y0) and (x1,y1)
    Source: https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm
*/
__device__ void bLine(int x0, int y0, int x1, int y1, short* dem_d, float& max_slope, short& origin_height, int& map_width_d)
{
    if (abs(y1 - y0) < abs(x1 - x0))
    {
        (x0 > x1) ? bLineDown(x1, y1, x0, y0, dem_d, max_slope, origin_height, map_width_d) : bLineDown(x0, y0, x1, y1, dem_d, max_slope, origin_height, map_width_d);
    }
    else
    {
        (y0 > y1) ? bLineUp(x1, y1, x0, y0, dem_d, max_slope, origin_height, map_width_d) : bLineUp(x0, y0, x1, y1, dem_d, max_slope, origin_height, map_width_d);
    }
}