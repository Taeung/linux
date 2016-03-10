/*
 * builtin-config.c
 *
 * Copyright (C) 2015, Taeung Song <treeze.taeung@gmail.com>
 *
 */
#include "builtin.h"

#include "perf.h"

#include "util/cache.h"
#include <subcmd/parse-options.h>
#include "util/util.h"
#include "util/debug.h"

static bool use_system_config, use_user_config;

static const char * const config_usage[] = {
	"perf config [<file-option>] [options]",
	NULL
};

enum actions {
	ACTION_LIST = 1
} actions;

static struct option config_options[] = {
	OPT_SET_UINT('l', "list", &actions,
		     "show current config variables", ACTION_LIST),
	OPT_BOOLEAN(0, "system", &use_system_config, "use system config file"),
	OPT_BOOLEAN(0, "user", &use_user_config, "use user config file"),
	OPT_END()
};

enum perf_config_idx {
	CONFIG_COLORS_TOP,
	CONFIG_COLORS_MEDIUM,
	CONFIG_COLORS_NORMAL,
	CONFIG_COLORS_SELECTED,
	CONFIG_COLORS_JUMP_ARROWS,
	CONFIG_COLORS_ADDR,
	CONFIG_COLORS_ROOT,
	CONFIG_TUI_REPORT,
	CONFIG_TUI_ANNOTATE,
	CONFIG_TUI_TOP,
	CONFIG_BUILDID_DIR,
	CONFIG_ANNOTATE_HIDE_SRC_CODE,
	CONFIG_ANNOTATE_USE_OFFSET,
	CONFIG_ANNOTATE_JUMP_ARROWS,
	CONFIG_ANNOTATE_SHOW_NR_JUMPS,
	CONFIG_ANNOTATE_SHOW_LINENR,
	CONFIG_ANNOTATE_SHOW_TOTAL_PERIOD,
	CONFIG_GTK_ANNOTATE,
	CONFIG_GTK_REPORT,
	CONFIG_GTK_TOP,
	CONFIG_PAGER_CMD,
	CONFIG_PAGER_REPORT,
	CONFIG_PAGER_ANNOTATE,
	CONFIG_PAGER_TOP,
	CONFIG_PAGER_DIFF,
	CONFIG_HELP_FORMAT,
	CONFIG_HELP_AUTOCORRECT,
	CONFIG_HIST_PERCENTAGE,
	CONFIG_UI_SHOW_HEADERS,
	CONFIG_CALL_GRAPH_RECORD_MODE,
	CONFIG_CALL_GRAPH_DUMP_SIZE,
	CONFIG_CALL_GRAPH_PRINT_TYPE,
	CONFIG_CALL_GRAPH_ORDER,
	CONFIG_CALL_GRAPH_SORT_KEY,
	CONFIG_CALL_GRAPH_THRESHOLD,
	CONFIG_CALL_GRAPH_PRINT_LIMIT,
	CONFIG_REPORT_GROUP,
	CONFIG_REPORT_CHILDREN,
	CONFIG_REPORT_PERCENT_LIMIT,
	CONFIG_REPORT_QUEUE_SIZE,
	CONFIG_TOP_CHILDREN,
	CONFIG_MAN_VIEWER,
	CONFIG_KMEM_DEFAULT,
	CONFIG_END
};

enum perf_config_type {
	CONFIG_TYPE_BOOL,
	CONFIG_TYPE_INT,
	CONFIG_TYPE_LONG,
	CONFIG_TYPE_U64,
	CONFIG_TYPE_FLOAT,
	CONFIG_TYPE_DOUBLE,
	CONFIG_TYPE_STRING
};

/*
 * Kinds of config file : user, system or both
 * (i.e user wide ~/.perfconfig and system wide $(sysconfdir)/perfconfig)
 */
#define CONFIG_KIND 3

enum perf_config_kind {
	ALL_CONFIG,
	USER_CONFIG,
	SYSTEM_CONFIG,
} config_kind;

struct perf_config_item {
	const char *section;
	const char *name;
	const char *value[CONFIG_KIND];
	union {
		bool b;
		int i;
		u32 l;
		u64 ll;
		float f;
		double d;
		const char *s;
	} default_value;
	enum perf_config_type type;
	bool is_custom;
	struct list_head list;
};

struct perf_config_set {
	const char *file_path;
	struct list_head config_list;
};

#define CONF_VAR(_sec, _name, _field, _val, _type)			\
	{ .section = _sec, .name = _name, .default_value._field = _val, .type = _type }

#define CONF_BOOL_VAR(_idx, _sec, _name, _val)				\
	[CONFIG_##_idx] = CONF_VAR(_sec, _name, b, _val, CONFIG_TYPE_BOOL)
#define CONF_INT_VAR(_idx, _sec, _name, _val)				\
	[CONFIG_##_idx] = CONF_VAR(_sec, _name, i, _val, CONFIG_TYPE_INT)
#define CONF_LONG_VAR(_idx, _sec, _name, _val)				\
	[CONFIG_##_idx] = CONF_VAR(_sec, _name, l, _val, CONFIG_TYPE_LONG)
#define CONF_U64_VAR(_idx, _sec, _name, _val)				\
	[CONFIG_##_idx] = CONF_VAR(_sec, _name, ll, _val, CONFIG_TYPE_U64)
#define CONF_FLOAT_VAR(_idx, _sec, _name, _val)				\
	[CONFIG_##_idx] = CONF_VAR(_sec, _name, f, _val, CONFIG_TYPE_FLOAT)
#define CONF_DOUBLE_VAR(_idx, _sec, _name, _val)			\
	[CONFIG_##_idx] = CONF_VAR(_sec, _name, d, _val, CONFIG_TYPE_DOUBLE)
#define CONF_STR_VAR(_idx, _sec, _name, _val)				\
	[CONFIG_##_idx] = CONF_VAR(_sec, _name, s, _val, CONFIG_TYPE_STRING)

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

	config->is_custom = true;
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

static int set_value(struct perf_config_item *config, const char* value)
{
	char *val = strdup(value);

	if (!val)
		return -1;

	config->value[ALL_CONFIG] = val;
	config->value[config_kind] = val;
	return 0;
}

static int collect_current_config(const char *var, const char *value,
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
		goto out_err;
	}

	config = find_config(config_list, section, name);
	if (!config) {
		config = add_config(config_list, section, name);
		if (!config) {
			free((char *)config->section);
			free((char *)config->name);
			goto out_err;
		}
	}
	ret = set_value(config, value);

out_err:
	free(key);
	return ret;
}

static struct perf_config_set *perf_config_set__init(struct perf_config_set *perf_configs)
{
	int i;
	struct list_head *head = &perf_configs->config_list;

	INIT_LIST_HEAD(&perf_configs->config_list);

	for (i = 0; i != CONFIG_END; i++) {
		struct perf_config_item *config = &default_configs[i];

		list_add_tail(&config->list, head);
	}
	return perf_configs;
}

static struct perf_config_set *perf_config_set__new(void)
{
	const char* sys_config_filename = perf_etc_perfconfig();
	const char* usr_config_filename = mkpath("%s/.perfconfig", getenv("HOME"));
	struct perf_config_set *perf_configs = zalloc(sizeof(*perf_configs));

	perf_config_set__init(perf_configs);

	config_kind = SYSTEM_CONFIG;
	perf_config_from_file(collect_current_config, sys_config_filename,
			      &perf_configs->config_list);
	config_kind = USER_CONFIG;
	perf_config_from_file(collect_current_config, usr_config_filename,
			      &perf_configs->config_list);

	perf_configs->file_path = usr_config_filename;
	if (use_system_config) {
		config_kind = SYSTEM_CONFIG;
		perf_configs->file_path = sys_config_filename;
	} else if (use_user_config)
		config_kind = USER_CONFIG;
	else
		config_kind = ALL_CONFIG;

	return perf_configs;
}

static void perf_config_set__delete(struct perf_config_set *perf_configs)
{
	struct perf_config_item *pos, *item;

	list_for_each_entry_safe(pos, item, &perf_configs->config_list, list) {
		list_del(&pos->list);
		if (pos->is_custom) {
			free((char *)pos->section);
			free((char *)pos->name);
		}
		free((char *)pos->value[USER_CONFIG]);
		free((char *)pos->value[SYSTEM_CONFIG]);
	}

	free(perf_configs);
}

static int show_config(struct list_head *config_list)
{
	struct perf_config_item *config;

	if (list_empty(config_list))
		return -1;

	list_for_each_entry(config, config_list, list) {
		const char *value = config->value[config_kind];

		if (value)
			printf("%s.%s=%s\n", config->section,
			       config->name, value);
	}

	return 0;
}

int cmd_config(int argc, const char **argv, const char *prefix __maybe_unused)
{
	/*
	 * The perf_configs that contains not only default configs
	 * but also key-value pairs from config files.
	 * (i.e user wide ~/.perfconfig and system wide $(sysconfdir)/perfconfig)
	 * This is designed to be used by several functions that handle config set.
	 */
	struct perf_config_set *perf_configs = perf_config_set__new();
	int ret = 0;

	argc = parse_options(argc, argv, config_options, config_usage,
			     PARSE_OPT_STOP_AT_NON_OPTION);

	if (use_system_config && use_user_config) {
		pr_err("Error: only one config file at a time\n");
		parse_options_usage(config_usage, config_options, "user", 0);
		parse_options_usage(NULL, config_options, "system", 0);
		ret = -1;
		goto out;
	}

	perf_configs = perf_config_set__new();

	switch (actions) {
	case ACTION_LIST:
		if (argc) {
			pr_err("Error: takes no arguments\n");
			parse_options_usage(config_usage, config_options, "l", 1);
		} else {
			ret = show_config(&perf_configs->config_list);
			if (ret < 0)
				pr_err("Nothing configured, "
				       "please check your %s \n", perf_configs->file_path);
		}
		break;
	default:
		usage_with_options(config_usage, config_options);
	}

out:
	perf_config_set__delete(perf_configs);
	return ret;
}
