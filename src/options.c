#include "options.h"

gboolean
parse_options(int *argc, char ***argv, Options *options, GError **error)
{
   GOptionEntry entries[] = {
      { "input",   'i', 0, G_OPTION_ARG_STRING, &options->input_path,
        "Input directory to process", "DIR" },
      { "nodes",   'n', 0, G_OPTION_ARG_STRING, &options->nodes_map,
        "Node-to-cores mapping (e.g., node001:0-15,node002:0-15)", "MAP" },
      { "threads", 't', 0, G_OPTION_ARG_INT,    &options->max_threads,
        "Maximum threads per node", "NUM" },
      { NULL }
   };

   GOptionContext *context = g_option_context_new("[DIR]");
   if (context == NULL) {
      if (error)
         g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "failed to allocate option context");
      return FALSE;
   }
   g_option_context_set_summary(context,
      "Distribute lines from input files across MPI ranks.");
   g_option_context_add_main_entries(context, entries, NULL);

   gboolean ok = g_option_context_parse(context, argc, argv, error);
   if (!ok) {
      g_option_context_free(context);
      return FALSE;
   }

   if (options->input_path == NULL) {
      if (*argc == 2) {
         options->input_path = g_strdup((*argv)[1]);
         if (options->input_path == NULL) {
            if (error)
               g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                           "out of memory while duplicating input path");
            g_option_context_free(context);
            return FALSE;
         }
      } else {
         if (error)
            g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                        "missing input directory; pass --input DIR or DIR");
         g_option_context_free(context);
         return FALSE;
      }
   } else if (*argc > 1) {
      if (error)
         g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "too many positional arguments; pass a single DIR or --input DIR");
      g_option_context_free(context);
      return FALSE;
   }

   g_option_context_free(context);
   return TRUE;
}

void
options_free(Options *options)
{
   if (options == NULL)
      return;
   g_free(options->input_path);
   g_free(options->nodes_map);
   options->input_path = NULL;
   options->nodes_map = NULL;
   options->max_threads = 0;
}
