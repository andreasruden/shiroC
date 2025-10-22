#include "path.h"
#include "../containers/string.h"
#include <string.h>

static constexpr char PATH_SEPARATOR = '/';

char* join_path(const char* path_a, const char* path_b)
{
    string_t result = STRING_INIT;

    // Append first path
    if (path_a != nullptr && path_a[0] != '\0') {
        string_append_cstr(&result, path_a);

        // Check if we need to add a separator
        size_t len_a = strlen(path_a);
        if (len_a > 0 && path_a[len_a - 1] != PATH_SEPARATOR) {
            if (path_b != nullptr && path_b[0] != '\0' && path_b[0] != PATH_SEPARATOR) {
                string_append_char(&result, PATH_SEPARATOR);
            }
        }
    }

    // Append second path
    if (path_b != nullptr && path_b[0] != '\0') {
        string_append_cstr(&result, path_b);
    }

    return string_release(&result);
}

bool path_is_subpath_of(const char* base_path, const char* subpath)
{
    if (base_path == nullptr || subpath == nullptr)
        return false;

    size_t base_len = strlen(base_path);
    size_t sub_len = strlen(subpath);

    if (sub_len < base_len)
        return false;

    if (strncmp(base_path, subpath, base_len) != 0)
        return false;

    if (sub_len == base_len)
        return true;

    // Check that the next character is a path separator (or we're at end)
    // This prevents "/foo" from matching "/foobar"
    return subpath[base_len] == PATH_SEPARATOR;
}
