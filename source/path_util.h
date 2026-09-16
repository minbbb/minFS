#ifndef PATH_UTIL_H
#define PATH_UTIL_H

#include <stddef.h>

const char *path_basename(const char *path);
void path_percent_decode(const char *in, char *out, size_t out_size);
void path_normalize(char *out, size_t out_size, const char *in);
int  path_from_query(const char *query, char *out, size_t out_size);
void path_parent_dir(const char *path, char *out, size_t size);
void logical_to_fs_path(char *out, size_t out_size, const char *logical);
void fs_to_logical_path(char *out, size_t out_size, const char *in);

#endif