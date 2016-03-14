#ifndef __PERF_CONFIG_H
#define __PERF_CONFIG_H

#include <stdbool.h>
#include <linux/list.h>

struct perf_config_item {
	char *section;
	char *name;
	char *value;
	struct list_head list;
};

struct perf_config_set {
	struct list_head config_list;
};

struct perf_config_set *perf_config_set__new(void);
void perf_config_set__delete(struct perf_config_set *perf_configs);

#endif /* __PERF_CONFIG_H */
