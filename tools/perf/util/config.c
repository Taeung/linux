/*
 * config.c
 *
 * Helper functions for parsing config items.
 * Originally copied from GIT source.
 *
 * Copyright (C) Linus Torvalds, 2005
 * Copyright (C) Johannes Schindelin, 2005
 *
 */
#include "util.h"
#include "cache.h"
#include <subcmd/exec-cmd.h>
#include "util/hist.h"  /* perf_hist_config */
#include "util/llvm-utils.h"   /* perf_llvm_config */
#include "config.h"

#define MAXNAME (256)

#define DEBUG_CACHE_DIR ".debug"


char buildid_dir[MAXPATHLEN]; /* root dir for buildid, binary cache */

static FILE *config_file;
static const char *config_file_name;
static int config_linenr;
static int config_file_eof;

const char *config_exclusive_filename;

struct perf_config_item default_configs[] = {
	CONF_STR_VAR(COLORS_TOP, "colors", "top", "red, default"),
	CONF_STR_VAR(COLORS_MEDIUM, "colors", "medium", "green, default"),
	CONF_STR_VAR(COLORS_NORMAL, "colors", "normal", "lightgray, default"),
	CONF_STR_VAR(COLORS_SELECTED, "colors", "selected", "white, lightgray"),
	CONF_STR_VAR(COLORS_JUMP_ARROWS, "colors", "jump_arrows", "blue, default"),
	CONF_STR_VAR(COLORS_ADDR, "colors", "addr", "magenta, default"),
	CONF_STR_VAR(COLORS_ROOT, "colors", "root", "white, blue"),
	CONF_BOOL_VAR(TUI_REPORT, "tui", "report", true),
	CONF_BOOL_VAR(TUI_ANNOTATE, "tui", "annotate", true),
	CONF_BOOL_VAR(TUI_TOP, "tui", "top", true),
	CONF_STR_VAR(BUILDID_DIR, "buildid", "dir", "~/.debug"),
	CONF_BOOL_VAR(ANNOTATE_HIDE_SRC_CODE, "annotate", "hide_src_code", false),
	CONF_BOOL_VAR(ANNOTATE_USE_OFFSET, "annotate", "use_offset", true),
	CONF_BOOL_VAR(ANNOTATE_JUMP_ARROWS, "annotate", "jump_arrows", true),
	CONF_BOOL_VAR(ANNOTATE_SHOW_NR_JUMPS, "annotate", "show_nr_jumps", false),
	CONF_BOOL_VAR(ANNOTATE_SHOW_LINENR, "annotate", "show_linenr", false),
	CONF_BOOL_VAR(ANNOTATE_SHOW_TOTAL_PERIOD, "annotate", "show_total_period", false),
	CONF_BOOL_VAR(GTK_ANNOTATE, "gtk", "annotate", false),
	CONF_BOOL_VAR(GTK_REPORT, "gtk", "report", false),
	CONF_BOOL_VAR(GTK_TOP, "gtk", "top", false),
	CONF_BOOL_VAR(PAGER_CMD, "pager", "cmd", true),
	CONF_BOOL_VAR(PAGER_REPORT, "pager", "report", true),
	CONF_BOOL_VAR(PAGER_ANNOTATE, "pager", "annotate", true),
	CONF_BOOL_VAR(PAGER_TOP, "pager", "top", true),
	CONF_BOOL_VAR(PAGER_DIFF, "pager", "diff", true),
	CONF_STR_VAR(HELP_FORMAT, "help", "format", "man"),
	CONF_INT_VAR(HELP_AUTOCORRECT, "help", "autocorrect", 0),
	CONF_STR_VAR(HIST_PERCENTAGE, "hist", "percentage", "absolute"),
	CONF_BOOL_VAR(UI_SHOW_HEADERS, "ui", "show-headers", true),
	CONF_STR_VAR(CALL_GRAPH_RECORD_MODE, "call-graph", "record-mode", "fp"),
	CONF_LONG_VAR(CALL_GRAPH_DUMP_SIZE, "call-graph", "dump-size", 8192),
	CONF_STR_VAR(CALL_GRAPH_PRINT_TYPE, "call-graph", "print-type", "graph"),
	CONF_STR_VAR(CALL_GRAPH_ORDER, "call-graph", "order", "callee"),
	CONF_STR_VAR(CALL_GRAPH_SORT_KEY, "call-graph", "sort-key", "function"),
	CONF_DOUBLE_VAR(CALL_GRAPH_THRESHOLD, "call-graph", "threshold", 0.5),
	CONF_LONG_VAR(CALL_GRAPH_PRINT_LIMIT, "call-graph", "print-limit", 0),
	CONF_BOOL_VAR(REPORT_GROUP, "report", "group", true),
	CONF_BOOL_VAR(REPORT_CHILDREN, "report", "children", true),
	CONF_FLOAT_VAR(REPORT_PERCENT_LIMIT, "report", "percent-limit", 0),
	CONF_U64_VAR(REPORT_QUEUE_SIZE, "report", "queue-size", 0),
	CONF_BOOL_VAR(TOP_CHILDREN, "top", "children", true),
	CONF_STR_VAR(MAN_VIEWER, "man", "viewer", "man"),
	CONF_STR_VAR(KMEM_DEFAULT, "kmem", "default", "slab"),
};

static int get_next_char(void)
{
	int c;
	FILE *f;

	c = '\n';
	if ((f = config_file) != NULL) {
		c = fgetc(f);
		if (c == '\r') {
			/* DOS like systems */
			c = fgetc(f);
			if (c != '\n') {
				ungetc(c, f);
				c = '\r';
			}
		}
		if (c == '\n')
			config_linenr++;
		if (c == EOF) {
			config_file_eof = 1;
			c = '\n';
		}
	}
	return c;
}

static char *parse_value(void)
{
	static char value[1024];
	int quote = 0, comment = 0, space = 0;
	size_t len = 0;

	for (;;) {
		int c = get_next_char();

		if (len >= sizeof(value) - 1)
			return NULL;
		if (c == '\n') {
			if (quote)
				return NULL;
			value[len] = 0;
			return value;
		}
		if (comment)
			continue;
		if (isspace(c) && !quote) {
			space = 1;
			continue;
		}
		if (!quote) {
			if (c == ';' || c == '#') {
				comment = 1;
				continue;
			}
		}
		if (space) {
			if (len)
				value[len++] = ' ';
			space = 0;
		}
		if (c == '\\') {
			c = get_next_char();
			switch (c) {
			case '\n':
				continue;
			case 't':
				c = '\t';
				break;
			case 'b':
				c = '\b';
				break;
			case 'n':
				c = '\n';
				break;
			/* Some characters escape as themselves */
			case '\\': case '"':
				break;
			/* Reject unknown escape sequences */
			default:
				return NULL;
			}
			value[len++] = c;
			continue;
		}
		if (c == '"') {
			quote = 1-quote;
			continue;
		}
		value[len++] = c;
	}
}

static inline int iskeychar(int c)
{
	return isalnum(c) || c == '-' || c == '_';
}

static int get_value(config_fn_t fn, void *data, char *name, unsigned int len)
{
	int c;
	char *value;

	/* Get the full name */
	for (;;) {
		c = get_next_char();
		if (config_file_eof)
			break;
		if (!iskeychar(c))
			break;
		name[len++] = c;
		if (len >= MAXNAME)
			return -1;
	}
	name[len] = 0;
	while (c == ' ' || c == '\t')
		c = get_next_char();

	value = NULL;
	if (c != '\n') {
		if (c != '=')
			return -1;
		value = parse_value();
		if (!value)
			return -1;
	}
	return fn(name, value, data);
}

static int get_extended_base_var(char *name, int baselen, int c)
{
	do {
		if (c == '\n')
			return -1;
		c = get_next_char();
	} while (isspace(c));

	/* We require the format to be '[base "extension"]' */
	if (c != '"')
		return -1;
	name[baselen++] = '.';

	for (;;) {
		int ch = get_next_char();

		if (ch == '\n')
			return -1;
		if (ch == '"')
			break;
		if (ch == '\\') {
			ch = get_next_char();
			if (ch == '\n')
				return -1;
		}
		name[baselen++] = ch;
		if (baselen > MAXNAME / 2)
			return -1;
	}

	/* Final ']' */
	if (get_next_char() != ']')
		return -1;
	return baselen;
}

static int get_base_var(char *name)
{
	int baselen = 0;

	for (;;) {
		int c = get_next_char();
		if (config_file_eof)
			return -1;
		if (c == ']')
			return baselen;
		if (isspace(c))
			return get_extended_base_var(name, baselen, c);
		if (!iskeychar(c) && c != '.')
			return -1;
		if (baselen > MAXNAME / 2)
			return -1;
		name[baselen++] = tolower(c);
	}
}

static int perf_parse_file(config_fn_t fn, void *data)
{
	int comment = 0;
	int baselen = 0;
	static char var[MAXNAME];

	/* U+FEFF Byte Order Mark in UTF8 */
	static const unsigned char *utf8_bom = (unsigned char *) "\xef\xbb\xbf";
	const unsigned char *bomptr = utf8_bom;

	for (;;) {
		int line, c = get_next_char();

		if (bomptr && *bomptr) {
			/* We are at the file beginning; skip UTF8-encoded BOM
			 * if present. Sane editors won't put this in on their
			 * own, but e.g. Windows Notepad will do it happily. */
			if ((unsigned char) c == *bomptr) {
				bomptr++;
				continue;
			} else {
				/* Do not tolerate partial BOM. */
				if (bomptr != utf8_bom)
					break;
				/* No BOM at file beginning. Cool. */
				bomptr = NULL;
			}
		}
		if (c == '\n') {
			if (config_file_eof)
				return 0;
			comment = 0;
			continue;
		}
		if (comment || isspace(c))
			continue;
		if (c == '#' || c == ';') {
			comment = 1;
			continue;
		}
		if (c == '[') {
			baselen = get_base_var(var);
			if (baselen <= 0)
				break;
			var[baselen++] = '.';
			var[baselen] = 0;
			continue;
		}
		if (!isalpha(c))
			break;
		var[baselen] = tolower(c);

		/*
		 * The get_value function might or might not reach the '\n',
		 * so saving the current line number for error reporting.
		 */
		line = config_linenr;
		if (get_value(fn, data, var, baselen+1) < 0) {
			config_linenr = line;
			break;
		}
	}
	die("bad config file line %d in %s", config_linenr, config_file_name);
}

static int parse_unit_factor(const char *end, unsigned long *val)
{
	if (!*end)
		return 1;
	else if (!strcasecmp(end, "k")) {
		*val *= 1024;
		return 1;
	}
	else if (!strcasecmp(end, "m")) {
		*val *= 1024 * 1024;
		return 1;
	}
	else if (!strcasecmp(end, "g")) {
		*val *= 1024 * 1024 * 1024;
		return 1;
	}
	return 0;
}

static int perf_parse_llong(const char *value, long long *ret)
{
	if (value && *value) {
		char *end;
		long long val = strtoll(value, &end, 0);
		unsigned long factor = 1;

		if (!parse_unit_factor(end, &factor))
			return 0;
		*ret = val * factor;
		return 1;
	}
	return 0;
}

static int perf_parse_long(const char *value, long *ret)
{
	if (value && *value) {
		char *end;
		long val = strtol(value, &end, 0);
		unsigned long factor = 1;
		if (!parse_unit_factor(end, &factor))
			return 0;
		*ret = val * factor;
		return 1;
	}
	return 0;
}

static void die_bad_config(const char *name)
{
	if (config_file_name)
		die("bad config value for '%s' in %s", name, config_file_name);
	die("bad config value for '%s'", name);
}

u64 perf_config_u64(const char *name, const char *value)
{
	long long ret = 0;

	if (!perf_parse_llong(value, &ret))
		die_bad_config(name);
	return (u64) ret;
}

int perf_config_int(const char *name, const char *value)
{
	long ret = 0;
	if (!perf_parse_long(value, &ret))
		die_bad_config(name);
	return ret;
}

static int perf_config_bool_or_int(const char *name, const char *value, int *is_bool)
{
	*is_bool = 1;
	if (!value)
		return 1;
	if (!*value)
		return 0;
	if (!strcasecmp(value, "true") || !strcasecmp(value, "yes") || !strcasecmp(value, "on"))
		return 1;
	if (!strcasecmp(value, "false") || !strcasecmp(value, "no") || !strcasecmp(value, "off"))
		return 0;
	*is_bool = 0;
	return perf_config_int(name, value);
}

int perf_config_bool(const char *name, const char *value)
{
	int discard;
	return !!perf_config_bool_or_int(name, value, &discard);
}

const char *perf_config_dirname(const char *name, const char *value)
{
	if (!name)
		return NULL;
	return value;
}

static int perf_default_core_config(const char *var __maybe_unused,
				    const char *value __maybe_unused)
{
	/* Add other config variables here. */
	return 0;
}

static int perf_ui_config(const char *var, const char *value)
{
	/* Add other config variables here. */
	if (!strcmp(var, "ui.show-headers")) {
		symbol_conf.show_hist_headers = perf_config_bool(var, value);
		return 0;
	}
	return 0;
}

int perf_default_config(const char *var, const char *value,
			void *dummy __maybe_unused)
{
	if (!prefixcmp(var, "core."))
		return perf_default_core_config(var, value);

	if (!prefixcmp(var, "hist."))
		return perf_hist_config(var, value);

	if (!prefixcmp(var, "ui."))
		return perf_ui_config(var, value);

	if (!prefixcmp(var, "call-graph."))
		return perf_callchain_config(var, value);

	if (!prefixcmp(var, "llvm."))
		return perf_llvm_config(var, value);

	/* Add other config variables here. */
	return 0;
}

static int perf_config_from_file(config_fn_t fn, const char *filename, void *data)
{
	int ret;
	FILE *f = fopen(filename, "r");

	ret = -1;
	if (f) {
		config_file = f;
		config_file_name = filename;
		config_linenr = 1;
		config_file_eof = 0;
		ret = perf_parse_file(fn, data);
		fclose(f);
		config_file_name = NULL;
	}
	return ret;
}

const char *perf_etc_perfconfig(void)
{
	static const char *system_wide;
	if (!system_wide)
		system_wide = system_path(ETC_PERFCONFIG);
	return system_wide;
}

static int perf_env_bool(const char *k, int def)
{
	const char *v = getenv(k);
	return v ? perf_config_bool(k, v) : def;
}

static int perf_config_system(void)
{
	return !perf_env_bool("PERF_CONFIG_NOSYSTEM", 0);
}

static int perf_config_global(void)
{
	return !perf_env_bool("PERF_CONFIG_NOGLOBAL", 0);
}

int perf_config(config_fn_t fn, void *data)
{
	int ret = 0, found = 0;
	const char *home = NULL;

	/* Setting $PERF_CONFIG makes perf read _only_ the given config file. */
	if (config_exclusive_filename)
		return perf_config_from_file(fn, config_exclusive_filename, data);
	if (perf_config_system() && !access(perf_etc_perfconfig(), R_OK)) {
		ret += perf_config_from_file(fn, perf_etc_perfconfig(),
					    data);
		found += 1;
	}

	home = getenv("HOME");
	if (perf_config_global() && home) {
		char *user_config = strdup(mkpath("%s/.perfconfig", home));
		struct stat st;

		if (user_config == NULL) {
			warning("Not enough memory to process %s/.perfconfig, "
				"ignoring it.", home);
			goto out;
		}

		if (stat(user_config, &st) < 0)
			goto out_free;

		if (st.st_uid && (st.st_uid != geteuid())) {
			warning("File %s not owned by current user or root, "
				"ignoring it.", user_config);
			goto out_free;
		}

		if (!st.st_size)
			goto out_free;

		ret += perf_config_from_file(fn, user_config, data);
		found += 1;
out_free:
		free(user_config);
	}
out:
	if (found == 0)
		return -1;
	return ret;
}

static struct perf_config_item *find_config(struct list_head *config_list,
				       const char *section,
				       const char *name)
{
	struct perf_config_item *config;

	list_for_each_entry(config, config_list, list) {
		if (!strcmp(config->section, section) &&
		    !strcmp(config->name, name))
			return config;
	}

	return NULL;
}

static struct perf_config_item *add_config(struct list_head *config_list,
					   const char *section,
					   const char *name)
{
	struct perf_config_item *config = zalloc(sizeof(*config));

	if (!config)
		return NULL;

	config->section = strdup(section);
	if (!section)
		goto out_err;

	config->name = strdup(name);
	if (!name) {
		free((char *)config->section);
		goto out_err;
	}

	list_add_tail(&config->list, config_list);
	return config;

out_err:
	free(config);
	pr_err("%s: strdup failed\n", __func__);
	return NULL;
}

static int set_value(struct perf_config_item *config, const char *value)
{
	char *val = strdup(value);

	if (!val)
		return -1;
	config->value = val;

	return 0;
}

static int collect_config(const char *var, const char *value,
			  void *configs)
{
	int ret = 0;
	char *ptr, *key;
	char *section, *name;
	struct perf_config_item *config;
	struct list_head *config_list = configs;

	key = ptr = strdup(var);
	if (!key) {
		pr_err("%s: strdup failed\n", __func__);
		return -1;
	}

	section = strsep(&ptr, ".");
	name = ptr;
	if (name == NULL || value == NULL) {
		ret = -1;
		goto out_free;
	}

	config = find_config(config_list, section, name);
	if (!config) {
		config = add_config(config_list, section, name);
		if (!config) {
			free((char *)config->section);
			free((char *)config->name);
			ret = -1;
			goto out_free;
		}
	}

	ret = set_value(config, value);

out_free:
	free(key);
	return ret;
}

struct perf_config_set *perf_config_set__new(void)
{
	struct perf_config_set *perf_configs = zalloc(sizeof(*perf_configs));

	if (!perf_configs)
		return NULL;

	INIT_LIST_HEAD(&perf_configs->config_list);
	perf_config(collect_config, &perf_configs->config_list);

	return perf_configs;
}

void perf_config_set__delete(struct perf_config_set *perf_configs)
{
	struct perf_config_item *pos, *item;

	list_for_each_entry_safe(pos, item, &perf_configs->config_list, list) {
		list_del(&pos->list);
		free((char *)pos->section);
		free((char *)pos->name);
		free(pos->value);
		free(pos);
	}

	free(perf_configs);
}

/*
 * Call this to report error for your variable that should not
 * get a boolean value (i.e. "[my] var" means "true").
 */
int config_error_nonbool(const char *var)
{
	return error("Missing value for '%s'", var);
}

struct buildid_dir_config {
	char *dir;
};

static int buildid_dir_command_config(const char *var, const char *value,
				      void *data)
{
	struct buildid_dir_config *c = data;
	const char *v;

	/* same dir for all commands */
	if (!strcmp(var, "buildid.dir")) {
		v = perf_config_dirname(var, value);
		if (!v)
			return -1;
		strncpy(c->dir, v, MAXPATHLEN-1);
		c->dir[MAXPATHLEN-1] = '\0';
	}
	return 0;
}

static void check_buildid_dir_config(void)
{
	struct buildid_dir_config c;
	c.dir = buildid_dir;
	perf_config(buildid_dir_command_config, &c);
}

void set_buildid_dir(const char *dir)
{
	if (dir)
		scnprintf(buildid_dir, MAXPATHLEN-1, "%s", dir);

	/* try config file */
	if (buildid_dir[0] == '\0')
		check_buildid_dir_config();

	/* default to $HOME/.debug */
	if (buildid_dir[0] == '\0') {
		char *v = getenv("HOME");
		if (v) {
			snprintf(buildid_dir, MAXPATHLEN-1, "%s/%s",
				 v, DEBUG_CACHE_DIR);
		} else {
			strncpy(buildid_dir, DEBUG_CACHE_DIR, MAXPATHLEN-1);
		}
		buildid_dir[MAXPATHLEN-1] = '\0';
	}
	/* for communicating with external commands */
	setenv("PERF_BUILDID_DIR", buildid_dir, 1);
}
