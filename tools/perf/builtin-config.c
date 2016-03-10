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
	ACTION_LIST = 1
} actions;

static struct option config_options[] = {
	OPT_SET_UINT('l', "list", &actions,
		     "show current config variables", ACTION_LIST),
	OPT_BOOLEAN(0, "system", &use_system_config, "use system config file"),
	OPT_BOOLEAN(0, "user", &use_user_config, "use user config file"),
	OPT_END()
};

static int show_config(struct perf_config_set *perf_configs)
{
	struct perf_config_item *config;
	enum perf_config_kind pos = perf_configs->pos;
	bool *file_usable = perf_configs->file_usable;
	struct list_head *config_list = &perf_configs->config_list;

	if (pos == BOTH) {
		if (!file_usable[USER] && !file_usable[SYSTEM])
			return -1;
	} else if (!file_usable[pos])
		return -1;

	list_for_each_entry(config, config_list, list) {
		char *value = config->value[pos];

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
	struct perf_config_set *perf_configs;
	int ret = 0;

	argc = parse_options(argc, argv, config_options, config_usage,
			     PARSE_OPT_STOP_AT_NON_OPTION);

	if (use_system_config && use_user_config) {
		pr_err("Error: only one config file at a time\n");
		parse_options_usage(config_usage, config_options, "user", 0);
		parse_options_usage(NULL, config_options, "system", 0);
		return -1;
	}

	perf_configs = perf_config_set__new();
	if (!perf_configs)
		return -1;

	if (use_user_config)
		perf_configs->pos = USER;
	else if (use_system_config)
		perf_configs->pos = SYSTEM;
	else
		perf_configs->pos = BOTH;


	switch (actions) {
	case ACTION_LIST:
		if (argc) {
			pr_err("Error: takes no arguments\n");
			parse_options_usage(config_usage, config_options, "l", 1);
		} else {
			ret = show_config(perf_configs);
			if (ret < 0)
				pr_err("Nothing configured, "
				       "please check your %s \n",
				       perf_configs->file_path[perf_configs->pos]);
		}
		break;
	default:
		usage_with_options(config_usage, config_options);
	}

	perf_config_set__delete(perf_configs);
	return ret;
}
