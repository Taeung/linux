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
	ACTION_LIST = 1
} actions;

static const struct option config_options[] = {
	OPT_GROUP("Config file location"),
	OPT_BOOLEAN(0, "system", &use_system_config, "use system config file"),
	OPT_BOOLEAN(0, "user", &use_user_config, "use user config file"),
	OPT_GROUP("Action"),
	OPT_SET_UINT('l', "list", &actions,
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

	switch (actions) {
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
