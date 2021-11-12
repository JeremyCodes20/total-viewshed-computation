#include <iostream>
#include <fstream>

const int map_width = 6000;
const char* dem_location = "../data/srtm_14_04_6000x6000_short16.raw";

int openDem(const char* location, short* dem);

int main()
{
    // Digital Elevation Map: 6000x6000 array of shorts
    short* dem = new short[map_width * map_width];

    if (!openDem(dem_location, dem))
    {
        free(dem);
        return 1; // File failed to open, exit program
    }

    

    free(dem);
    return 0;
}

int openDem(const char* location, short* dem)
{
    std::ifstream input_raw(location, std::ios::binary);
    if (!input_raw.is_open())
    {
        std::cout << "Failed to open input file: ";
        std::cout << location;
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