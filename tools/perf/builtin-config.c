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
	"perf config [<file-option>] [options]",
	NULL
};

enum actions {
	ACTION_LIST = 1
} actions;

static struct option config_options[] = {
	OPT_GROUP("Config file location"),
	OPT_BOOLEAN(0, "system", &use_system_config, "use system config file"),
	OPT_BOOLEAN(0, "user", &use_user_config, "use user config file"),
	OPT_GROUP("Action"),
	OPT_SET_UINT('l', "list", &actions,
		     "show current config variables", ACTION_LIST),
	OPT_END()
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

int cmd_config(int argc, const char **argv, const char *prefix __maybe_unused)
{
	int ret = 0;
	struct list_head sections;
	const char *system_config = perf_etc_perfconfig();
	char *user_config = mkpath("%s/.perfconfig", getenv("HOME"));

	argc = parse_options(argc, argv, config_options, config_usage,
			     PARSE_OPT_STOP_AT_NON_OPTION);

	if (use_system_config && use_user_config) {
		pr_err("Error: only one config file at a time\n");
		parse_options_usage(config_usage, config_options, "user", 0);
		parse_options_usage(NULL, config_options, "system", 0);
		return -1;
	}

	INIT_LIST_HEAD(&sections);

	if (use_system_config || (!use_system_config && !use_user_config))
		perf_config_from_file(collect_current_config, system_config, &sections);

	if (use_user_config || (!use_system_config && !use_user_config))
		perf_config_from_file(collect_current_config, user_config, &sections);

	switch (actions) {
	case ACTION_LIST:
	default:
		if (argc) {
			pr_err("Error: takes no arguments\n");
			parse_options_usage(config_usage, config_options, "l", 1);
			return -1;
		} else
			ret = show_config(&sections);
	}

	return ret;
}
