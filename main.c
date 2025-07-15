#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <mpi.h>
#include <omp.h>
#include "file_utils.h"
#include "matcher.h"

#define MAX_FILES 1000

void search_serial(char files[][512], int file_count, const char *pattern, int mode)
{
  int found = 0;
  for (int i = 0; i < file_count; i++)
  {
    if (do_search(files[i], pattern, mode))
    {
      printf("[SERIAL] Found in %s\n", files[i]);
      found = 1;
    }
  }
  if (!found)
    printf("[SERIAL] No match found.\n");
}

void search_openmp(char files[][512], int file_count, const char *pattern, int mode)
{
  omp_set_num_threads(16);

  // Print the actual number of threads used
  #pragma omp parallel
  {
    #pragma omp single
    {
      int actual_threads = omp_get_num_threads();
      printf("[OPENMP] Using %d threads\n", actual_threads);
    }
  }

  int found = 0;

  // each thread will select files dynamically to search
  #pragma omp parallel for schedule(dynamic)
  for (int i = 0; i < file_count; i++)
  {
    if (do_search(files[i], pattern, mode))
    {
      // only one thread can print at a time 
      #pragma omp critical
      {
        printf("[OPENMP] Thread %d found in %s\n", omp_get_thread_num(), files[i]);
        found = 1;
      }
    }
  }

  if (!found)
    printf("[OPENMP] No match found.\n");
}


void search_mpi(char files[][512], int file_count, const char *pattern, int mode, int rank, int size)
{
  int found = 0;

  for (int i = rank; i < file_count; i += size)
  {
    if (do_search(files[i], pattern, mode))
    {
      printf("[MPI] Rank %d found in %s\n", rank, files[i]);
      found = 1;
    }
  }

  int global_found;
  MPI_Reduce(&found, &global_found, 1, MPI_INT, MPI_LOR, 0, MPI_COMM_WORLD);

  if (rank == 0 && !global_found)
  {
    printf("[MPI] No match found.\n");
  }
}

void search_mpi_openmp(char files[][512], int file_count, const char *pattern, int mode, int rank, int size)
{
  int found = 0;

#pragma omp parallel for schedule(dynamic)
  for (int i = rank; i < file_count; i += size)
  {
    if (do_search(files[i], pattern, mode))
    {
#pragma omp critical
      {
        printf("[MPI+OPENMP] Rank %d Thread %d found in %s\n", rank, omp_get_thread_num(), files[i]);
        found = 1;
      }
    }
  }

  int global_found;
  MPI_Reduce(&found, &global_found, 1, MPI_INT, MPI_LOR, 0, MPI_COMM_WORLD);

  if (rank == 0 && !global_found)
  {
    printf("[MPI+OPENMP] No match found.\n");
  }
}

double get_time_in_seconds()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (tv.tv_sec + tv.tv_usec / 1000000.0);
}

int main(int argc, char *argv[])
{
  if (argc != 4)
  {
    printf("Usage: mpirun -np <n> ./docsearch <docs_folder> <pattern> <mode: 0=exact, 1=approx>\n");
    return 1;
  }

  const char *docs_dir = argv[1];
  const char *pattern = argv[2];
  int mode = atoi(argv[3]);

  char files[MAX_FILES][512];
  int file_count = 0;

  int rank = 0, size = 1;
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  if (rank == 0)
  {
    preprocess_files(docs_dir, "/tmp/docsearch", files, &file_count);
  }

  MPI_Bcast(&file_count, 1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(files, MAX_FILES * 512, MPI_CHAR, 0, MPI_COMM_WORLD);

  
  if (rank == 0)
  {
    // Run serial (rank 0 only)
    double t1 = get_time_in_seconds();
    search_serial(files, file_count, pattern, mode);
    double t2 = get_time_in_seconds();
    printf("[SERIAL] Time: %.4f seconds\n\n", t2 - t1);

    // Run with OpenMP (rank 0 only) 
    double t3 = get_time_in_seconds();
    search_openmp(files, file_count, pattern, mode);
    double t4 = get_time_in_seconds();
    printf("[OPENMP] Time: %.4f seconds\n\n", t4 - t3);
  }

  // All ranks run MPI-only
  MPI_Barrier(MPI_COMM_WORLD);
  double t5 = MPI_Wtime();
  search_mpi(files, file_count, pattern, mode, rank, size);
  double t6 = MPI_Wtime();

  if (rank == 0)
  {
    printf("[MPI] Time: %.4f seconds\n\n", t6 - t5);
  }

  // All ranks run MPI+OpenMP
  MPI_Barrier(MPI_COMM_WORLD);
  double t7 = MPI_Wtime();
  search_mpi_openmp(files, file_count, pattern, mode, rank, size);
  double t8 = MPI_Wtime();

  if (rank == 0)
    printf("[MPI+OPENMP] Time: %.4f seconds\n", t4 - t3);

  // Finalize MPI
  MPI_Finalize();
  return 0;
}
