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
    if (!dir) {
        *count = 0;
        return;
    }
    
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
    // Create output directory
    char mkdir_cmd[1024];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", out_dir);
    system(mkdir_cmd);
    
    char input_files[1000][512];
    int total = 0;

    list_files(src_dir, input_files, &total);
    *count = 0;

    int rank = 0, size = 1;

    // Get MPI info if in MPI mode
    if (mode == 3 || mode == 4)
    {
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        MPI_Comm_size(MPI_COMM_WORLD, &size);
    }

    //=================================== SERIAL MODE =========================================
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
                snprintf(cmd, sizeof(cmd), "pdftotext \"%s\" \"%s\" 2>/dev/null", file, output_files[*count]);
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

    //============================================== OPENMP MODE ====================================================
    else if (mode == 2)
    {
        // to ensure thread-safe access to the count variable
        char temp_output_files[1000][512];
        int temp_count = 0;

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
                snprintf(cmd, sizeof(cmd), "pdftotext \"%s\" \"%s\" 2>/dev/null", file, output_path);
                system(cmd);
            }
            else if (strcmp(ext, ".docx") == 0)
            {
                char orig_output[512];
                snprintf(orig_output, sizeof(orig_output), "/tmp/%s_%d.txt", base, omp_get_thread_num());
                char cmd[1024];
                snprintf(cmd, sizeof(cmd), "libreoffice --headless --convert-to txt:Text \"%s\" --outdir /tmp > /dev/null 2>&1", file);
                system(cmd);
                snprintf(output_path, 512, "%s/%s.txt", out_dir, base);
                rename(orig_output, output_path);
            }

#pragma omp critical
            {
                snprintf(temp_output_files[temp_count], 512, "%s", output_path);
                temp_count++;
            }
        }
        
        // Copy results back
        for (int i = 0; i < temp_count; i++) {
            snprintf(output_files[i], 512, "%s", temp_output_files[i]);
        }
        *count = temp_count;
    }

    // MPI MODE (mode 3) or MPI + OpenMP MODE (mode 4)
    else if (mode == 3 || mode == 4)
    {
        // In MPI mode, only rank 0 do all preprocessing

        if (rank == 0) {
            if (mode == 3) {
                //====================================== Pure MPI ===============================================
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
                        snprintf(cmd, sizeof(cmd), "pdftotext \"%s\" \"%s\" 2>/dev/null", file, output_files[*count]);
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
            } else {
                //======================================= MPI + OpenMP ========================================
                char temp_output_files[1000][512];
                int temp_count = 0;

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
                        snprintf(cmd, sizeof(cmd), "pdftotext \"%s\" \"%s\" 2>/dev/null", file, output_path);
                        system(cmd);
                    }
                    else if (strcmp(ext, ".docx") == 0)
                    {
                        char orig_output[512];
                        snprintf(orig_output, sizeof(orig_output), "/tmp/%s_%d.txt", base, omp_get_thread_num());
                        char cmd[1024];
                        snprintf(cmd, sizeof(cmd), "libreoffice --headless --convert-to txt:Text \"%s\" --outdir /tmp > /dev/null 2>&1", file);
                        system(cmd);
                        snprintf(output_path, 512, "%s/%s.txt", out_dir, base);
                        rename(orig_output, output_path);
                    }

#pragma omp critical
                    {
                        snprintf(temp_output_files[temp_count], 512, "%s", output_path);
                        temp_count++;
                    }
                }
                
                // Copy results back
                for (int i = 0; i < temp_count; i++) {
                    snprintf(output_files[i], 512, "%s", temp_output_files[i]);
                }
                *count = temp_count;
            }
        }
        
       
    }
}