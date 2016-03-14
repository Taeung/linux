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

struct perf_config_item {
	const char *section;
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
	struct list_head list;
};

struct perf_config_set {
	struct list_head config_list;
};

enum perf_config_idx {
	CONFIG_COLORS_TOP,
	CONFIG_COLORS_MEDIUM,
	CONFIG_COLORS_NORMAL,
	CONFIG_COLORS_SELECTED,
	CONFIG_COLORS_JUMP_ARROWS,
	CONFIG_COLORS_ADDR,
	CONFIG_COLORS_ROOT,
	CONFIG_TUI_REPORT,
	CONFIG_TUI_ANNOTATE,
	CONFIG_TUI_TOP,
	CONFIG_BUILDID_DIR,
	CONFIG_ANNOTATE_HIDE_SRC_CODE,
	CONFIG_ANNOTATE_USE_OFFSET,
	CONFIG_ANNOTATE_JUMP_ARROWS,
	CONFIG_ANNOTATE_SHOW_NR_JUMPS,
	CONFIG_ANNOTATE_SHOW_LINENR,
	CONFIG_ANNOTATE_SHOW_TOTAL_PERIOD,
	CONFIG_GTK_ANNOTATE,
	CONFIG_GTK_REPORT,
	CONFIG_GTK_TOP,
	CONFIG_PAGER_CMD,
	CONFIG_PAGER_REPORT,
	CONFIG_PAGER_ANNOTATE,
	CONFIG_PAGER_TOP,
	CONFIG_PAGER_DIFF,
	CONFIG_HELP_FORMAT,
	CONFIG_HELP_AUTOCORRECT,
	CONFIG_HIST_PERCENTAGE,
	CONFIG_UI_SHOW_HEADERS,
	CONFIG_CALL_GRAPH_RECORD_MODE,
	CONFIG_CALL_GRAPH_DUMP_SIZE,
	CONFIG_CALL_GRAPH_PRINT_TYPE,
	CONFIG_CALL_GRAPH_ORDER,
	CONFIG_CALL_GRAPH_SORT_KEY,
	CONFIG_CALL_GRAPH_THRESHOLD,
	CONFIG_CALL_GRAPH_PRINT_LIMIT,
	CONFIG_REPORT_GROUP,
	CONFIG_REPORT_CHILDREN,
	CONFIG_REPORT_PERCENT_LIMIT,
	CONFIG_REPORT_QUEUE_SIZE,
	CONFIG_TOP_CHILDREN,
	CONFIG_MAN_VIEWER,
	CONFIG_KMEM_DEFAULT,
	CONFIG_END
};

#define CONF_VAR(_sec, _name, _field, _val, _type)			\
	{ .section = _sec, .name = _name, .default_value._field = _val, .type = _type }

#define CONF_BOOL_VAR(_idx, _sec, _name, _val)				\
	[CONFIG_##_idx] = CONF_VAR(_sec, _name, b, _val, CONFIG_TYPE_BOOL)
#define CONF_INT_VAR(_idx, _sec, _name, _val)				\
	[CONFIG_##_idx] = CONF_VAR(_sec, _name, i, _val, CONFIG_TYPE_INT)
#define CONF_LONG_VAR(_idx, _sec, _name, _val)				\
	[CONFIG_##_idx] = CONF_VAR(_sec, _name, l, _val, CONFIG_TYPE_LONG)
#define CONF_U64_VAR(_idx, _sec, _name, _val)				\
	[CONFIG_##_idx] = CONF_VAR(_sec, _name, ll, _val, CONFIG_TYPE_U64)
#define CONF_FLOAT_VAR(_idx, _sec, _name, _val)				\
	[CONFIG_##_idx] = CONF_VAR(_sec, _name, f, _val, CONFIG_TYPE_FLOAT)
#define CONF_DOUBLE_VAR(_idx, _sec, _name, _val)			\
	[CONFIG_##_idx] = CONF_VAR(_sec, _name, d, _val, CONFIG_TYPE_DOUBLE)
#define CONF_STR_VAR(_idx, _sec, _name, _val)				\
	[CONFIG_##_idx] = CONF_VAR(_sec, _name, s, _val, CONFIG_TYPE_STRING)

struct perf_config_set *perf_config_set__new(void);
void perf_config_set__delete(struct perf_config_set *perf_configs);

#endif /* __PERF_CONFIG_H */
