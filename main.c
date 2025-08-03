#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <mpi.h>
#include <omp.h>
#include "file_utils.h"
#include "matcher.h"

#define MAX_FILES 1000
#define MAX_FILENAME_LEN 512

// Structure to store search results for accuracy comparison
typedef struct {
    char filename[MAX_FILENAME_LEN];
    int found;
} SearchResult;

// Comparison function for qsort
int compare_results(const void *a, const void *b) {
    const SearchResult *ra = (const SearchResult *)a;
    const SearchResult *rb = (const SearchResult *)b;
    return strcmp(ra->filename, rb->filename);
}

// Function to normalize file paths for comparison (remove temp directory prefixes)
void normalize_filename(const char *path, char *normalized) {
    const char *filename = strrchr(path, '/');
    if (filename) {
        strcpy(normalized, filename + 1);
    } else {
        strcpy(normalized, path);
    }
}

// Serial
int search_serial(char files[][512], int file_count, const char *pattern, int mode, SearchResult *results)
{
    int found_count = 0;
    for (int i = 0; i < file_count; i++)
    {
        normalize_filename(files[i], results[i].filename);
        results[i].found = do_search(files[i], pattern, mode);
        if (results[i].found)
        {
            printf("[SERIAL] Found in %s\n", files[i]);
            found_count++;
        }
    }
    if (found_count == 0)
        printf("[SERIAL] No match found.\n");
    return found_count;
}

// OpenMP - Fixed version with proper synchronization
int search_openmp(char files[][512], int file_count, const char *pattern, int mode, SearchResult *results)
{
    omp_set_num_threads(1);

#pragma omp parallel
    {
#pragma omp single
        {
            int actual_threads = omp_get_num_threads();
            printf("[OPENMP] Using %d threads\n", actual_threads);
        }
    }

    // Pre-populate filenames to avoid race conditions
    for (int i = 0; i < file_count; i++) {
        normalize_filename(files[i], results[i].filename);
        results[i].found = 0;
    }

    int found_count = 0;

#pragma omp parallel for schedule(dynamic) reduction(+:found_count)
    for (int i = 0; i < file_count; i++)
    {
        int search_result = do_search(files[i], pattern, mode);
        results[i].found = search_result;
        if (search_result)
        {
            found_count++;
#pragma omp critical
            {
                printf("[OPENMP] Thread %d found in %s\n", omp_get_thread_num(), files[i]);
            }
        }
    }

    if (found_count == 0)
        printf("[OPENMP] No match found.\n");
    return found_count;
}

// MPI
int search_mpi(char files[][512], int file_count, const char *pattern, int mode, int rank, int size, SearchResult *results)
{
    int local_found_count = 0;
    
    // Initialize results array with normalized filenames
    for (int i = 0; i < file_count; i++) {
        normalize_filename(files[i], results[i].filename);
        results[i].found = 0;
    }

    // Each process searches its assigned files
    for (int i = rank; i < file_count; i += size)
    {
        results[i].found = do_search(files[i], pattern, mode);
        if (results[i].found)
        {
            printf("[MPI] Rank %d found in %s\n", rank, files[i]);
            local_found_count++;
        }
    }

    // Gather all results to rank 0
    if (rank == 0) {
        // Receive results from other processes
        for (int proc = 1; proc < size; proc++) {
            for (int i = proc; i < file_count; i += size) {
                int remote_result;
                MPI_Recv(&remote_result, 1, MPI_INT, proc, i, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                results[i].found = remote_result;
                if (remote_result) local_found_count++;
            }
        }
    } else {
        // Send results to rank 0
        for (int i = rank; i < file_count; i += size) {
            MPI_Send(&results[i].found, 1, MPI_INT, 0, i, MPI_COMM_WORLD);
        }
    }

    if (rank == 0 && local_found_count == 0)
    {
        printf("[MPI] No match found.\n");
    }
    
    return local_found_count;
}

// Optimized Hybrid MPI+OpenMP
int search_mpi_openmp(char files[][512], int file_count, const char *pattern, int mode, int rank, int size, SearchResult *results)
{
    // Set optimal number of OpenMP threads per MPI process
    int optimal_threads = 16 / size; // Distribute threads across MPI processes
    if (optimal_threads < 1) optimal_threads = 1;
    if (optimal_threads > 8) optimal_threads = 8; // Cap to avoid oversubscription
    
    omp_set_num_threads(optimal_threads);
    
    if (rank == 0) {
        printf("[MPI+OPENMP] Using %d MPI processes with %d OpenMP threads each\n", size, optimal_threads);
    }

    int local_found_count = 0;
    
    // Initialize results array with normalized filenames
    for (int i = 0; i < file_count; i++) {
        normalize_filename(files[i], results[i].filename);
        results[i].found = 0;
    }

    // Create array of files this process will handle
    int *my_files = malloc(file_count * sizeof(int));
    int my_file_count = 0;
    for (int i = rank; i < file_count; i += size) {
        my_files[my_file_count++] = i;
    }

    // Use OpenMP to parallelize within each MPI process
#pragma omp parallel for schedule(dynamic, 1) reduction(+:local_found_count)
    for (int j = 0; j < my_file_count; j++)
    {
        int i = my_files[j];
        int search_result = do_search(files[i], pattern, mode);
        results[i].found = search_result;
        if (search_result)
        {
            local_found_count++;
#pragma omp critical
            {
                printf("[MPI+OPENMP] Rank %d Thread %d found in %s\n", rank, omp_get_thread_num(), files[i]);
            }
        }
    }

    free(my_files);

    // Gather all results to rank 0
    if (rank == 0) {
        // Receive results from other processes
        for (int proc = 1; proc < size; proc++) {
            int remote_count;
            MPI_Recv(&remote_count, 1, MPI_INT, proc, 999, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            local_found_count += remote_count;
            
            for (int i = proc; i < file_count; i += size) {
                int remote_result;
                MPI_Recv(&remote_result, 1, MPI_INT, proc, i, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                results[i].found = remote_result;
            }
        }
    } else {
        // Send local count first, then individual results
        MPI_Send(&local_found_count, 1, MPI_INT, 0, 999, MPI_COMM_WORLD);
        for (int i = rank; i < file_count; i += size) {
            MPI_Send(&results[i].found, 1, MPI_INT, 0, i, MPI_COMM_WORLD);
        }
    }

    if (rank == 0 && local_found_count == 0)
    {
        printf("[MPI+OPENMP] No match found.\n");
    }
    
    return local_found_count;
}

// Function to compare accuracy between methods
void compare_accuracy(SearchResult *ref_results, SearchResult *test_results, int file_count, const char *method_name)
{
    int matches = 0;
    int ref_found = 0;
    int test_found = 0;
    
    // Sort both arrays by filename for proper comparison
    qsort(ref_results, file_count, sizeof(SearchResult), compare_results);
    qsort(test_results, file_count, sizeof(SearchResult), compare_results);
    
    // Count reference results
    for (int i = 0; i < file_count; i++) {
        if (ref_results[i].found) ref_found++;
    }
    
    // Count test results and matches
    for (int i = 0; i < file_count; i++) {
        if (test_results[i].found) test_found++;
        if (ref_results[i].found == test_results[i].found) matches++;
    }
    
    double accuracy = (double)matches / file_count * 100.0;
    printf("[ACCURACY] %s: %.2f%% (%d/%d files match reference, found %d vs reference %d)\n", 
           method_name, accuracy, matches, file_count, test_found, ref_found);
    
    // Show discrepancies if any
    if (matches < file_count) {
        printf("[DISCREPANCIES] %s:\n", method_name);
        int discrepancy_count = 0;

        // Limit to first 10 discrepancies
        for (int i = 0; i < file_count && discrepancy_count < 10; i++) { 
            if (ref_results[i].found != test_results[i].found) {
                printf("  File %s: Reference=%s, %s=%s\n", 
                       ref_results[i].filename,
                       ref_results[i].found ? "FOUND" : "NOT_FOUND",
                       method_name,
                       test_results[i].found ? "FOUND" : "NOT_FOUND");
                discrepancy_count++;
            }
        }
        if (file_count - matches > 10) {
            printf("  ... and %d more discrepancies\n", file_count - matches - 10);
        }
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

    // Results storage for accuracy comparison
    SearchResult serial_results[MAX_FILES];
    SearchResult openmp_results[MAX_FILES];
    SearchResult mpi_results[MAX_FILES];
    SearchResult hybrid_results[MAX_FILES];
    
    // Timing storage
    double serial_time = 0.0, openmp_time = 0.0, mpi_time = 0.0, hybrid_time = 0.0;
    int serial_found = 0, openmp_found = 0;

    // Storage for timing breakdown
    double serial_preprocess_time = 0.0, serial_search_time = 0.0;
    double openmp_preprocess_time = 0.0, openmp_search_time = 0.0;

    // === SERIAL (Full Pipeline) ===
    if (rank == 0)
    {
        printf("=== SERIAL METHOD (Preprocessing + Search) ===\n");
        file_count = 0;
        
        // Measure preprocessing time
        double preprocess_start = get_time_in_seconds();
        preprocess_files(docs_dir, "/tmp/doc_serial", files, &file_count, 1);
        double preprocess_end = get_time_in_seconds();
        serial_preprocess_time = preprocess_end - preprocess_start;
        
        // Measure search time
        double search_start = get_time_in_seconds();
        serial_found = search_serial(files, file_count, pattern, mode, serial_results);
        double search_end = get_time_in_seconds();
        serial_search_time = search_end - search_start;
        
        serial_time = serial_preprocess_time + serial_search_time;
        printf("[SERIAL] Preprocessing: %.4f seconds\n", serial_preprocess_time);
        printf("[SERIAL] Search: %.4f seconds\n", serial_search_time);
        printf("[SERIAL] Total: %.4f seconds, Found: %d files\n\n", serial_time, serial_found);
    }

    // === OPENMP (Full Pipeline) ===
    if (rank == 0)
    {
        printf("=== OPENMP METHOD (Preprocessing + Search) ===\n");
        file_count = 0;
        
        // Measure preprocessing time
        double preprocess_start = get_time_in_seconds();
        preprocess_files(docs_dir, "/tmp/doc_openmp", files, &file_count, 2);
        double preprocess_end = get_time_in_seconds();
        openmp_preprocess_time = preprocess_end - preprocess_start;
        
        // Measure search time
        double search_start = get_time_in_seconds();
        openmp_found = search_openmp(files, file_count, pattern, mode, openmp_results);
        double search_end = get_time_in_seconds();
        openmp_search_time = search_end - search_start;
        
        openmp_time = openmp_preprocess_time + openmp_search_time;
        printf("[OPENMP] Preprocessing: %.4f seconds\n", openmp_preprocess_time);
        printf("[OPENMP] Search: %.4f seconds\n", openmp_search_time);
        printf("[OPENMP] Total: %.4f seconds, Found: %d files\n\n", openmp_time, openmp_found);
        
        // Compare with serial
        compare_accuracy(serial_results, openmp_results, file_count, "OPENMP");
        printf("\n");
    }

    // === MPI (Full Pipeline) ===
    MPI_Barrier(MPI_COMM_WORLD);
    if (rank == 0) printf("=== MPI METHOD (Preprocessing + Search) ===\n");
    double t5 = MPI_Wtime();

    // MPI preprocessing - only rank 0 does preprocessing
    double mpi_preprocess_time = 0.0, mpi_search_time = 0.0;
    if (rank == 0)
    {
        file_count = 0;
        double preprocess_start = MPI_Wtime();
        preprocess_files(docs_dir, "/tmp/doc_mpi", files, &file_count, 1); // Use serial preprocessing
        double preprocess_end = MPI_Wtime();
        mpi_preprocess_time = preprocess_end - preprocess_start;
    }

    // Broadcast the preprocessed files to all processes
    MPI_Bcast(&file_count, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(files, MAX_FILES * 512, MPI_CHAR, 0, MPI_COMM_WORLD);

    // MPI search phase
    MPI_Barrier(MPI_COMM_WORLD);
    double search_start = MPI_Wtime();
    int mpi_found = search_mpi(files, file_count, pattern, mode, rank, size, mpi_results);
    double search_end = MPI_Wtime();
    mpi_search_time = search_end - search_start;
    
    double t6 = MPI_Wtime();
    mpi_time = t6 - t5;
    
    if (rank == 0) {
        printf("[MPI] Preprocessing: %.4f seconds\n", mpi_preprocess_time);
        printf("[MPI] Search: %.4f seconds\n", mpi_search_time);
        printf("[MPI] Total: %.4f seconds, Found: %d files\n", mpi_time, mpi_found);
        compare_accuracy(serial_results, mpi_results, file_count, "MPI");
        printf("\n");
    }

    // === MPI + OpenMP (Full Pipeline) ===
    MPI_Barrier(MPI_COMM_WORLD);
    if (rank == 0) printf("=== MPI + OPENMP METHOD (Preprocessing + Search) ===\n");
    double t7 = MPI_Wtime();

    // Hybrid preprocessing - only rank 0 does preprocessing with OpenMP
    double hybrid_preprocess_time = 0.0, hybrid_search_time = 0.0;
    if (rank == 0)
    {
        file_count = 0;
        double preprocess_start = MPI_Wtime();
        preprocess_files(docs_dir, "/tmp/doc_hybrid", files, &file_count, 4); // Use hybrid preprocessing
        double preprocess_end = MPI_Wtime();
        hybrid_preprocess_time = preprocess_end - preprocess_start;
    }

    // Broadcast the preprocessed files to all processes
    MPI_Bcast(&file_count, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(files, MAX_FILES * 512, MPI_CHAR, 0, MPI_COMM_WORLD);

    // Hybrid search phase
    MPI_Barrier(MPI_COMM_WORLD);
    search_start = MPI_Wtime();
    int hybrid_found = search_mpi_openmp(files, file_count, pattern, mode, rank, size, hybrid_results);
     search_end = MPI_Wtime();
    hybrid_search_time = search_end - search_start;
    
    double t8 = MPI_Wtime();
    hybrid_time = t8 - t7;
    
    if (rank == 0) {
        printf("[MPI+OPENMP] Preprocessing: %.4f seconds\n", hybrid_preprocess_time);
        printf("[MPI+OPENMP] Search: %.4f seconds\n", hybrid_search_time);
        printf("[MPI+OPENMP] Total: %.4f seconds, Found: %d files\n", hybrid_time, hybrid_found);
        compare_accuracy(serial_results, hybrid_results, file_count, "MPI+OPENMP");
        
        // Final summary with detailed breakdown
        printf("\n=== PERFORMANCE SUMMARY ===\n");
        printf("Method          | Preprocessing | Search    | Total     | Found\n");
        printf("----------------|---------------|-----------|-----------|------\n");
        printf("Serial          | %8.4f      | %8.4f  | %8.4f  | %d\n", 
               serial_preprocess_time, serial_search_time, serial_time, serial_found);
        printf("OpenMP          | %8.4f      | %8.4f  | %8.4f  | %d\n", 
               openmp_preprocess_time, openmp_search_time, openmp_time, openmp_found);
        printf("MPI             | %8.4f      | %8.4f  | %8.4f  | %d\n", 
               mpi_preprocess_time, mpi_search_time, mpi_time, mpi_found);
        printf("MPI+OpenMP      | %8.4f      | %8.4f  | %8.4f  | %d\n", 
               hybrid_preprocess_time, hybrid_search_time, hybrid_time, hybrid_found);
        
        // Calculate speedups for both phases and total
        printf("\n=== SPEEDUP ANALYSIS ===\n");
        printf("Phase           | OpenMP | MPI    | MPI+OpenMP\n");
        printf("----------------|--------|--------|-----------\n");
        printf("Preprocessing   | %5.2fx  | %5.2fx  | %5.2fx\n", 
               serial_preprocess_time / openmp_preprocess_time,
               serial_preprocess_time / mpi_preprocess_time,
               serial_preprocess_time / hybrid_preprocess_time);
        printf("Search          | %5.2fx  | %5.2fx  | %5.2fx\n", 
               serial_search_time / openmp_search_time,
               serial_search_time / mpi_search_time,
               serial_search_time / hybrid_search_time);
        printf("Total           | %5.2fx  | %5.2fx  | %5.2fx\n", 
               serial_time / openmp_time,
               serial_time / mpi_time,
               serial_time / hybrid_time);
        
        // Calculate efficiency
        printf("\n=== EFFICIENCY ANALYSIS ===\n");
        printf("OpenMP:    %.1f%% (%d threads)\n", 
               (serial_time / openmp_time) / 16.0 * 100, omp_get_num_threads());
        printf("MPI:       %.1f%% (%d processes)\n", 
               (serial_time / mpi_time) / size * 100, size);
        printf("Hybrid:    %.1f%% (%d processes × %d threads)\n", 
               (serial_time / hybrid_time) / (size * (16/size)) * 100, size, 16/size);
               
        // Performance insights
        printf("\n=== PERFORMANCE INSIGHTS ===\n");
        if (openmp_preprocess_time < serial_preprocess_time) {
            printf("✓ OpenMP preprocessing shows %.2fx speedup\n", 
                   serial_preprocess_time / openmp_preprocess_time);
        }
        if (hybrid_preprocess_time < serial_preprocess_time) {
            printf("✓ Hybrid preprocessing shows %.2fx speedup\n", 
                   serial_preprocess_time / hybrid_preprocess_time);
        }
        if (hybrid_search_time < mpi_search_time) {
            printf("✓ Hybrid search is %.2fx faster than pure MPI\n", 
                   mpi_search_time / hybrid_search_time);
        }
        if (openmp_search_time < serial_search_time) {
            printf("✓ OpenMP search shows %.2fx speedup\n", 
                   serial_search_time / openmp_search_time);
        }
    }

    MPI_Finalize();
    return 0;
}