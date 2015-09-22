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

static const char * const config_usage[] = {
	"perf config [<file-option>] [options] [section.name[=value] ...]",
	NULL
};

enum actions {
	ACTION_LIST = 1,
	ACTION_LIST_ALL
} actions;

static struct option config_options[] = {
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
		.value = "true",
		.type = TYPE_BOOL,
	},
	{
		.section_name = TUI,
		.name = "annotate",
		.value = "true",
		.type = TYPE_BOOL,
	},
	{
		.section_name = TUI,
		.name = "top",
		.value = "true",
		.type = TYPE_BOOL,
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
		.value = "false",
		.type = TYPE_BOOL,
	},
	{
		.section_name = GTK,
		.name = "report",
		.value = "false",
		.type = TYPE_BOOL,
	},
	{
		.section_name = GTK,
		.name = "top",
		.value = "false",
		.type = TYPE_BOOL,
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
		.value = "true",
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

static int show_config(struct list_head *sections)
{
	struct config_section *section;
	struct config_element *element;

	list_for_each_entry(section, sections, list) {
		list_for_each_entry(element, &section->element_head, list) {
			printf("%s.%s=%s\n", section->name,
				       element->name, element->value);
		}
	}

	return 0;
}

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

static int show_all_config(struct list_head *sections)
{
	int i;
	bool has_config;
	struct config_section *section;
	struct config_element *element;

	for (i = 0; default_configsets[i].section_name != NULL; i++) {
		find_config(sections, &section, &element,
			    default_configsets[i].section_name, default_configsets[i].name);

		if (!element)
			printf("%s.%s=%s\n", default_configsets[i].section_name,
			       default_configsets[i].name, default_configsets[i].value);
		else
			printf("%s.%s=%s\n", section->name,
			       element->name, element->value);
	}

	/* Print config variables the default configsets haven't */
	list_for_each_entry(section, sections, list) {
		list_for_each_entry(element, &section->element_head, list) {
			has_config = false;
			for (i = 0; default_configsets[i].section_name != NULL; i++) {
				if (!strcmp(default_configsets[i].section_name, section->name)
				    && !strcmp(default_configsets[i].name, element->name)) {
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

static int show_spec_config(struct list_head *sections,
			    const char *config_file_name __maybe_unused,
			    const char *section_name, const char *name,
			    char *value __maybe_unused)
{
	int i;
	struct config_section *section = NULL;
	struct config_element *element = NULL;

	find_config(sections, &section, &element, section_name, name);

	if (section && element) {
		printf("%s.%s=%s\n", section->name,
		       element->name, element->value);
		return 0;
	}

	for (i = 0; default_configsets[i].section_name != NULL; i++) {
		struct default_configset *config = &default_configsets[i];

		if (!strcmp(config->section_name, section_name) &&
		    !strcmp(config->name, name)) {
			printf("%s.%s=%s (default)\n", config->section_name,
			       config->name, config->value);
			return 0;
		}
	}

	return -1;
}

static int set_config(struct list_head *sections, const char *config_file_name,
		      const char *section_name, const char *name, char *value)
{
	struct config_section *section = NULL;
	struct config_element *element = NULL;

	find_config(sections, &section, &element, section_name, name);
	if (value != NULL) {
		value = strdup(value);
		if (!value) {
			pr_err("%s: strdup failed\n", __func__);
			return -1;
		}

		/* if there isn't existent section, add a new section */
		if (!section) {
			section = init_section(section_name);
			if (!section)
				return -1;
			list_add_tail(&section->list, sections);
		}
		/* if nothing to replace, add a new element which contains key-value pair. */
		if (!element) {
			add_element(&section->element_head, name, value);
		} else {
			free(element->value);
			element->value = value;
		}
	}

	return perf_configset_write_in_full(sections, config_file_name);
}

static int collect_current_config(const char *var, const char *value,
				  void *spec_sections __maybe_unused)
{
	struct config_section *section = NULL;
	struct config_element *element = NULL;
	struct list_head *sections = (struct list_head *)spec_sections;
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

	find_config(sections, &section, &element, section_name, name);

	if (!section) {
		section = init_section(section_name);
		if (!section)
			return -1;
		list_add_tail(&section->list, sections);
	}

	value = strdup(value);
	if (!value) {
		pr_err("%s: strdup failed\n", __func__);
		return -1;
	}

	if (!element)
		return add_element(&section->element_head, name, value);
	else {
		free(element->value);
		element->value = (char *)value;
	}

	return 0;
}

static int perf_configset_with_option(configset_fn_t fn,
				      struct list_head *sections,
				      const char *config_file_name,
				      const char *var, char *value)
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
		if (name[0] == '\0') {
			pr_err("invalid key: %s\n", var);
			return -1;
		}
	}
	free(key);

	return fn(sections, config_file_name, section_name, name, value);
}

int cmd_config(int argc, const char **argv, const char *prefix __maybe_unused)
{
	int i, ret = 0;
	struct list_head *sections;
	struct list_head all_sections, user_sections, system_sections;
	const char *config_file_name;
	const char *system_config = perf_etc_perfconfig();
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

	INIT_LIST_HEAD(&user_sections);
	INIT_LIST_HEAD(&system_sections);
	perf_config_from_file(collect_current_config, user_config,  &user_sections);
	perf_config_from_file(collect_current_config, system_config,  &system_sections);

	if (use_system_config) {
		sections = &system_sections;
		config_file_name = system_config;
	} else if (use_user_config) {
		sections = &user_sections;
		config_file_name = user_config;
	} else {
		INIT_LIST_HEAD(&all_sections);
		sections = &all_sections;
		config_file_name = user_config;
		perf_config_from_file(collect_current_config, system_config, &all_sections);
		perf_config_from_file(collect_current_config, user_config, &all_sections);
	}

	switch (actions) {
	case ACTION_LIST_ALL:
		if (argc == 0) {
			ret = show_all_config(sections);
			break;
		}
	case ACTION_LIST:
		if (argc) {
			pr_err("Error: takes no arguments\n");
			if (actions == ACTION_LIST_ALL)
				parse_options_usage(config_usage, config_options, "a", 1);
			else
				parse_options_usage(config_usage, config_options, "l", 1);
			return -1;
		}
	default:

		if (argc) {
			for (i = 0; argv[i]; i++) {
				char *value = strchr(argv[i], '=');

				if (value == NULL)
					ret = perf_configset_with_option(show_spec_config,
									 sections, NULL,
									 argv[i], value);
				else {
					if (!use_system_config && !use_user_config)
						ret = perf_configset_with_option(set_config,
										 &user_sections,
										 user_config,
										 argv[i], value);
					else
						ret = perf_configset_with_option(set_config,
										 sections,
										 config_file_name,
										 argv[i], value);
				}
			}
		} else
			ret = show_config(sections);
	}

	return ret;
}
