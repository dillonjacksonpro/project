#include "glib_compat.h"

#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct _GOptionContext {
   char               *parameter_string;
   char               *summary;
   const GOptionEntry *entries;
};

static char *
g_strdup_vprintf(const char *format, va_list ap)
{
   va_list ap_copy;
   va_copy(ap_copy, ap);
   int needed = vsnprintf(NULL, 0, format, ap_copy);
   va_end(ap_copy);

   if (needed < 0)
      return NULL;

   size_t len = (size_t)needed;
   char *buf = malloc(len + 1);
   if (buf == NULL)
      return NULL;

   int written = vsnprintf(buf, len + 1, format, ap);
   if (written < 0) {
      free(buf);
      return NULL;
   }
   return buf;
}

void *
g_malloc_n(size_t n_blocks, size_t n_block_bytes)
{
   if (n_block_bytes != 0 && n_blocks > SIZE_MAX / n_block_bytes)
      return NULL;
   return malloc(n_blocks * n_block_bytes);
}

void *
g_malloc0_n(size_t n_blocks, size_t n_block_bytes)
{
   if (n_block_bytes != 0 && n_blocks > SIZE_MAX / n_block_bytes)
      return NULL;
   return calloc(n_blocks, n_block_bytes);
}

void
g_free(void *ptr)
{
   free(ptr);
}

void *
g_realloc(void *ptr, size_t size)
{
   return realloc(ptr, size);
}

char *
g_strdup(const char *str)
{
   if (str == NULL)
      return NULL;
   size_t len = strlen(str);
   char *copy = malloc(len + 1);
   if (copy == NULL)
      return NULL;
   memcpy(copy, str, len + 1);
   return copy;
}

char *
g_strndup(const char *str, size_t n)
{
   if (str == NULL)
      return NULL;
   size_t len = 0;
   while (len < n && str[len] != '\0')
      len++;

   char *copy = malloc(len + 1);
   if (copy == NULL)
      return NULL;

   memcpy(copy, str, len);
   copy[len] = '\0';
   return copy;
}

size_t
g_strlcpy(char *dest, const char *src, size_t dest_size)
{
   size_t src_len = strlen(src);
   if (dest_size > 0) {
      size_t copy_len = src_len < (dest_size - 1) ? src_len : (dest_size - 1);
      memcpy(dest, src, copy_len);
      dest[copy_len] = '\0';
   }
   return src_len;
}

void
g_printerr(const char *format, ...)
{
   va_list ap;
   va_start(ap, format);
   vfprintf(stderr, format, ap);
   va_end(ap);
}

void
g_error_free(GError *error)
{
   if (error == NULL)
      return;
   free(error->message);
   free(error);
}

void
g_set_error(GError **error, int domain, int code, const char *format, ...)
{
   if (error == NULL)
      return;

   g_error_free(*error);
   *error = malloc(sizeof(**error));
   if (*error == NULL)
      return;

   (*error)->domain = domain;
   (*error)->code = code;

   va_list ap;
   va_start(ap, format);
   (*error)->message = g_strdup_vprintf(format, ap);
   va_end(ap);

   if ((*error)->message == NULL)
      (*error)->message = g_strdup("failed to format error message");
}

static const GOptionEntry *
find_long_entry(const GOptionEntry *entries, const char *name)
{
   if (entries == NULL || name == NULL)
      return NULL;
   for (const GOptionEntry *e = entries; e->long_name != NULL; e++) {
      if (strcmp(e->long_name, name) == 0)
         return e;
   }
   return NULL;
}

static const GOptionEntry *
find_short_entry(const GOptionEntry *entries, char short_name)
{
   if (entries == NULL || short_name == '\0')
      return NULL;
   for (const GOptionEntry *e = entries; e->long_name != NULL; e++) {
      if (e->short_name == short_name)
         return e;
   }
   return NULL;
}

static gboolean
set_entry_value(const GOptionEntry *entry, const char *value, GError **error)
{
   if (entry == NULL || entry->arg_data == NULL)
      return FALSE;

   if (entry->arg == G_OPTION_ARG_STRING) {
      char **target = entry->arg_data;
      g_free(*target);
      *target = g_strdup(value);
      if (*target == NULL) {
         g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "out of memory while setting --%s", entry->long_name);
         return FALSE;
      }
      return TRUE;
   }

   if (entry->arg == G_OPTION_ARG_INT) {
      char *end = NULL;
      errno = 0;
      long parsed = strtol(value, &end, 10);
      if (errno != 0 || end == value || *end != '\0' ||
          parsed < INT32_MIN || parsed > INT32_MAX) {
         g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "invalid integer value '%s' for --%s", value, entry->long_name);
         return FALSE;
      }
      int *target = entry->arg_data;
      *target = (int)parsed;
      return TRUE;
   }

   g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
               "unsupported option type for --%s", entry->long_name);
   return FALSE;
}

GOptionContext *
g_option_context_new(const char *parameter_string)
{
   GOptionContext *context = g_new0(GOptionContext, 1);
   if (context == NULL)
      return NULL;

   context->parameter_string = g_strdup(parameter_string);
   if (parameter_string != NULL && context->parameter_string == NULL) {
      g_free(context);
      return NULL;
   }
   return context;
}

void
g_option_context_set_summary(GOptionContext *context, const char *summary)
{
   if (context == NULL)
      return;
   g_free(context->summary);
   context->summary = g_strdup(summary);
}

void
g_option_context_add_main_entries(GOptionContext *context,
                                  const GOptionEntry *entries,
                                  const char *translation_domain)
{
   (void)translation_domain;
   if (context == NULL)
      return;
   context->entries = entries;
}

gboolean
g_option_context_parse(GOptionContext *context,
                       int *argc,
                       char ***argv,
                       GError **error)
{
   if (context == NULL || argc == NULL || argv == NULL || *argv == NULL)
      return FALSE;

   char **args = *argv;
   int write_idx = 1;

   for (int i = 1; i < *argc; i++) {
      const char *arg = args[i];

      if (arg[0] != '-' || arg[1] == '\0') {
         args[write_idx++] = args[i];
         continue;
      }

      if (strcmp(arg, "--") == 0) {
         for (int j = i + 1; j < *argc; j++)
            args[write_idx++] = args[j];
         break;
      }

      if (arg[0] == '-' && arg[1] == '-') {
         const char *name = arg + 2;
         const char *value = NULL;
         char *owned_name = NULL;

         const char *eq = strchr(name, '=');
         if (eq != NULL) {
            size_t name_len = (size_t)(eq - name);
            owned_name = g_strndup(name, name_len);
            name = owned_name;
            value = eq + 1;
         }

         const GOptionEntry *entry = find_long_entry(context->entries, name);
         if (entry == NULL) {
            g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                        "unknown option '--%s'", name);
            g_free(owned_name);
            return FALSE;
         }

         if (value == NULL) {
            if (i + 1 >= *argc) {
               g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                           "missing value for option '--%s'", name);
               g_free(owned_name);
               return FALSE;
            }
            value = args[++i];
         }

         gboolean ok = set_entry_value(entry, value, error);
         g_free(owned_name);
         if (!ok)
            return FALSE;
         continue;
      }

      const GOptionEntry *entry = find_short_entry(context->entries, arg[1]);
      if (entry == NULL || arg[2] != '\0') {
         g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "unknown option '%s'", arg);
         return FALSE;
      }
      if (i + 1 >= *argc) {
         g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     "missing value for option '%s'", arg);
         return FALSE;
      }

      if (!set_entry_value(entry, args[++i], error))
         return FALSE;
   }

   *argc = write_idx;
   args[write_idx] = NULL;
   return TRUE;
}

void
g_option_context_free(GOptionContext *context)
{
   if (context == NULL)
      return;
   g_free(context->parameter_string);
   g_free(context->summary);
   g_free(context);
}