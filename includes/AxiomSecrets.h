#ifndef AXIOM_SECRETS_H
# define AXIOM_SECRETS_H

# include <cstddef>
# include <string.h>
# include <stdlib.h>
# include <libfsntfs.h>
# include <shlwapi.h>

const char* UTILS_strdup(const char* str);
const char** UTILS_strsplit(char const* s, char c);

typedef intptr_t libcerror_error_t;

#endif

