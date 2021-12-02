#include <iostream>
#include <fstream>
#include <stdio.h>

const int map_width = 6000;
const char* dem_location = "../data/srtm_14_04_6000x6000_short16.raw";

int openDem(const char* location, short* dem);
void testDemRead(short* dem);

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

void testDemRead(short* dem)
{
    int upper_bound = map_width * map_width;
    for (int i = upper_bound - 100; i < upper_bound; ++i)
    {
        printf("%d\n", dem[i]);
    }
}