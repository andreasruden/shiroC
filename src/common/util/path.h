#ifndef COMMON_UTIL_PATH__H
#define COMMON_UTIL_PATH__H

char* join_path(const char* path_a, const char* path_b);

const char* filename(const char* path);

// Returns true if `subpath` is equal to or nested inside `base_path`
bool path_is_subpath_of(const char* base_path, const char* subpath);

#endif
