#include <stdio.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <mpi.h>
#include <cmath>
#include <limits>

const int map_width = 6000; // TODO: fix this to 6000
const int range = 15; // the width of the box shaped range one can see from the origin
const int range_radius = (range - 1) / 2;
const char* dem_location = "../data/srtm_14_04_6000x6000_short16.raw";
const char* out_file = "./viewshed.raw";
int num_processes, rank;

int openDem(const char* location, std::vector<short> &dem);
int writeViewshed(const char* location, std::vector<short> &vshed);
void printRows(std::vector<short> &rows);
void bLineDown(int x0, int y0, int x1, int y1, std::vector<short> &dem, float& max_slope, short& origin_height);
void bLineUp(int x0, int y0, int x1, int y1, std::vector<short> &dem, float& max_slope, short& origin_height);
void bLine(int x0, int y0, int x1, int y1, std::vector<short> &dem, float& max_slope, short& origin_height);
short singleViewshedCount(int origin, std::vector<short> &dem);
void toGridCoords(int& index, int& x, int& y);
void toFlatCoords(int& x, int& y, int& index);

int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_processes);

    double start_time = MPI_Wtime();

    // calculate displacements and sendcounts;
    std::vector<int> sendcounts(num_processes, 0);
    std::vector<int> displs(num_processes, 0);
    int top_count = map_width * ((map_width / num_processes) + range_radius);
    sendcounts[0] = top_count;
    int i_displace = (map_width / num_processes) * map_width;
    int row_count = map_width / num_processes;
    int local_count = row_count * map_width;
    sendcounts[num_processes - 1] = top_count;
    // printf("Rank 0\nsendcounts: %d\ndispls: %d\n", sendcounts[0], displs[0]);
    for (int i = 1; i < num_processes - 1; ++i)
    {
        sendcounts[i] = map_width * (row_count +  (2 * range_radius));
        displs[i] = i_displace - (map_width * range_radius);
        i_displace += row_count * map_width;
        // printf("Rank %d\nsendcounts: %d\ndispls: %d\n", i, sendcounts[i], displs[i]);
    }
    displs[num_processes - 1] = i_displace - (map_width * range_radius);
    // printf("Rank %d\nsendcounts: %d\ndispls: %d\n", num_processes, sendcounts[num_processes], displs[num_processes]);

    // Rank 0 open and read the DEM file
    std::vector<short> local_dem;
    if (rank == 0)
    {
        std::vector<short> dem;
        if (openDem(dem_location, dem))
        {
            printf("Failed to open input file: %s\n", dem_location);
            return 1; // File failed to open, exit program
        }
        printf("Opened input file: %s\nVector size: %zd\nNumber of processes: %d\n", dem_location, dem.size(), num_processes);

        // small-scale test DEM
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

        local_dem.resize(sendcounts[0]);
        MPI_Scatterv(&dem[0], &sendcounts[0], &displs[0], MPI_SHORT, &local_dem[0], sendcounts[rank], MPI_SHORT, 0, MPI_COMM_WORLD);
        // printRows(local_dem);
    }
    else
    {
        local_dem.resize(sendcounts[rank]);
        MPI_Scatterv(NULL, NULL, NULL, NULL, &local_dem[0], sendcounts[rank], MPI_SHORT, 0, MPI_COMM_WORLD);
    }

    // compute viewshed for each assigned cell
    std::vector<short> local_counts(local_count, 1);
    int start;
    if (rank == 0)
    {
        start = 0;
    }
    else
    {
        start = range_radius * map_width;
    }

    for (int i = 0; i < local_count; ++i,++start)
    {
        local_counts[i] = singleViewshedCount(start, local_dem);
    }

    // collect computed viewsheds
    if (rank == 0)
    {
        std::vector<short> counts(map_width * map_width, 0);
        MPI_Gather(&local_counts[0], local_count, MPI_SHORT, &counts[0], local_count, MPI_SHORT, 0, MPI_COMM_WORLD);

        // output viewshed counts to RAW file
        writeViewshed(out_file, counts);
        // printRows(counts);

        double total_time = MPI_Wtime() - start_time;
        printf("Total execution time: %f\n", total_time);
    }
    else
    {
        MPI_Gather(&local_counts[0], local_count, MPI_SHORT, NULL, 0, NULL, 0, MPI_COMM_WORLD);
    }

    MPI_Finalize();
    return 0;
}

/*
    Print contents of array by row
*/
void printRows(std::vector<short> &rows)
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
    Write computed viewshed counts to raw file
*/
int writeViewshed(const char* location, std::vector<short> &vshed)
{
    char* file_name;
    asprintf(&file_name, "./viewshed-%d.raw", num_processes);

    std::ofstream output_raw(file_name, std::ios::binary | std::ios::trunc);
    if (!output_raw.is_open())
    {
        return 1;
    }

    printf("Writing output to file %s...  ", file_name);
    output_raw.write((char const*)&vshed[0], map_width * map_width * sizeof(short));
    printf("Done!\n");

    output_raw.close();
    free(file_name);
    return 0;
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

    int p;
    // TODO: this is likely partially correct, still need to figure out left and right bounds
    for (int i = -range_radius; i <= range_radius; ++i)
    {
        for (int j = -range_radius; j <= range_radius; ++j)
        {
            p = origin + (i * map_width + j);
            if (p < 0 || p > static_cast<int>(dem.size()) || p == origin) continue; // top/bottom bounds and self
            p_height = dem[p];
            px = static_cast<float>(p % map_width);
            py = static_cast<float>(p / map_width);
            dx = ox - px;
            dy = oy - py;
            if ((j < 0 && dx < -j) || (j > 0 && dx > j)) continue; // left/right bounds
            d = hypot(dx, dy);
            slope = (p_height - origin_height) / d;
            float max_slope = -std::numeric_limits<float>::max();
            bLine(ox, oy, px, py, dem, max_slope, origin_height);
            if (slope >= max_slope)
            {
                // printf("I see %d(%d) from %d(%d). Slope is %f\n", p, p_height, origin, origin_height, slope);
                ++count;
            }
        }
    }

    return count;
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