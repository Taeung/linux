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
#include "util/config.h"

static bool use_system_config, use_user_config;

static const char * const config_usage[] = {
	"perf config [<file-option>] [options]",
	NULL
};

enum actions {
	ACTION_LIST = 1,
	ACTION_LIST_ALL
} actions;

static struct option config_options[] = {
	OPT_SET_UINT('l', "list", &actions,
		     "show current config variables", ACTION_LIST),
	OPT_SET_UINT('a', "list-all", &actions,
		     "show current and all possible config"
		     " variables with default values", ACTION_LIST_ALL),
	OPT_BOOLEAN(0, "system", &use_system_config, "use system config file"),
	OPT_BOOLEAN(0, "user", &use_user_config, "use user config file"),
	OPT_END()
};

#define DEFAULT_VAL(_field) config->default_value._field

static char *get_default_value(struct perf_config_item *config)
{
	int ret = 0;
	char *value;

	if (config->is_custom)
		return NULL;

	if (config->type == CONFIG_TYPE_BOOL)
		ret = asprintf(&value, "%s",
			       DEFAULT_VAL(b) ? "true" : "false");
	else if (config->type == CONFIG_TYPE_INT)
		ret = asprintf(&value, "%d", DEFAULT_VAL(i));
	else if (config->type == CONFIG_TYPE_LONG)
		ret = asprintf(&value, "%u", DEFAULT_VAL(l));
	else if (config->type == CONFIG_TYPE_U64)
		ret = asprintf(&value, "%"PRId64, DEFAULT_VAL(ll));
	else if (config->type == CONFIG_TYPE_FLOAT)
		ret = asprintf(&value, "%f", DEFAULT_VAL(f));
	else if (config->type == CONFIG_TYPE_DOUBLE)
		ret = asprintf(&value, "%f", DEFAULT_VAL(d));
	else
		ret = asprintf(&value, "%s", DEFAULT_VAL(s));

	if (ret < 0)
		return NULL;

	return value;
}

static int show_all_config(struct perf_config_set *perf_configs)
{
	char *value, *default_value;
	struct perf_config_item *config;
	struct list_head *config_list = &perf_configs->config_list;

	list_for_each_entry(config, config_list, list) {
		value = config->value;
		if (!value)
			value = default_value = get_default_value(config);

		if (value)
			printf("%s.%s=%s\n", config->section,
			       config->name, value);

		free(default_value);
		default_value = NULL;
	}

	return 0;
}

static int show_config(struct perf_config_set *perf_configs)
{
	bool has_value = false;
	struct perf_config_item *config;
	struct list_head *config_list = &perf_configs->config_list;

	list_for_each_entry(config, config_list, list) {
		char *value = config->value;

		if (value) {
			printf("%s.%s=%s\n", config->section,
			       config->name, value);
			has_value = true;
		}
	}

	if (!has_value)
		return -1;

	return 0;
}

int cmd_config(int argc, const char **argv, const char *prefix __maybe_unused)
{
	int ret = 0;
	struct perf_config_set *perf_configs;
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

	if (use_system_config)
		config_exclusive_filename = perf_etc_perfconfig();
	else if (use_user_config)
		config_exclusive_filename = user_config;

	perf_configs = perf_config_set__new();
	if (!perf_configs) {
		ret = -1;
		goto out_err;
	}

	switch (actions) {
	case ACTION_LIST_ALL:
		if (argc)
			parse_options_usage(config_usage, config_options, "a", 1);
		else
			ret = show_all_config(perf_configs);
		break;
	case ACTION_LIST:
		if (argc) {
			pr_err("Error: takes no arguments\n");
			parse_options_usage(config_usage, config_options, "l", 1);
		} else {
			ret = show_config(perf_configs);
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

	perf_config_set__delete(perf_configs);
out_err:
	return ret;
}
