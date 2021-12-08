#include <iostream>
#include <fstream>
#include <stdio.h>
#include <vector>
#include <string>
#include <cmath>
#include <limits>
#include <chrono>
using namespace std::chrono;

const int map_width = 6000; // TODO: fix this to 6000
const int mask_width = 15; // mask == range, the width of the box shaped range one can see from the origin
const char* dem_location = "../data/srtm_14_04_6000x6000_short16.raw";
int num_threads = 4;
bool show;

int openDem(const char* location, std::vector<short> &dem);
void testDemRead(std::vector<short> &dem);
void bLineDown(int x0, int y0, int x1, int y1, std::vector<short> &dem, float& max_slope, short& origin_height);
void bLineUp(int x0, int y0, int x1, int y1, std::vector<short> &dem, float& max_slope, short& origin_height);
void bLine(int x0, int y0, int x1, int y1, std::vector<short> &dem, float& max_slope, short& origin_height);
void generateMask(std::vector<std::vector<short>> &mask);
void reportMask(std::vector<std::vector<short>> &mask);
short singleViewshedCount(int origin, std::vector<short> &dem);
void toGridCoords(int& index, int& x, int& y);
void toFlatCoords(int& x, int& y, int& index);
void computeTotalViewshed(std::vector<short> &dem, std::vector<short> &vshed);

int main()
{
    // Digital Elevation Map: 6000x6000 array of shorts
    std::vector<short> dem;
    if (openDem(dem_location, dem))
    {
        printf("Failed to open input file: %s\n", dem_location);
        return 1; // File failed to open, exit program
    }
    printf("Opened input file: %s\nVector size: %d\nThreads: %d\n", dem_location, dem.size(), num_threads);
    // testDemRead(dem);

    // dem =  {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    //         1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    //         1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    //         1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    //         1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    //         1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    //         1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    //         1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    //         1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    //         1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    //         1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    //         1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

    // dem = {1, 1, 1, 1, 1, 1, 1, 1, 1,
    //        1, 0, 0, 0, 0, 0, 0, 0, 1,
    //        1, 0, 0, 0, 0, 0, 0, 0, 1,
    //        1, 0, 0, 0, 0, 0, 0, 0, 1,
    //        1, 0, 0, 0, 0, 0, 0, 0, 1,
    //        1, 0, 0, 0, 0, 0, 0, 0, 1,
    //        1, 0, 0, 0, 0, 0, 0, 0, 1,
    //        1, 0, 0, 0, 0, 0, 0, 0, 1,
    //        1, 1, 1, 1, 1, 1, 1, 1, 1};
    // dem = {1, 1, 1, 1, 1, 1, 1, 1, 1,
    //        1, 2, 2, 2, 2, 2, 2, 2, 2,
    //        1, 2, 3, 3, 3, 3, 3, 2, 1,
    //        1, 2, 3, 4, 4, 4, 3, 2, 1,
    //        1, 2, 3, 4, 5, 4, 3, 2, 1,
    //        1, 2, 3, 4, 4, 4, 3, 2, 1,
    //        1, 2, 3, 3, 3, 3, 3, 2, 1,
    //        1, 2, 2, 2, 2, 2, 2, 2, 2,
    //        1, 1, 1, 1, 1, 1, 1, 1, 1};
    // dem = {1, 1, 1,
    //        1, 2, 1,
    //        1, 1, 1};

    auto start = high_resolution_clock::now();
    std::vector<short> vshed(map_width * map_width, 0);
    computeTotalViewshed(dem, vshed);
    auto total_execution = duration_cast<milliseconds>(high_resolution_clock::now() - start);

    printf("Total execution time (ms): %d\n", total_execution);

    // printf("First 20 values:\n");
    // int i;
    // for (i = 0; i < 20; ++i)
    // {
    //     printf("%d ", vshed[i]);
    // }

    printf("Viewshed:\n");
    int i;
    int j;
    for (i = 0; i < map_width; ++i)
    {
        for (j = 0; j < map_width; ++j)
        {
            printf("%4d", vshed[(i * map_width) + j]);
        }
        printf("\n");
    }

    return 0;
}

/*
    Calculate the viewshed for every possible origin in the dem and store visibility counts in vshed
*/
void computeTotalViewshed(std::vector<short> &dem, std::vector<short> &vshed)
{
    int i;
#pragma omp parallel for num_threads(num_threads) shared(dem, vshed) private(i)
    for (i = 0; i < map_width * map_width; ++i)
    {
        vshed[i] = singleViewshedCount(i, dem);
    }
}

/*
    Calculate the viewshed for a given origin
*/
short singleViewshedCount(int origin, std::vector<short> &dem)
{
    short count = 0;
    short origin_height = dem[origin];
    short p_height;
    float dx, dy, d, slope, ox, oy, px, py;
    ox = static_cast<float>(origin % map_width);
    oy = static_cast<float>(origin / map_width);
    show = false;
    if (origin == 0 || origin == 8 || origin == 80 || origin == 71) show = true;
    if (show) printf("I am (%f, %f)\n", ox, oy);

    int range_length = (mask_width * mask_width - 1) / 2;
    int range_radius = (mask_width - 1) / 2;
    int p;
    // TODO: this is likely partially correct, still need to figure out left and right bounds
    for (int i = -range_radius; i <= range_radius; ++i)
    {
        for (int j = -range_radius; j <= range_radius; ++j)
        {
            p = origin + (i * map_width + j);
            if (p < 0 || p > map_width * map_width || p == origin) continue; // top/bottom bounds and self
            p_height = dem[p];
            px = static_cast<float>(p % map_width);
            py = static_cast<float>(p / map_width);
            dx = ox - px;
            dy = oy - py;
            if ((j < 0 && dx < -j) || (j > 0 && dx > j)) continue; // left/right bounds
            if (show) printf("Line for (%f, %f):\n", px, py);
            d = hypot(dx, dy);
            slope = (p_height - origin_height) / d;
            float max_slope = -std::numeric_limits<float>::max();
            bLine(ox, oy, px, py, dem, max_slope, origin_height);
            if (slope >= max_slope)
            {
                if (show) printf("I see %d(%d) from %d(%d). Slope is %f\n", p, p_height, origin, origin_height, slope);
                ++count;
            }
        }
    }

    return count;
}

/*
    Open DEM from RAW file and store in 1-dimensional short array
*/
int openDem(const char* location, std::vector<short> &dem)
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
    Read a few values from the dem array to verify that input read was successful
*/
void testDemRead(std::vector<short> &dem)
{
    int upper_bound = map_width * map_width;
    for (int i = upper_bound - 100; i < upper_bound; ++i)
    {
        printf("%d\n", dem[i]);
    }
}

/*
    Convert 1-dimensional index to 2-dimensional indices
*/
void toGridCoords(int& index, int& x, int& y)
{
    x = index % map_width;
    y = index / map_width;
}

/*
    Convert 2-dimensional indices to 1-dimensional index
*/
void toFlatCoords(int& x, int& y, int& index)
{
    index = (y * map_width) + x;
}

/*
    Calculate line when the slope is negative (or 0!!)
    Source: https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm
*/
void bLineDown(int x0, int y0, int x1, int y1, std::vector<short> &dem, float& max_slope, short& origin_height)
{
    float diffx, diffy, diff;
    float slope = -std::numeric_limits<float>::max();
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

    // NOTE: may be just <
    for (int x = x0 + 1; x < x1; ++x)
    {
        // add x,y to line collection
        if (show) printf("(%d, %d)\n", x, y);
        // flatten x,y coordinates to 1D -> m
        toFlatCoords(x, y, m);
        diffx = x - x0;
        diffy = y - y0;
        diff = hypot(diffx, diffy);
        slope = (dem[m] - origin_height) / diff;
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
void bLineUp(int x0, int y0, int x1, int y1, std::vector<short> &dem, float& max_slope, short& origin_height)
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

    // NOTE: may be just <
    for (int y = y0 + 1; y < y1; ++y)
    {
        // add x,y to line collection
        if (show) printf("(%d, %d)\n", x, y);
        toFlatCoords(x, y, m);
        diffx = x - x0;
        diffy = y - y0;
        diff = hypot(diffx, diffy);
        slope = (dem[m] - origin_height) / diff;
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
void bLine(int x0, int y0, int x1, int y1, std::vector<short> &dem, float& max_slope, short& origin_height)
{
    if (abs(y1 - y0) < abs(x1 - x0))
    {
        (x0 > x1) ? bLineDown(x1, y1, x0, y0, dem, max_slope, origin_height) : bLineDown(x0, y0, x1, y1, dem, max_slope, origin_height);
    }
    else
    {
        (y0 > y1) ? bLineUp(x1, y1, x0, y0, dem, max_slope, origin_height) : bLineUp(x0, y0, x1, y1, dem, max_slope, origin_height);
    }
}
