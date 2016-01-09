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
	ACTION_LIST = 1,
	ACTION_LIST_ALL,
	ACTION_SKEL
} actions;

static struct option config_options[] = {
	OPT_SET_UINT('l', "list", &actions,
		     "show current config variables", ACTION_LIST),
	OPT_SET_UINT('a', "list-all", &actions,
		     "show current and all possible config"
		     " variables with default values", ACTION_LIST_ALL),
	OPT_SET_UINT('k', "skel", &actions,
		     "produce an skeleton with the possible"
		     " config variables", ACTION_SKEL),
	OPT_INCR('v', "verbose", &verbose, "Be more verbose"
		 " (show config description)"),
	OPT_BOOLEAN(0, "system", &use_system_config, "use system config file"),
	OPT_BOOLEAN(0, "user", &use_user_config, "use user config file"),
	OPT_END()
};

enum config_idx {
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
};

enum config_type {
	CONFIG_TYPE_BOOL,
	CONFIG_TYPE_INT,
	CONFIG_TYPE_LONG,
	CONFIG_TYPE_U64,
	CONFIG_TYPE_FLOAT,
	CONFIG_TYPE_DOUBLE,
	CONFIG_TYPE_STRING,
	/* special type */
	CONFIG_END
};

struct config_item {
	const char *section;
	const char *name;
	union {
		bool b;
		int i;
		u32 l;
		u64 ll;
		float f;
		double d;
		const char *s;
	} value;
	enum config_type type;
	const char *desc;
};

#define CONF_VAR(_sec, _name, _field, _val, _type, _desc)			\
	{ .section = _sec, .name = _name, .value._field = _val, .type = _type, .desc = _desc}

#define CONF_BOOL_VAR(_idx, _sec, _name, _val, _desc) \
	[CONFIG_##_idx] = CONF_VAR(_sec, _name, b, _val, CONFIG_TYPE_BOOL, _desc)
#define CONF_INT_VAR(_idx, _sec, _name, _val, _desc) \
	[CONFIG_##_idx] = CONF_VAR(_sec, _name, i, _val, CONFIG_TYPE_INT, _desc)
#define CONF_LONG_VAR(_idx, _sec, _name, _val, _desc) \
	[CONFIG_##_idx] = CONF_VAR(_sec, _name, l, _val, CONFIG_TYPE_LONG, _desc)
#define CONF_U64_VAR(_idx, _sec, _name, _val, _desc) \
	[CONFIG_##_idx] = CONF_VAR(_sec, _name, ll, _val, CONFIG_TYPE_U64, _desc)
#define CONF_FLOAT_VAR(_idx, _sec, _name, _val, _desc) \
	[CONFIG_##_idx] = CONF_VAR(_sec, _name, f, _val, CONFIG_TYPE_FLOAT, _desc)
#define CONF_DOUBLE_VAR(_idx, _sec, _name, _val, _desc) \
	[CONFIG_##_idx] = CONF_VAR(_sec, _name, d, _val, CONFIG_TYPE_DOUBLE, _desc)
#define CONF_STR_VAR(_idx, _sec, _name, _val, _desc) \
	[CONFIG_##_idx] = CONF_VAR(_sec, _name, s, _val, CONFIG_TYPE_STRING, _desc)
#define CONF_END() { .type = CONFIG_END }

struct config_item default_configs[] = {
	CONF_STR_VAR(COLORS_TOP, "colors", "top", "red, default",
		     "A overhead percentage which is more than 5%"),
	CONF_STR_VAR(COLORS_MEDIUM, "colors", "medium", "green, default",
		     "A overhead percentage which has more than 0.5%"),
	CONF_STR_VAR(COLORS_NORMAL, "colors", "normal", "lightgray, default",
		     "The rest of overhead percentages"),
	CONF_STR_VAR(COLORS_SELECTED, "colors", "selected", "white, lightgray",
		     "The current entry in a list of entries on TUI"),
	CONF_STR_VAR(COLORS_JUMP_ARROWS, "colors", "jump_arrows", "blue, default",
		     "Arrows and lines in jumps on  assembly code listings"),
	CONF_STR_VAR(COLORS_ADDR, "colors", "addr", "magenta, default",
		     "Addresses from 'annotate"),
	CONF_STR_VAR(COLORS_ROOT, "colors", "root", "white, blue",
		     "Headers in the output of a sub-command 'top'"),
	CONF_BOOL_VAR(TUI_REPORT, "tui", "report", true,
		      "TUI can be enabled or not"),
	CONF_BOOL_VAR(TUI_ANNOTATE, "tui", "annotate", true,
		      "TUI can be enabled or not"),
	CONF_BOOL_VAR(TUI_TOP, "tui", "top", true,
		      "TUI can be enabled or not"),
	CONF_STR_VAR(BUILDID_DIR, "buildid", "dir", "~/.debug",
		     "The directory location of binaries, shared libraries,"
		     " /proc/kallsyms and /proc/kcore files to be used"
		     " at analysis time"),
	CONF_BOOL_VAR(ANNOTATE_HIDE_SRC_CODE, "annotate", "hide_src_code", false,
		      "Print a list of assembly code without the source code or not"),
	CONF_BOOL_VAR(ANNOTATE_USE_OFFSET, "annotate", "use_offset", true,
		      "Addresses subtracted from a base address can be printed"),
	CONF_BOOL_VAR(ANNOTATE_JUMP_ARROWS, "annotate", "jump_arrows", true,
		      "Arrows for jump instruction can be printed or not"),
	CONF_BOOL_VAR(ANNOTATE_SHOW_NR_JUMPS, "annotate", "show_nr_jumps", false,
		      "The number of branches branching to that address can be printed"),
	CONF_BOOL_VAR(ANNOTATE_SHOW_LINENR, "annotate", "show_linenr", false,
		      "Show line numbers"),
	CONF_BOOL_VAR(ANNOTATE_SHOW_TOTAL_PERIOD, "annotate", "show_total_period", false,
		      "Show a column with the sum of periods"),
	CONF_BOOL_VAR(GTK_ANNOTATE, "gtk", "annotate", false,
		      "GTK can be enabled or not"),
	CONF_BOOL_VAR(GTK_REPORT, "gtk", "report", false,
		      "GTK can be enabled or not"),
	CONF_BOOL_VAR(GTK_TOP, "gtk", "top", false,
		      "GTK can be enabled or not"),
	CONF_BOOL_VAR(PAGER_CMD, "pager", "cmd", true,
		      "As stdio instead of TUI"),
	CONF_BOOL_VAR(PAGER_REPORT, "pager", "report", true,
		      "As stdio instead of TUI"),
	CONF_BOOL_VAR(PAGER_ANNOTATE, "pager", "annotate", true,
		      "As stdio instead of TUI"),
	CONF_BOOL_VAR(PAGER_TOP, "pager", "top", true,
		      "As stdio instead of TUI"),
	CONF_BOOL_VAR(PAGER_DIFF, "pager", "diff", true,
		      "As stdio instead of TUI"),
	CONF_STR_VAR(HELP_FORMAT, "help", "format", "man", "A format of manual page"),
	CONF_INT_VAR(HELP_AUTOCORRECT, "help", "autocorrect", 0,
		     "Automatically correct and execute mistyped commands after"
		     " waiting for the given number of deciseconds"),
	CONF_STR_VAR(HIST_PERCENTAGE, "hist", "percentage", "absolute",
		     "Control a way to calcurate overhead of filtered entries"),
	CONF_BOOL_VAR(UI_SHOW_HEADERS, "ui", "show-headers", true,
		      "Show or hide columns as header on TUI"),
	CONF_STR_VAR(CALL_GRAPH_RECORD_MODE, "call-graph", "record-mode", "fp",
		     "The mode can be 'fp' (frame pointer) and 'dwarf'"),
	CONF_LONG_VAR(CALL_GRAPH_DUMP_SIZE, "call-graph", "dump-size", 8192,
		      "The size of stack to dump in order to do post-unwinding"),
	CONF_STR_VAR(CALL_GRAPH_PRINT_TYPE, "call-graph", "print-type", "graph",
		     "The type can be graph (graph absolute), fractal (graph relative), fla"),
	CONF_STR_VAR(CALL_GRAPH_ORDER, "call-graph", "order", "callee",
		     "Controls print order of callchains (callee or caller)"),
	CONF_STR_VAR(CALL_GRAPH_SORT_KEY, "call-graph", "sort-key", "function",
		     "It can be 'function' or 'address'"),
	CONF_DOUBLE_VAR(CALL_GRAPH_THRESHOLD, "call-graph", "threshold", 0.5,
			"Small callchains can be omitted under a certain overhead (threshold)"),
	CONF_LONG_VAR(CALL_GRAPH_PRINT_LIMIT, "call-graph", "print-limit", 0,
		      "Control the number of callchains printed for a single entry"),
	CONF_BOOL_VAR(REPORT_GROUP, "report", "group", true,
		      "Show event group information together"),
	CONF_BOOL_VAR(REPORT_CHILDREN, "report", "children", true,
		      "Accumulate callchain of children and show total overhead or not"),
	CONF_FLOAT_VAR(REPORT_PERCENT_LIMIT, "report", "percent-limit", 0,
		       "Entries have overhead lower than this percentage will not be printed"),
	CONF_U64_VAR(REPORT_QUEUE_SIZE, "report", "queue-size", 0,
		     "The maximum allocation size for session's ordered events queue"),
	CONF_BOOL_VAR(TOP_CHILDREN, "top", "children", true,
		      "Similar as report.children"),
	CONF_STR_VAR(MAN_VIEWER, "man", "viewer", "man",
		     "Select manual tools that work a sub-command 'help'"),
	CONF_STR_VAR(KMEM_DEFAULT, "kmem", "default", "slab",
		     "Which allocator is analyzed between 'slab' and 'page"),
	CONF_END()
};

static struct config_section *find_section(struct list_head *sections,
					   const char *section_name)
{
	struct config_section *section;

	list_for_each_entry(section, sections, list)
		if (!strcmp(section->name, section_name))
			return section;

	return NULL;
}

static struct config_element *find_element(const char *name,
					   struct config_section *section)
{
	struct config_element *element;

	list_for_each_entry(element, &section->element_head, list)
		if (!strcmp(element->name, name))
			return element;

	return NULL;
}

static void find_config(struct list_head *sections,
			struct config_section **section,
			struct config_element **element,
			const char *section_name, const char *name)
{
	*section = find_section(sections, section_name);

	if (*section != NULL)
		*element = find_element(name, *section);
	else
		*element = NULL;
}

static struct config_section *init_section(const char *section_name)
{
	struct config_section *section;

	section = zalloc(sizeof(*section));
	if (!section)
		return NULL;

	INIT_LIST_HEAD(&section->element_head);
	section->name = strdup(section_name);
	if (!section->name) {
		pr_err("%s: strdup failed\n", __func__);
		free(section);
		return NULL;
	}

	return section;
}

static int add_element(struct list_head *head,
			      const char *name, const char *value)
{
	struct config_element *element;

	element = zalloc(sizeof(*element));
	if (!element)
		return -1;

	element->name = strdup(name);
	if (!element->name) {
		pr_err("%s: strdup failed\n", __func__);
		free(element);
		return -1;
	}
	if (value)
		element->value = (char *)value;
	else
		element->value = NULL;

	list_add_tail(&element->list, head);
	return 0;
}

static char *get_value(struct config_item *config)
{
	int ret = 0;
	char *value;

	if (config->type == CONFIG_TYPE_BOOL)
		ret = asprintf(&value, "%s",
			       config->value.b ? "true" : "false");
	else if (config->type == CONFIG_TYPE_INT)
		ret = asprintf(&value, "%d", config->value.i);
	else if (config->type == CONFIG_TYPE_LONG)
		ret = asprintf(&value, "%u", config->value.l);
	else if (config->type == CONFIG_TYPE_U64)
		ret = asprintf(&value, "%"PRId64, config->value.ll);
	else if (config->type == CONFIG_TYPE_FLOAT)
		ret = asprintf(&value, "%f", config->value.f);
	else if (config->type == CONFIG_TYPE_DOUBLE)
		ret = asprintf(&value, "%f", config->value.d);
	else
		ret = asprintf(&value, "%s", config->value.s);

	if (ret < 0)
		return NULL;

	return value;
}

static int show_skel_config(void)
{
	const char *section = "";

	for (int i = 0; default_configs[i].type != CONFIG_END; i++) {
		struct config_item *config = &default_configs[i];
		char *value = get_value(config);
		if (strcmp(section, config->section) != 0) {
			section = config->section;
			printf("\n[%s]\n", config->section);
		}
		if (verbose)
			printf("\t# %s\n", config->desc);
		printf("\t%s = %s\n", config->name, value);
		free(value);
	}

	return 0;
}

static int show_all_config(struct list_head *sections)
{
	int i;
	bool has_config;
	struct config_section *section;
	struct config_element *element;

	for (i = 0; default_configs[i].type != CONFIG_END; i++) {
		struct config_item *config = &default_configs[i];
		find_config(sections, &section, &element,
			    config->section, config->name);

		if (!element) {
			char *value = get_value(config);

			printf("%s.%s=%s\n", config->section, config->name, value);
			free(value);
		} else
			printf("%s.%s=%s\n", section->name,
			       element->name, element->value);
	}

	/* Print config variables the default configsets haven't */
	list_for_each_entry(section, sections, list) {
		list_for_each_entry(element, &section->element_head, list) {
			has_config = false;
			for (i = 0; default_configs[i].type != CONFIG_END; i++) {
				if (!strcmp(default_configs[i].section, section->name) &&
				    !strcmp(default_configs[i].name, element->name)) {
					has_config = true;
					break;
				}
			}
			if (!has_config)
				printf("%s.%s=%s\n", section->name,
				       element->name, element->value);
		}
	}

	return 0;
}

static int collect_current_config(const char *var, const char *value,
				  void *spec_sections)
{
	int ret = -1;
	char *ptr, *key;
	char *section_name, *name;
	struct config_section *section = NULL;
	struct config_element *element = NULL;
	struct list_head *sections = (struct list_head *)spec_sections;

	key = ptr = strdup(var);
	if (!key) {
		pr_err("%s: strdup failed\n", __func__);
		return -1;
	}

	section_name = strsep(&ptr, ".");
	name = ptr;
	if (name == NULL || value == NULL)
		goto out_err;

	find_config(sections, &section, &element, section_name, name);

	if (!section) {
		section = init_section(section_name);
		if (!section)
			goto out_err;
		list_add_tail(&section->list, sections);
	}

	value = strdup(value);
	if (!value) {
		pr_err("%s: strdup failed\n", __func__);
		goto out_err;
	}

	if (!element)
		add_element(&section->element_head, name, value);
	else {
		free(element->value);
		element->value = (char *)value;
	}

	ret = 0;
out_err:
	free(key);
	return ret;
}

static int show_config(struct list_head *sections)
{
	struct config_section *section;
	struct config_element *element;

	if (list_empty(sections))
		return -1;
	list_for_each_entry(section, sections, list) {
		list_for_each_entry(element, &section->element_head, list) {
			printf("%s.%s=%s\n", section->name,
				       element->name, element->value);
		}
	}

	return 0;
}

int cmd_config(int argc, const char **argv, const char *prefix __maybe_unused)
{
	int ret = 0;
	struct list_head sections;
	char *user_config = mkpath("%s/.perfconfig", getenv("HOME"));

	set_option_flag(config_options, 'l', "list", PARSE_OPT_EXCLUSIVE);
	set_option_flag(config_options, 'a', "list-all", PARSE_OPT_EXCLUSIVE);

	argc = parse_options(argc, argv, config_options, config_usage,
			     PARSE_OPT_STOP_AT_NON_OPTION);

	if (use_system_config && use_user_config) {
		pr_err("Error: only one config file at a time\n");
		parse_options_usage(config_usage, config_options, "user", 0);
		parse_options_usage(NULL, config_options, "system", 0);
		return -1;
	}

	INIT_LIST_HEAD(&sections);

	if (use_system_config)
		config_exclusive_filename = perf_etc_perfconfig();
	else if (use_user_config)
		config_exclusive_filename = user_config;

	perf_config(collect_current_config, &sections);

	switch (actions) {
	case ACTION_SKEL:
		if (argc)
			parse_options_usage(config_usage, config_options, "k", 1);
		else
			ret = show_skel_config();
		break;
	case ACTION_LIST_ALL:
		if (argc == 0) {
			ret = show_all_config(&sections);
			break;
		}
		/* fall through */
	case ACTION_LIST:
		if (argc) {
			pr_err("Error: takes no arguments\n");
			if (actions == ACTION_LIST_ALL)
				parse_options_usage(config_usage, config_options, "a", 1);
			else
				parse_options_usage(config_usage, config_options, "l", 1);
		} else {
			ret = show_config(&sections);
			if (ret < 0) {
				const char * config_filename = config_exclusive_filename;
				if (!config_exclusive_filename)
					config_filename = user_config;
				pr_err("Nothing configured, "
				       "please check your %s \n", config_filename);
			}
		}
		break;
	default:
		usage_with_options(config_usage, config_options);
	}

	return ret;
}
