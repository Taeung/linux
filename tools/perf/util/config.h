#ifndef __PERF_CONFIG_H
#define __PERF_CONFIG_H

#include <stdbool.h>
#include <linux/list.h>

enum perf_config_type {
	CONFIG_TYPE_BOOL,
	CONFIG_TYPE_INT,
	CONFIG_TYPE_LONG,
	CONFIG_TYPE_U64,
	CONFIG_TYPE_FLOAT,
	CONFIG_TYPE_DOUBLE,
	CONFIG_TYPE_STRING
};

/**
 * struct perf_config_item - element of perf's configs
 *
 * @is_custom: unknown or new config other than default config
 */
struct perf_config_item {
	const char *name;
	char *value;
	union {
		bool b;
		int i;
		u32 l;
		u64 ll;
		float f;
		double d;
		const char *s;
	} default_value;
	enum perf_config_type type;
	bool is_custom;
	struct list_head list;
};

struct perf_config_section {
	const char *name;
	struct list_head config_items;
	struct list_head list;
};

struct perf_config_set {
	struct list_head sections;
};

enum perf_config_secion_idx {
	CONFIG_COLORS,
	CONFIG_TUI,
	CONFIG_BUILDID,
	CONFIG_ANNOTATE,
	CONFIG_GTK,
	CONFIG_PAGER,
	CONFIG_HELP,
	CONFIG_HIST,
	CONFIG_UI,
	CONFIG_CALL_GRAPH,
	CONFIG_REPORT,
	CONFIG_TOP,
	CONFIG_MAN,
	CONFIG_KMEM,
	CONFIG_END
};

#define CONF_VAR(_name, _field, _val, _type)				\
	{ .name = _name, .default_value._field = _val, .type = _type }

#define CONF_BOOL_VAR(_name, _val)			\
	CONF_VAR(_name, b, _val, CONFIG_TYPE_BOOL)
#define CONF_INT_VAR(_name, _val)			\
	CONF_VAR(_name, i, _val, CONFIG_TYPE_INT)
#define CONF_LONG_VAR(_name, _val)			\
	CONF_VAR(_name, l, _val, CONFIG_TYPE_LONG)
#define CONF_U64_VAR(_name, _val)			\
	CONF_VAR(_name, ll, _val, CONFIG_TYPE_U64)
#define CONF_FLOAT_VAR(_name, _val)			\
	CONF_VAR(_name, f, _val, CONFIG_TYPE_FLOAT)
#define CONF_DOUBLE_VAR(_name, _val)			\
	CONF_VAR(_name, d, _val, CONFIG_TYPE_DOUBLE)
#define CONF_STR_VAR(_name, _val)			\
	CONF_VAR(_name, s, _val, CONFIG_TYPE_STRING)
#define CONF_END()					\
	{ .name = NULL }

struct perf_config_set *perf_config_set__new(void);
void perf_config_set__delete(struct perf_config_set *perf_configs);

#endif /* __PERF_CONFIG_H */
