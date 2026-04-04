#ifndef GLIB_COMPAT_H
#define GLIB_COMPAT_H

#include <stddef.h>

typedef int gboolean;

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

typedef struct _GError {
   int   domain;
   int   code;
   char *message;
} GError;

typedef enum {
   G_OPTION_ARG_STRING = 0,
   G_OPTION_ARG_INT    = 1
} GOptionArg;

typedef struct _GOptionEntry {
   const char *long_name;
   char        short_name;
   int         flags;
   GOptionArg  arg;
   void       *arg_data;
   const char *description;
   const char *arg_description;
} GOptionEntry;

typedef struct _GOptionContext GOptionContext;

enum {
   G_OPTION_ERROR = 1,
   G_OPTION_ERROR_BAD_VALUE = 1
};

void g_free(void *ptr);
void *g_realloc(void *ptr, size_t size);
char *g_strdup(const char *str);
char *g_strndup(const char *str, size_t n);
size_t g_strlcpy(char *dest, const char *src, size_t dest_size);
void g_printerr(const char *format, ...);
void g_error_free(GError *error);
void g_set_error(GError **error, int domain, int code, const char *format, ...);

void *g_malloc_n(size_t n_blocks, size_t n_block_bytes);
void *g_malloc0_n(size_t n_blocks, size_t n_block_bytes);

#define g_new(type, n_structs) \
   ((type *)g_malloc_n((size_t)(n_structs), sizeof(type)))

#define g_new0(type, n_structs) \
   ((type *)g_malloc0_n((size_t)(n_structs), sizeof(type)))

GOptionContext *g_option_context_new(const char *parameter_string);
void g_option_context_set_summary(GOptionContext *context, const char *summary);
void g_option_context_add_main_entries(GOptionContext *context,
                                       const GOptionEntry *entries,
                                       const char *translation_domain);
gboolean g_option_context_parse(GOptionContext *context,
                                int *argc,
                                char ***argv,
                                GError **error);
void g_option_context_free(GOptionContext *context);

#endif