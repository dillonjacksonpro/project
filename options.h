#ifndef OPTIONS_H
#define OPTIONS_H

#include "glib_compat.h"

typedef struct {
   char *input_path;
   char *nodes_map;
   int   max_threads;
} Options;

gboolean parse_options(int *argc, char ***argv, Options *options, GError **error);
void options_free(Options *options);

#endif
