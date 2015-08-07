/*
 * builtin-config.c
 *
 * Copyright (C) 2015, Taeung Song <treeze.taeung@gmail.com>
 *
 */
#include "builtin.h"

#include "perf.h"

#include "util/cache.h"
#include "util/parse-options.h"
#include "util/util.h"
#include "util/debug.h"

static bool use_system_config, use_user_config;
static const char *config_file_name;

static const char * const config_usage[] = {
	"perf config [options]",
	NULL
};

enum actions {
	ACTION_LIST = 1,
	ACTION_LIST_ALL
} actions;

static const struct option config_options[] = {
	OPT_GROUP("Config file location"),
	OPT_BOOLEAN(0, "system", &use_system_config, "use system config file"),
	OPT_BOOLEAN(0, "user", &use_user_config, "use user config file"),
	OPT_GROUP("Action"),
	OPT_SET_UINT('l', "list", &actions,
		     "show current config variables", ACTION_LIST),
	OPT_SET_UINT('a', "list-all", &actions,
		     "show current and all possible config variables with default values",
		     ACTION_LIST_ALL),
	OPT_END()
};

/* section names */
#define COLORS "colors"
#define TUI "tui"
#define BUILDID "buildid"
#define ANNOTATE "annotate"
#define GTK "gtk"
#define PAGER "pager"
#define HELP "help"
#define HIST "hist"
#define UI "ui"
#define CALL_GRAPH "call-graph"
#define REPORT "report"
#define TOP "top"
#define MAN "man"
#define KMEM "kmem"

/* config variable types */
#define TYPE_INT "int"
#define TYPE_LONG "long"
#define TYPE_DIRNAME "dirname"
#define TYPE_BOOL "bool"
#define TYPE_ON_OFF "on_off"

static struct default_configset {
	const char *section_name;
	const char *name, *value, *type;

} default_configsets[] = {
	{
		.section_name = COLORS,
		.name = "top",
		.value = "red, default",
		.type = NULL,
	},
	{
		.section_name = COLORS,
		.name = "medium",
		.value = "green, default",
		.type = NULL,
	},
	{
		.section_name = COLORS,
		.name = "normal",
		.value = "lightgray, default",
		.type = NULL,
	},
	{
		.section_name = COLORS,
		.name = "selected",
		.value = "white, lightgray",
		.type = NULL,
	},
	{
		.section_name = COLORS,
		.name = "code",
		.value = "blue, default",
		.type = NULL,
	},
	{
		.section_name = COLORS,
		.name = "addr",
		.value = "magenta, default",
		.type = NULL,
	},
	{
		.section_name = COLORS,
		.name = "root",
		.value = "white, blue",
		.type = NULL,
	},
	{
		.section_name = TUI,
		.name = "report",
		.value = "on",
		.type = TYPE_ON_OFF,
	},
	{
		.section_name = TUI,
		.name = "annotate",
		.value = "on",
		.type = TYPE_ON_OFF,
	},
	{
		.section_name = TUI,
		.name = "top",
		.value = "on",
		.type = TYPE_ON_OFF,
	},
	{
		.section_name = BUILDID,
		.name = "dir",
		.value = "~/.debug",
		.type = TYPE_DIRNAME,
	},
	{
		.section_name = ANNOTATE,
		.name = "hide_src_code",
		.value = "false",
		.type = TYPE_BOOL,
	},
	{
		.section_name = ANNOTATE,
		.name = "use_offset",
		.value = "true",
		.type = TYPE_BOOL,
	},
	{
		.section_name = ANNOTATE,
		.name = "jump_arrows",
		.value = "true",
		.type = TYPE_BOOL,
	},
	{
		.section_name = ANNOTATE,
		.name = "show_nr_jumps",
		.value = "false",
		.type = TYPE_BOOL,
	},
	{
		.section_name = GTK,
		.name = "annotate",
		.value = "off",
		.type = TYPE_ON_OFF,
	},
	{
		.section_name = GTK,
		.name = "report",
		.value = "off",
		.type = TYPE_ON_OFF,
	},
	{
		.section_name = GTK,
		.name = "top",
		.value = "off",
		.type = TYPE_ON_OFF,
	},
	{
		.section_name = PAGER,
		.name = "cmd",
		.value = "true",
		.type = TYPE_BOOL,
	},
	{
		.section_name = PAGER,
		.name = "report",
		.value = "true",
		.type = TYPE_BOOL,
	},
	{
		.section_name = PAGER,
		.name = "annotate",
		.value = "true",
		.type = TYPE_BOOL,
	},
	{
		.section_name = PAGER,
		.name = "record",
		.value = "true",
		.type = TYPE_BOOL,
	},
	{
		.section_name = PAGER,
		.name = "top",
		.value = "true",
		.type = TYPE_BOOL,
	},
	{
		.section_name = PAGER,
		.name = "diff",
		.value = "true",
		.type = TYPE_BOOL,
	},
	{
		.section_name = HELP,
		.name = "format",
		.value = "man",
		.type = NULL,
	},
	{
		.section_name = HELP,
		.name = "autocorrect",
		.value = "0",
		.type = NULL,
	},
	{
		.section_name = HIST,
		.name = "percentage",
		.value = "absolute",
		.type = NULL,
	},
	{
		.section_name = UI,
		.name = "show-headers",
		.value = "true",
		.type = TYPE_BOOL,
	},
	{
		.section_name = CALL_GRAPH,
		.name = "record-mode",
		.value = "fp",
	},
	{
		.section_name = CALL_GRAPH,
		.name = "dump-size",
		.value = "8192",
		.type = TYPE_INT,
	},
	{
		.section_name = CALL_GRAPH,
		.name = "print-type",
		.value = "fractal",
		.type = NULL,
	},
	{
		.section_name = CALL_GRAPH,
		.name = "order",
		.value = "caller",
		.type = NULL,
	},
	{
		.section_name = CALL_GRAPH,
		.name = "sort-key",
		.value = "function",
		.type = NULL,
	},
	{
		.section_name = CALL_GRAPH,
		.name = "threshold",
		.value = "0.5",
		.type = TYPE_LONG,
	},
	{
		.section_name = CALL_GRAPH,
		.name = "print-limit",
		.value = "0",
		.type = TYPE_INT,
	},
	{
		.section_name = REPORT,
		.name = "children",
		.value = "false",
		.type = TYPE_BOOL,
	},
	{
		.section_name = REPORT,
		.name = "percent-limit",
		.value = "0",
		.type = TYPE_INT,
	},
	{
		.section_name = REPORT,
		.name = "queue-size",
		.value = "0",
		.type = TYPE_INT,
	},
	{
		.section_name = TOP,
		.name = "children",
		.value = "false",
		.type = TYPE_BOOL,
	},
	{
		.section_name = MAN,
		.name = "viewer",
		.value = "man",
		.type = NULL,
	},
	{
		.section_name = KMEM,
		.name = "default",
		.value = "slab",
		.type = NULL,
	},
	{
		.section_name = NULL,
		.name = NULL,
		.value = NULL,
		.type = NULL,
	},
};

static struct config_section *find_section(const char *section_name)
{
	struct config_section *section_node;

	list_for_each_entry(section_node, &sections, list)
		if (!strcmp(section_node->name, section_name))
			return section_node;

	return NULL;
}

static struct config_element *find_element(const char *name,
					   struct config_section *section_node)
{
	struct config_element *element_node;

	list_for_each_entry(element_node, &section_node->element_head, list)
		if (!strcmp(element_node->name, name))
			return element_node;

	return NULL;
}

static struct config_section *init_section(const char *section_name)
{
	struct config_section *section_node;

	section_node = zalloc(sizeof(*section_node));
	if (!section_node)
		return NULL;

	INIT_LIST_HEAD(&section_node->element_head);
	section_node->name = strdup(section_name);
	if (!section_node->name) {
		pr_err("%s: strdup failed\n", __func__);
		free(section_node);
		return NULL;
	}

	return section_node;
}

static int add_element(struct list_head *head,
			      const char *name, const char *value)
{
	struct config_element *element_node;

	element_node = zalloc(sizeof(*element_node));
	if (!element_node)
		return -1;

	element_node->name = strdup(name);
	if (!element_node->name) {
		pr_err("%s: strdup failed\n", __func__);
		goto out_free;
	}
	if (value)
		element_node->value = (char *)value;
	else
		element_node->value = NULL;

	list_add_tail(&element_node->list, head);
	return 0;

out_free:
	free(element_node);
	return -1;
}

static int show_config(const char *key, const char *value,
		       void *cb __maybe_unused)
{
	if (value)
		printf("%s=%s\n", key, value);
	else
		printf("%s\n", key);

	return 0;
}

static void find_config(struct config_section **section_node,
			struct config_element **element_node,
			const char *section_name, const char *name)
{
	*section_node = find_section(section_name);

	if (*section_node != NULL)
		*element_node = find_element(name, *section_node);
	else
		*element_node = NULL;
}

static char *normalize_value(const char *section_name, const char *name, const char *value)
{
	int i, ret = 0;
	char key[BUFSIZ];
	char *normalized;

	scnprintf(key, sizeof(key), "%s.%s", section_name, name);
	for (i = 0; default_configsets[i].section_name != NULL; i++) {
		struct default_configset *config = &default_configsets[i];

		if (!strcmp(config->section_name, section_name)
		    && !strcmp(config->name, name)) {
			if (!config->type)
				ret = asprintf(&normalized, "%s", value);
			else if (!strcmp(config->type, TYPE_BOOL))
				ret = asprintf(&normalized, "%s",
					       perf_config_bool(key, value) ? "true" : "false");
			else if (!strcmp(config->type, TYPE_ON_OFF))
				ret = asprintf(&normalized, "%s",
					       perf_config_bool(key, value) ? "on" : "off");
			else if (!strcmp(config->type, TYPE_INT))
				ret = asprintf(&normalized, "%d",
					       perf_config_int(key, value));
			else if (!strcmp(config->type, TYPE_LONG))
				ret = asprintf(&normalized, "%"PRId64,
					       perf_config_u64(key, value));
			else if (!strcmp(config->type, TYPE_DIRNAME))
				ret = asprintf(&normalized, "%s",
					       perf_config_dirname(key, value));
			if (ret < 0)
				return NULL;

			return normalized;
		}
	}

	normalized = strdup(value);
	if (!normalized) {
		pr_err("%s: strdup failed\n", __func__);
		return NULL;
	}

	return normalized;
}

static int collect_current_config(const char *var, const char *value,
			  void *cb __maybe_unused)
{
	struct config_section *section_node;
	char *key = strdup(var);
	char *section_name, *name;

	if (!key) {
		pr_err("%s: strdup failed\n", __func__);
		return -1;
	}

	section_name = strsep(&key, ".");
	name = strsep(&key, ".");
	free(key);
	if (name == NULL)
		return -1;

	section_node = find_section(section_name);
	if (!section_node) {
		section_node = init_section(section_name);
		if (!section_node)
			return -1;
		list_add_tail(&section_node->list, &sections);
	}

	return add_element(&section_node->element_head, name,
			   normalize_value(section_name, name, value));
}

static int show_all_config(void)
{
	int i;
	bool has_config;
	struct config_section *section_node;
	struct config_element *element_node;

	for (i = 0; default_configsets[i].section_name != NULL; i++) {
		find_config(&section_node, &element_node,
			    default_configsets[i].section_name, default_configsets[i].name);

		if (!element_node)
			printf("%s.%s=%s\n", default_configsets[i].section_name,
			       default_configsets[i].name, default_configsets[i].value);
		else
			printf("%s.%s=%s\n", section_node->name,
			       element_node->name, element_node->value);
	}

	/* Print config variables the default configsets haven't */
	list_for_each_entry(section_node, &sections, list) {
		list_for_each_entry(element_node, &section_node->element_head, list) {
			has_config = false;
			for (i = 0; default_configsets[i].section_name != NULL; i++) {
				if (!strcmp(default_configsets[i].section_name, section_node->name)
				    && !strcmp(default_configsets[i].name, element_node->name)) {
					has_config = true;
					break;
				}
			}
			if (!has_config)
				printf("%s.%s=%s\n", section_node->name,
				       element_node->name, element_node->value);
		}
	}

	return 0;
}

int cmd_config(int argc, const char **argv, const char *prefix __maybe_unused)
{
	int ret = 0;

	argc = parse_options(argc, argv, config_options, config_usage,
			     PARSE_OPT_STOP_AT_NON_OPTION);

	if (use_system_config && use_user_config) {
		pr_err("Error: only one config file at a time\n");
		goto out_err;
	}
	config_file_name = mkpath("%s/.perfconfig", getenv("HOME"));
	if (use_system_config || (!use_user_config &&
				  access(config_file_name, R_OK) == -1))
		config_file_name = perf_etc_perfconfig();

	INIT_LIST_HEAD(&sections);
	perf_config_from_file(collect_current_config, config_file_name, NULL);

	switch (actions) {
	case ACTION_LIST_ALL:
		if (argc == 0) {
			ret = show_all_config();
			break;
		}
	case ACTION_LIST:
	default:
		if (argc) {
			pr_err("Error: unknown argument\n");
			goto out_err;
		} else
			ret = perf_config_from_file(show_config, config_file_name, NULL);
	}

	return ret;

out_err:
	usage_with_options(config_usage, config_options);
	return -1;
}
