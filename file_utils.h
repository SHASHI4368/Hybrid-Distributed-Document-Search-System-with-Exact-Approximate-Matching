#ifndef FILE_UTILS_H
#define FILE_UTILS_H

int is_supported_file(const char *filename);
void list_files(const char *directory, char files[][512], int *count);
void preprocess_files(const char *src_dir, const char *out_dir, char output_files[][512], int *count);

#endif
