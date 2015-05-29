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

static int actions;

static const char * const config_usage[] = {
	"perf config [options] [section.name[=value] ...]",
	NULL
};

#define ACTION_LIST (1<<0)

static const struct option config_options[] = {
	OPT_GROUP("Action"),
	OPT_BIT('l', "list", &actions, "show current config variables", ACTION_LIST),
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

static int show_spec_config(const char *section_name, const char *name,
			    char *value __maybe_unused)
{
	int i;
	struct config_section *section_node = NULL;
	struct config_element *element_node = NULL;
	char key[BUFSIZ];

	find_config(&section_node, &element_node, section_name, name);

	if (section_node && element_node) {
		scnprintf(key, sizeof(key), "%s.%s",
			  section_node->name, element_node->name);
		return show_config(key, element_node->value, NULL);
	}

	for (i = 0; default_configsets[i].section_name != NULL; i++) {
		if (!strcmp(default_configsets[i].section_name, section_name)
		    && !strcmp(default_configsets[i].name, name)) {
			printf("%s.%s=%s\n", default_configsets[i].section_name,
			       default_configsets[i].name, default_configsets[i].value);
			return 0;
		}
	}

	pr_err("Error: Failed to find the variable.\n");

	return 0;
}

static char *normalize_value(const char *section_name, const char *name, const char *value)
{
	int i;
	char key[BUFSIZ];
	char *normalized;

	scnprintf(key, sizeof(key), "%s.%s", section_name, name);
	for (i = 0; default_configsets[i].section_name != NULL; i++) {
		if (!strcmp(default_configsets[i].section_name, section_name)
		    && !strcmp(default_configsets[i].name, name)) {
			normalized = zalloc(BUFSIZ);
			if (!default_configsets[i].type)
				scnprintf(normalized, BUFSIZ, "%s", value);
			else if (!strcmp(default_configsets[i].type, TYPE_BOOL))
				scnprintf(normalized, BUFSIZ, "%s",
					  perf_config_bool(key, value) ? "true" : "false");
			else if (!strcmp(default_configsets[i].type, TYPE_ON_OFF))
				scnprintf(normalized, BUFSIZ, "%s",
					  perf_config_bool(key, value) ? "on" : "off");
			else if (!strcmp(default_configsets[i].type, TYPE_INT))
				scnprintf(normalized, BUFSIZ, "%d",
					  perf_config_int(key, value));
			else if (!strcmp(default_configsets[i].type, TYPE_LONG))
				scnprintf(normalized, BUFSIZ, "%"PRId64,
					  perf_config_u64(key, value));
			else if (!strcmp(default_configsets[i].type, TYPE_DIRNAME))
				scnprintf(normalized, BUFSIZ, "%s",
					  perf_config_dirname(key, value));
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

static int set_config(const char *section_name, const char *name, char *value)
{
	struct config_section *section_node = NULL;
	struct config_element *element_node = NULL;

	find_config(&section_node, &element_node, section_name, name);
	if (value != NULL) {
		value = normalize_value(section_name, name, value);

		/* if there isn't existent section, add a new section */
		if (!section_node) {
			section_node = init_section(section_name);
			if (!section_node)
				return -1;
			list_add_tail(&section_node->list, &sections);
		}
		/* if nothing to replace, add a new element which contains key-value pair. */
		if (!element_node) {
			add_element(&section_node->element_head, name, value);
		} else {
			if (!element_node->value)
				free(element_node->value);
			element_node->value = value;
		}
	}
	return perf_configset_write_in_full();
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

static int perf_configset_with_option(configset_fn_t fn, const char *var, char *value)
{
	char *section_name;
	char *name;
	const char *last_dot;
	char *key = strdup(var);

	if (!key) {
		pr_err("%s: strdup failed\n", __func__);
		return -1;
	}
	last_dot = strchr(key, '.');
	/*
	 * Since "key" actually contains the section name and the real
	 * key name separated by a dot, we have to know where the dot is.
	 */
	if (last_dot == NULL || last_dot == key) {
		pr_err("The config variable does not contain a section: %s\n", key);
		return -1;
	}
	if (!last_dot[1]) {
		pr_err("The config varible does not contain variable name: %s\n", key);
		return -1;
	}

	section_name = strsep(&key, ".");
	name = strsep(&key, ".");

	if (!value) {
		/* do nothing */
	} else if (!strcmp(value, "=")) {
		pr_err("The config variable does not contain a value: %s.%s\n",
		       section_name, name);
		return -1;
	} else {
		value++;
		name = strsep(&name, "=");
	}

	return fn(section_name, name, value);

	pr_err("invalid key: %s\n", var);
	return -1;
}

int cmd_config(int argc, const char **argv, const char *prefix __maybe_unused)
{
	int i, ret = 0;
	int origin_argc = argc - 1;
	char *value;
	bool has_option;

	argc = parse_options(argc, argv, config_options, config_usage,
			     PARSE_OPT_STOP_AT_NON_OPTION);
	if (origin_argc > argc)
		has_option = true;
	else
		has_option = false;

	INIT_LIST_HEAD(&sections);
	perf_config(collect_current_config, NULL);

	switch (actions) {
	case ACTION_LIST:
		if (argc == 0)
			ret = perf_config(show_config, NULL);
		else
			goto out_err;
		goto out;
	default:
		if (!has_option && argc == 0) {
			ret = perf_config(show_config, NULL);
			goto out;
		} else if (argc > 0) {
			for (i = 0; argv[i]; i++) {
				value = strchr(argv[i], '=');
				if (value == NULL)
					ret = perf_configset_with_option(show_spec_config,
									 argv[i], value);
				else
					ret = perf_configset_with_option(set_config,
									 argv[i], value);
				if (ret < 0)
					break;
			}
			goto out;
		} else
			goto out_err;
	}

out_err:
	pr_warning("Error: Unknown argument.\n");
	usage_with_options(config_usage, config_options);
out:
	return ret;
}
