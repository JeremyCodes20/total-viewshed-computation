#include <iostream>
#include <fstream>
#include <stdio.h>
#include <vector>
#include <string>
#include <cmath>

const int map_width = 6000;
const int mask_width = 99; // mask == range, the width of the box shaped range one can see from the origin
const char* dem_location = "../data/srtm_14_04_6000x6000_short16.raw";

int openDem(const char* location, std::vector<short> &dem);
void testDemRead(std::vector<short> &dem);
void bLineDown(int x0, int y0, int x1, int y1, std::vector<short> &line);
void bLineUp(int x0, int y0, int x1, int y1, std::vector<short> &line);
void bLine(int x0, int y0, int x1, int y1, std::vector<short> &line);
void generateMask(std::vector<std::vector<short>> &mask);
void reportMask(std::vector<std::vector<short>> &mask);
int singleViewshedCount(int origin, std::vector<short> &dem);

int main()
{
    // Digital Elevation Map: 6000x6000 array of shorts
    std::vector<short> dem;
    if (openDem(dem_location, dem))
    {
        printf("Failed to open input file: %s\n", dem_location);
        return 1; // File failed to open, exit program
    }
    printf("Opened input file: %s\n", dem_location);
    // testDemRead(dem);

    singleViewshedCount(0, dem);

    // std::vector<short> test;
    // bLine(49, 49, 49, 0, test);

    // std::vector<std::vector<short>> mask(mask_width * mask_width, std::vector<short>());
    // generateMask(mask);
    // reportMask(mask);

    return 0;
}

int singleViewshedCount(int origin, std::vector<short> &dem)
{
    int count = 0;
    short origin_height = dem[origin];
    short p_height;
    float dx, dy, d, slope;

    int range_length = (mask_width * mask_width - 1) / 2;
    int p;
    for (p = origin - range_length; p < origin + range_length; ++p)
    {
        if (p < 0 || p > map_width * map_width || p == origin) continue;
        p_height = dem[p];
        dx = (static_cast<float>(origin % map_width)) - (p % map_width);
        dy = (static_cast<float>(origin / map_width)) - (p / map_width);
        d = hypot(dx, dy);
        slope = (p_height - origin_height) / d;
        // printf("Slope: %f\n", slope);
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
    Calculate line when the slope is negative (or 0!!)
    Source: https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm
*/
void bLineDown(int x0, int y0, int x1, int y1, std::vector<short> &line)
{
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
    for (int x = x0; x <= x1; ++x)
    {
        // add x,y to line collection
        printf("(%d, %d)\n", x, y);
        line.push_back(x);
        line.push_back(y);
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
void bLineUp(int x0, int y0, int x1, int y1, std::vector<short> &line)
{
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
    for (int y = y0; y <= y1; ++y)
    {
        // add x,y to line collection
        printf("(%d, %d)\n", x, y);
        line.push_back(x);
        line.push_back(y);
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
void bLine(int x0, int y0, int x1, int y1, std::vector<short> &line)
{
    if (abs(y1 - y0) < abs(x1 - x0))
    {
        (x0 > x1) ? bLineDown(x1, y1, x0, y0, line) : bLineDown(x0, y0, x1, y1, line);
    }
    else
    {
        (y0 > y1) ? bLineUp(x1, y1, x0, y0, line) : bLineUp(x0, y0, x1, y1, line);
    }
}

void generateMask(std::vector<std::vector<short>> &mask)
{
    int mask_size = mask_width * mask_width;
    int originx = mask_width / 2;
    int originy = originx;
    int i = 0;
    for (std::vector<short> &cell : mask)
    {
        int cellx = i % mask_width;
        int celly = i / mask_size;

        bLine(cellx, celly, originx, originy, cell);

        ++i;
    }
}

void reportMask(std::vector<std::vector<short>> &mask)
{
    size_t count = 0;
    printf("Mask.size(): %d", mask.size());
    for (std::vector<short> &cell : mask)
    {
        printf("(%d, %d)\n", cell[0], cell[1]);
        ++count;
    }
    printf("Count: %d\n", count);
}
