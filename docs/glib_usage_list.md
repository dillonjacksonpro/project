# GLib Usage Inventory (`mpi_orch.c`)

## Types / "Classes" used
- `gboolean`
- `GError`
- `GOptionEntry`
- `GOptionContext`

## Constants / enums used
- `TRUE`
- `FALSE`
- `G_OPTION_ARG_STRING`
- `G_OPTION_ARG_INT`
- `G_OPTION_ERROR`
- `G_OPTION_ERROR_BAD_VALUE`

## Function signatures used
- `MedianValue *g_new(MedianValue, size_t n)` (macro form)
- `SendBatch *g_new(SendBatch, int n)` (macro form)
- `NodeAgg *g_new0(NodeAgg, size_t n_threads)` (macro form)
- `StageBuf *g_new0(StageBuf, int N_MEDIAN_FIELDS)` (macro form)
- `GatherPayload *g_new(GatherPayload, size_t n)` (macro form)
- `void g_free(void *ptr)`
- `void *g_realloc(void *ptr, size_t size)`
- `char *g_strdup(const char *str)`
- `char *g_strndup(const char *str, size_t n)`
- `size_t g_strlcpy(char *dest, const char *src, size_t dest_size)`
- `void g_printerr(const char *format, ...)`
- `void g_error_free(GError *error)`
- `void g_set_error(GError **error, int domain, int code, const char *format, ...)`
- `GOptionContext *g_option_context_new(const char *parameter_string)`
- `void g_option_context_set_summary(GOptionContext *context, const char *summary)`
- `void g_option_context_add_main_entries(GOptionContext *context, const GOptionEntry *entries, const char *translation_domain)`
- `gboolean g_option_context_parse(GOptionContext *context, int *argc, char ***argv, GError **error)`
- `void g_option_context_free(GOptionContext *context)`
