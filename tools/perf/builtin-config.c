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
} params;

static const char * const config_usage[] = {
	"perf config [options]",
	NULL
};
static const struct option config_options[] = {
	OPT_GROUP("Action"),
	OPT_BOOLEAN('l', "list", &params.list_action, "show current config variables"),
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

	if (params.list_action && argc == 0)
		ret = perf_config(show_config, NULL);
	else {
		pr_warning("Error: Unknown argument.\n");
		usage_with_options(config_usage, config_options);
	}

	return ret;
}
