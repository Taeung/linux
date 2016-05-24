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

struct default_config_item {
	const char *name;
	union {
		bool b;
		int i;
		u32 l;
		u64 ll;
		float f;
		double d;
		const char *s;
	} value;
	enum perf_config_type type;
};

struct perf_config_item {
	char *name;
	char *value;
	struct list_head node;
};

struct perf_config_section {
	const char *name;
	struct list_head items;
	struct list_head node;
};

struct perf_config_set {
	struct list_head sections;
};

enum config_section_idx {
	CONFIG_COLORS,
};

enum colors_config_items_idx {
	CONFIG_COLORS_TOP,
	CONFIG_COLORS_MEDIUM,
	CONFIG_COLORS_NORMAL,
	CONFIG_COLORS_SELECTED,
	CONFIG_COLORS_JUMP_ARROWS,
	CONFIG_COLORS_ADDR,
	CONFIG_COLORS_ROOT,
};

#define CONF_VAR(_name, _field, _val, _type)			\
	{ .name = _name, .value._field = _val, .type = _type }

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

extern const struct default_config_item colors_config_items[];

struct perf_config_set *perf_config_set__new(void);
void perf_config_set__delete(struct perf_config_set *set);

#endif /* __PERF_CONFIG_H */
