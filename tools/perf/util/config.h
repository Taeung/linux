#ifndef __PERF_CONFIG_H
#define __PERF_CONFIG_H

#include <stdbool.h>
#include <linux/list.h>

struct perf_config_item {
	char *name;
	char *value;
	struct list_head node;
};

struct perf_config_section {
	char *name;
	struct list_head items;
	struct list_head node;
};

struct perf_config_set {
	struct list_head sections;
};

extern const char *config_exclusive_filename;

typedef int (*config_fn_t)(const char *, const char *, void *);
int perf_default_config(const char *, const char *, void *);
int perf_config(config_fn_t fn, void *);
int perf_config_int(const char *, const char *);
u64 perf_config_u64(const char *, const char *);
int perf_config_bool(const char *, const char *);
int config_error_nonbool(const char *);
const char *perf_etc_perfconfig(void);

struct perf_config_set *perf_config_set__new(void);
void perf_config_set__delete(struct perf_config_set *set);
void perf_config__init(void);
void perf_config__exit(void);
void perf_config__refresh(void);

/**
 * perf_config_sections__for_each - iterate thru all the sections
 * @list: list_head instance to iterate
 * @section: struct perf_config_section iterator
 */
#define perf_config_sections__for_each_entry(list, section)	\
        list_for_each_entry(section, list, node)

/**
 * perf_config_items__for_each - iterate thru all the items
 * @list: list_head instance to iterate
 * @item: struct perf_config_item iterator
 */
#define perf_config_items__for_each_entry(list, item)	\
        list_for_each_entry(item, list, node)

/**
 * perf_config_set__for_each - iterate thru all the config section-item pairs
 * @set: evlist instance to iterate
 * @section: struct perf_config_section iterator
 * @item: struct perf_config_item iterator
 */
#define perf_config_set__for_each_entry(set, section, item)			\
	perf_config_sections__for_each_entry(&set->sections, section)		\
	perf_config_items__for_each_entry(&section->items, item)

enum perf_config_type {
	CONFIG_TYPE_BOOL,
	CONFIG_TYPE_INT,
	CONFIG_TYPE_LONG,
	CONFIG_TYPE_U64,
	CONFIG_TYPE_FLOAT,
	CONFIG_TYPE_DOUBLE,
	CONFIG_TYPE_STRING
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

struct default_config_section {
	const char *name;
	const struct default_config_item *items;
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

extern const struct default_config_section default_sections[];
extern const struct default_config_item colors_config_items[];

#endif /* __PERF_CONFIG_H */
