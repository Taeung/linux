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
static bool use_system_config, use_global_config;
static const char *config_file_name;

static const char * const config_usage[] = {
	"perf config [options]",
	NULL
};

#define ACTION_LIST (1<<0)

static const struct option config_options[] = {
	OPT_GROUP("Config file location"),
	OPT_BOOLEAN(0, "system", &use_system_config, "use system config file"),
	OPT_BOOLEAN(0, "global", &use_global_config, "use global config file"),
	OPT_GROUP("Action"),
	OPT_BIT('l', "list", &actions,
		"show current config variables", ACTION_LIST),
	OPT_END()
};

static int show_config(const char *key, const char *value,
		       void *cb __maybe_unused)
{
	if (value)
		printf("%s=%s\n", key, value);
	else
		printf("%s\n", key);

	return 0;
}

int cmd_config(int argc, const char **argv, const char *prefix __maybe_unused)
{
	int ret = 0;
	int origin_argc = argc - 1;
	bool has_option;

	argc = parse_options(argc, argv, config_options, config_usage,
			     PARSE_OPT_STOP_AT_NON_OPTION);
	if (origin_argc > argc)
		has_option = true;
	else
		has_option = false;

	if (use_system_config && use_global_config) {
		pr_err("Error: only one config file at a time\n");
		usage_with_options(config_usage, config_options);
		return -1;
	} else if (use_global_config || (!use_system_config && !use_global_config))
		config_file_name = mkpath("%s/.perfconfig", getenv("HOME"));
	else if (use_system_config)
		config_file_name = perf_etc_perfconfig();

	switch (actions) {
	case ACTION_LIST:
		if (argc == 0)
			ret = perf_config_from_file(show_config, config_file_name, NULL);
		else
			goto out_err;
		goto out;
	default:
		if ((!has_option || use_global_config || use_system_config)
		    && argc == 0) {
			ret = perf_config_from_file(show_config, config_file_name, NULL);
			goto out;
		} else
			goto out_err;
	}

out_err:
	pr_err("Error: Unknown argument\n");
	usage_with_options(config_usage, config_options);
out:
	return ret;
}
