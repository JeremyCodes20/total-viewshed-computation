#include <iostream>
#include <fstream>
#include <stdio.h>

const int map_width = 6000;
const char* dem_location = "../data/srtm_14_04_6000x6000_short16.raw";

int openDem(const char* location, short* dem);
void testDemRead(short* dem);
void bLineDown(int x0, int y0, int x1, int y1);
void bLineUp(int x0, int y0, int x1, int y1);
void bLine(int x0, int y0, int x1, int y1);

int main()
{
    // Digital Elevation Map: 6000x6000 array of shorts
    short* dem = new short[map_width * map_width];

    if (openDem(dem_location, dem))
    {
        printf("Failed to open input file: %s\n", dem_location);
        delete[] dem;
        return 1; // File failed to open, exit program
    }
    printf("Opened input file: %s\n", dem_location);
    // testDemRead(dem);

    bLine(5, 5, 5, 0);

    delete[] dem;
    return 0;
}

/*
    Open DEM from RAW file and store in 1-dimensional short array
*/
int openDem(const char* location, short* dem)
{
    std::ifstream input_raw(location, std::ios::binary);
    if (!input_raw.is_open())
    {
        return 1;
    }

    int i = 0;
    char buf[sizeof(short)];
    input_raw.seekg(0, std::ios::beg);
    while (input_raw.read(buf, sizeof(short)))
    {
        memcpy(&dem[i], buf, sizeof(short));
        ++i;
    }

    input_raw.close();
    return 0;
}

/*
    Read a few values from the dem array to verify that input read was successful
*/
void testDemRead(short* dem)
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
void bLineDown(int x0, int y0, int x1, int y1)
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
void bLineUp(int x0, int y0, int x1, int y1)
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
void bLine(int x0, int y0, int x1, int y1)
{
    if (abs(y1 - y0) < abs(x1 - x0))
    {
        (x0 > x1) ? bLineDown(x1, y1, x0, y0) : bLineDown(x0, y0, x1, y1);
    }
    else
    {
        (y0 > y1) ? bLineUp(x1, y1, x0, y0) : bLineUp(x0, y0, x1, y1);
    }
}