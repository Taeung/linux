#ifndef __PERF_CONFIG_H
#define __PERF_CONFIG_H

#include <stdbool.h>
#include <linux/list.h>

/**
 * enum perf_config_kind - three kinds of config information
 *
 * @USER: user wide ~/.perfconfig
 * @SYSTEM: system wide $(sysconfdir)/perfconfig
 * @BOTH: both the config files
 * but user config file has order of priority.
 */
#define CONFIG_KIND 3
enum perf_config_kind {
	BOTH,
	USER,
	SYSTEM,
};

/**
 * struct perf_config_item - element of perf's configs
 *
 * @value: array that has values for each kind (USER/SYSTEM/BOTH)
 * @is_custom: if this is custom config other than default config
 */
struct perf_config_item {
	char *section;
	char *name;
	char *value[CONFIG_KIND];
	struct list_head list;
};

/**
 * struct perf_config_set - perf's config set from the config files
 *
 * @file_path: array that has paths of config files
 * @pos: current major config file
 * @config_list: perf_config_item list head
 */
struct perf_config_set {
	enum perf_config_kind pos;
	char *file_path[CONFIG_KIND];
	bool file_usable[CONFIG_KIND];
	struct list_head config_list;
};

struct perf_config_set *perf_config_set__new(void);
void perf_config_set__delete(struct perf_config_set *perf_configs);

#endif /* __PERF_CONFIG_H */
