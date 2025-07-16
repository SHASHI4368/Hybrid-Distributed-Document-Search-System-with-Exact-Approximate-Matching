#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <omp.h>
#include <mpi.h>
#include "file_utils.h"

int is_supported_file(const char *filename)
{
    const char *ext = strrchr(filename, '.');
    return ext && (strcmp(ext, ".txt") == 0 ||
                   strcmp(ext, ".pdf") == 0 ||
                   strcmp(ext, ".docx") == 0);
}

void list_files(const char *directory, char files[][512], int *count)
{
    DIR *dir = opendir(directory);
    struct dirent *entry;
    *count = 0;
    while ((entry = readdir(dir)))
    {
        if (entry->d_type == DT_REG && is_supported_file(entry->d_name))
        {
            snprintf(files[*count], 512, "%s/%s", directory, entry->d_name);
            (*count)++;
        }
    }
    closedir(dir);
}

void preprocess_files(const char *src_dir, const char *out_dir, char output_files[][512], int *count, int mode)
{
    mkdir(out_dir, 0777); // create output directory
    char input_files[1000][512];
    int total = 0;

    list_files(src_dir, input_files, &total);

    *count = 0;

    int rank = 0, size = 1;

    if (mode == 3 || mode == 4)
    {
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        MPI_Comm_size(MPI_COMM_WORLD, &size);
    }

    // SERIAL
    if (mode == 1)
    {
        for (int i = 0; i < total; i++)
        {
            const char *file = input_files[i];
            const char *ext = strrchr(file, '.');
            char base[256];
            sscanf(strrchr(file, '/') + 1, "%[^.]", base);

            if (strcmp(ext, ".txt") == 0)
            {
                snprintf(output_files[*count], 512, "%s", file);
            }
            else if (strcmp(ext, ".pdf") == 0)
            {
                snprintf(output_files[*count], 512, "%s/%s.txt", out_dir, base);
                char cmd[1024];
                snprintf(cmd, sizeof(cmd), "pdftotext \"%s\" \"%s\"", file, output_files[*count]);
                system(cmd);
            }
            else if (strcmp(ext, ".docx") == 0)
            {
                char orig_output[512];
                snprintf(orig_output, sizeof(orig_output), "/tmp/%s.txt", base);
                char cmd[1024];
                snprintf(cmd, sizeof(cmd), "libreoffice --headless --convert-to txt:Text \"%s\" --outdir /tmp > /dev/null 2>&1", file);
                system(cmd);
                snprintf(output_files[*count], 512, "%s/%s.txt", out_dir, base);
                rename(orig_output, output_files[*count]);
            }

            (*count)++;
        }
    }

    // OPENMP
    else if (mode == 2)
    {
#pragma omp parallel for schedule(dynamic)
        for (int i = 0; i < total; i++)
        {
            const char *file = input_files[i];
            const char *ext = strrchr(file, '.');
            char base[256];
            sscanf(strrchr(file, '/') + 1, "%[^.]", base);

            char output_path[512];

            if (strcmp(ext, ".txt") == 0)
            {
                snprintf(output_path, 512, "%s", file);
            }
            else if (strcmp(ext, ".pdf") == 0)
            {
                snprintf(output_path, 512, "%s/%s.txt", out_dir, base);
                char cmd[1024];
                snprintf(cmd, sizeof(cmd), "pdftotext \"%s\" \"%s\"", file, output_path);
                system(cmd);
            }
            else if (strcmp(ext, ".docx") == 0)
            {
                char orig_output[512];
                snprintf(orig_output, sizeof(orig_output), "/tmp/%s.txt", base);
                char cmd[1024];
                snprintf(cmd, sizeof(cmd), "libreoffice --headless --convert-to txt:Text \"%s\" --outdir /tmp > /dev/null 2>&1", file);
                system(cmd);
                snprintf(output_path, 512, "%s/%s.txt", out_dir, base);
                rename(orig_output, output_path);
            }

#pragma omp critical
            {
                snprintf(output_files[*count], 512, "%s", output_path);
                (*count)++;
            }
        }
    }

    // MPI or MPI + OpenMP (modes 3 and 4)
    else if (mode == 3 || mode == 4)
    {
        int local_count = 0;
#pragma omp parallel for if (mode == 4) schedule(dynamic)
        for (int i = 0; i < total; i++)
        {
            if (i % size != rank) continue;

            const char *file = input_files[i];
            const char *ext = strrchr(file, '.');
            char base[256];
            sscanf(strrchr(file, '/') + 1, "%[^.]", base);

            char output_path[512];

            if (strcmp(ext, ".txt") == 0)
            {
                snprintf(output_path, 512, "%s", file);
            }
            else if (strcmp(ext, ".pdf") == 0)
            {
                snprintf(output_path, 512, "%s/%s.txt", out_dir, base);
                char cmd[1024];
                snprintf(cmd, sizeof(cmd), "pdftotext \"%s\" \"%s\"", file, output_path);
                system(cmd);
            }
            else if (strcmp(ext, ".docx") == 0)
            {
                char orig_output[512];
                snprintf(orig_output, sizeof(orig_output), "/tmp/%s.txt", base);
                char cmd[1024];
                snprintf(cmd, sizeof(cmd), "libreoffice --headless --convert-to txt:Text \"%s\" --outdir /tmp > /dev/null 2>&1", file);
                system(cmd);
                snprintf(output_path, 512, "%s/%s.txt", out_dir, base);
                rename(orig_output, output_path);
            }

#pragma omp critical
            {
                snprintf(output_files[*count], 512, "%s", output_path);
                (*count)++;
            }
        }

        // Optionally: you could reduce counts into rank 0 here
        // and broadcast output_files if needed
    }
}
