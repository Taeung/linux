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
	case ACTION_LIST:
		if (argc) {
			pr_err("Error: takes no arguments\n");
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
