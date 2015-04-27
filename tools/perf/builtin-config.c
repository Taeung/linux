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

static struct {
	bool list_action;
	bool get_action;
	bool set_action;
	bool all_action;
} params;

struct list_head *sections;

static const char * const config_usage[] = {
	"perf config [options] [section.subkey[=value] ...]",
	NULL
};
static const struct option config_options[] = {
	OPT_GROUP("Action"),
	OPT_BOOLEAN('l', "list", &params.list_action, "show current config variables"),
	OPT_BOOLEAN('a', "all", &params.all_action,
		    "show current and all possible config variables with default values"),
	OPT_END()
};

static struct config_section *find_config_section(const char *section_name)
{
	struct config_section *section_node;
	list_for_each_entry(section_node, sections, list)
		if (!strcmp(section_node->name, section_name))
			return section_node;

	return NULL;
}

static struct config_element *find_config_element(const char *subkey,
						  struct config_section *section_node)
{
	struct config_element *element_node;

	list_for_each_entry(element_node, &section_node->element_head, list)
		if (!strcmp(element_node->subkey, subkey))
			return element_node;

	return NULL;
}

static struct config_section *init_config_section(const char *section_name)
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

static int add_config_element(struct list_head *head,
			      const char *subkey, const char *value)
{
	struct config_element *element_node;
	element_node = zalloc(sizeof(*element_node));
	element_node->subkey = strdup(subkey);
	if (!element_node->subkey) {
		pr_err("%s: strdup failed\n", __func__);
		goto out_free;
	}
	if (value) {
		element_node->value = strdup(value);
		if (!element_node->value) {
			pr_err("%s: strdup failed\n", __func__);
			goto out_free;
		}
	} else
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

static void find_config(struct config_section **section_node, struct config_element **element_node,
			const char *given_section_name, const char *given_subkey)
{
	*section_node = find_config_section(given_section_name);

	if (*section_node != NULL)
		*element_node = find_config_element(given_subkey, *section_node);
	else
		*element_node = NULL;
}

static int show_spec_config(const char *section_name, const char *subkey,
			    const char *value __maybe_unused)
{
	struct config_section *section_node = NULL;
	struct config_element *element_node = NULL;
	char key[BUFSIZ];

	find_config(&section_node, &element_node, section_name, subkey);

	if (section_node && element_node) {
		scnprintf(key, sizeof(key), "%s.%s", section_node->name, element_node->subkey);
		show_config(key, element_node->value, NULL);
	} else
		pr_err("Error: Failed to find the variable.\n");

	return 0;
}

static int set_config(const char *section_name, const char *subkey,
		      const char *value)
{
	struct config_section *section_node = NULL;
	struct config_element *element_node = NULL;

	find_config(&section_node, &element_node, section_name, subkey);

	/* if there isn't existent section, add a new section */
	if (!section_node) {
		section_node = init_config_section(section_name);
		if (!section_node)
			return -1;
		list_add_tail(&section_node->list, sections);
	}
	/* if nothing to replace, add a new element which contains key-value pair. */
	if (!element_node)
		return add_config_element(&section_node->element_head, subkey, value);
	else
		element_node->value = (char *)value;

	return 0;
}

static int set_spec_config(const char *section_name, const char *subkey,
			   const char *value)
{
	int ret = 0;
	ret += set_config(section_name, subkey, value);
	ret += perf_configset_write_in_full();

	return ret;
}

static void parse_key(const char *var, const char **section_name, const char **subkey)
{
	char *key = strdup(var);

	if (!key)
		die("%s: strdup failed\n", __func__);

	*section_name = strsep(&key, ".");
	*subkey = strsep(&key, ".");
}

static int collect_config(const char *var, const char *value,
			  void *cb __maybe_unused)
{
	struct config_section *section_node;
	const char *section_name, *subkey;

	parse_key(var, &section_name, &subkey);

	section_node = find_config_section(section_name);
	if (!section_node) {
		/* Add a new section */
		section_node = init_config_section(section_name);
		if (!section_node)
			return -1;
		list_add_tail(&section_node->list, sections);
	}

	return add_config_element(&section_node->element_head, subkey, value);
}

static int merge_config(const char *var, const char *value,
			void *cb __maybe_unused)
{
	const char *section_name, *subkey;
	parse_key(var, &section_name, &subkey);
	return set_config(section_name, subkey, value);
}

static int show_all_config(void)
{
	int ret = 0;
	struct config_section *section_node;
	struct config_element *element_node;
	char *pwd, *all_config;

	pwd = getenv("PWD");
	all_config = strdup(mkpath("%s/util/PERFCONFIG-DEFAULT", pwd));

	if (!all_config) {
		pr_err("%s: strdup failed\n", __func__);
		return -1;
	}

	sections = zalloc(sizeof(*sections));
	if (!sections)
		return -1;
	INIT_LIST_HEAD(sections);

	ret += perf_config_from_file(collect_config, all_config, NULL);
	ret += perf_config(merge_config, NULL);

	list_for_each_entry(section_node, sections, list) {
		list_for_each_entry(element_node, &section_node->element_head, list) {
			if (element_node->value)
				printf("%s.%s = %s\n", section_node->name,
				       element_node->subkey, element_node->value);
			else
				printf("%s.%s =\n", section_node->name, element_node->subkey);
		}
	}
	return ret;
}

static int perf_configset_with_option(configset_fn_t fn, const char *var)
{
	int i, num_dot = 0, num_equals = 0;
	static char *given_section_name;
	static char *given_subkey;
	static char *given_value;
	const char *last_dot;
	char *key = strdup(var);

	if (!key) {
		pr_err("%s: strdup failed\n", __func__);
		return -1;
	}
	last_dot = strrchr(key, '.');
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
	for (i = 0; key[i]; i++) {
		if (i == 0 && !isalpha(key[i++]))
			goto out_err;

		switch (key[i]) {
		case '.':
			num_dot += 1;
			if (!isalpha(key[++i]))
				goto out_err;
			break;
		case '=':
			num_equals += 1;
			break;
		case '-':
			break;
		case '_':
			break;
		default:
			if (!isalpha(key[i]) && !isalnum(key[i]))
				goto out_err;
		}
	}

	if (num_equals > 1 || num_dot > 1)
		goto out_err;

	given_value = strchr(key, '=');
	if (given_value == NULL || given_value == key)
		given_value = NULL;
	else {
		if (!given_value[1]) {
			pr_err("The config variable does not contain a value: %s\n", key);
			return -1;
		} else
			given_value++;
	}
	given_section_name = strsep(&key, ".");
	given_subkey = strsep(&key, ".");

	if (given_value)
		given_subkey = strsep(&given_subkey, "=");

	if (!sections) {
		sections = zalloc(sizeof(*sections));
		if (!sections)
			return -1;

		INIT_LIST_HEAD(sections);
		perf_config(collect_config, NULL);
	}

	return fn(given_section_name, given_subkey, given_value);

out_err:
	pr_err("invalid key: %s\n", var);
	return -1;
}

int cmd_config(int argc, const char **argv, const char *prefix __maybe_unused)
{
	int i, ret = 0;
	int origin_argc = argc - 1;
	char *value;
	bool is_option;

	argc = parse_options(argc, argv, config_options, config_usage,
			     PARSE_OPT_STOP_AT_NON_OPTION);
	if (origin_argc > argc)
		is_option = true;
	else
		is_option = false;

	if (!is_option && argc >= 0) {
		switch (argc) {
		case 0:
			params.all_action = true;
			break;
		default:
			for (i = 0; argv[i]; i++) {
				value = strrchr(argv[i], '=');
				if (value == NULL || value == argv[i])
					ret = perf_configset_with_option(show_spec_config, argv[i]);
				else
					ret = perf_configset_with_option(set_spec_config, argv[i]);
				if (ret < 0)
					break;
			}
			goto out;
		}
	}

	if (params.list_action && argc == 0)
		ret = perf_config(show_config, NULL);
	else if (params.all_action && argc == 0)
		ret = show_all_config();
	else {
		pr_warning("Error: Unknown argument.\n");
		usage_with_options(config_usage, config_options);
	}

out:
	return ret;
}
