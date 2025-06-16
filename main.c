#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <omp.h>
#include "file_utils.h"
#include "matcher.h"

#define MAX_FILES 1000

int main(int argc, char *argv[])
{
  // Check if the correct number of arguments is provided
  if (argc != 4)
  {
    printf("Usage: mpirun -np <n> ./docsearch <docs_folder> <pattern> <mode: 0=exact, 1=approx>\n");
    return 1;
  }

  const char *docs_dir = argv[1]; // document folder 
  const char *pattern = argv[2]; // search string
  int mode = atoi(argv[3]); // search mode (0 for exact, 1 for approx)

  MPI_Init(&argc, &argv);
  int rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank); // get the rank of the process
  MPI_Comm_size(MPI_COMM_WORLD, &size); // get the total number of processes

  char files[MAX_FILES][512]; // can store 1000 file paths with 512 characters each
  int file_count = 0;

  // for preprocessing, use master process
  // preprocessing includes converting .pdf, .docx to .txt
  if (rank == 0)
  {
    preprocess_files(docs_dir, "/tmp/docsearch", files, &file_count); // temporary directory for processed files
  }

  MPI_Bcast(&file_count, 1, MPI_INT, 0, MPI_COMM_WORLD); // tell all processes how many files there are
  MPI_Bcast(files, MAX_FILES * 512, MPI_CHAR, 0, MPI_COMM_WORLD); // broadcast the file paths to all processes

  // Each process will search through the files assigned to it
  
  int found = 0;

#pragma omp parallel for schedule(dynamic)
  // Each process starts searching from its own rank and skips size steps each time - round robin distribution
  for (int i = rank; i < file_count; i += size)
  {
    if (do_search(files[i], pattern, mode))
    {
#pragma omp critical
// to prevent multiple threads from printing at the same time
      {
        printf("Rank %d Thread %d found in %s\n", rank, omp_get_thread_num(), files[i]);
        found = 1;
      }
    }
  }

  // to check if any process found a match
  int global_found;
  MPI_Reduce(&found, &global_found, 1, MPI_INT, MPI_LOR, 0, MPI_COMM_WORLD);

  // if there is any match, global_found will be 1. otherwise it will be 0
  if (rank == 0 && !global_found)
  {
    printf("No match found.\n");
  }

  MPI_Finalize();
  return 0;
}
