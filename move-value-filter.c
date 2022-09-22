#include "move-transition.h"
#include <obs-module.h>
#include <float.h>
#include <stdio.h>
#include <time.h>
#include <util/dstr.h>
#include <obs-frontend-api.h>

#define TEXT_BUFFER_SIZE 256
#define VOLUME_SETTING "source_volume"
#define VOLUME_MIN 0.0
#define VOLUME_MAX 100.0
#define VOLUME_STEP 1.0

struct move_value_info {
	struct move_filter move_filter;

	obs_weak_source_t *filter;
	char *setting_filter_name;
	char *setting_name;

	long long int_to;
	long long int_value;
	long long int_from;
	long long int_min;
	long long int_max;

	int decimals;
	double double_to;
	double double_value;
	double double_from;
	double double_min;
	double double_max;

	struct vec4 color_to;
	struct vec4 color_value;
	struct vec4 color_from;
	struct vec4 color_min;
	struct vec4 color_max;

	char *text_from;
	size_t text_from_len;
	char *text_to;
	size_t text_to_len;
	size_t text_same;
	size_t text_step;
	size_t text_steps;

	obs_data_array_t *settings;

	long long move_value_type;
	long long value_type;
	long long format_type;
	char *format;
};

static void load_properties(obs_properties_t *props_from,
			    obs_data_array_t *array, obs_data_t *settings_to,
			    obs_data_t *settings_from)
{
	obs_property_t *prop_from = obs_properties_first(props_from);
	for (; prop_from != NULL; obs_property_next(&prop_from)) {
		const char *name = obs_property_name(prop_from);
		if (!obs_property_visible(prop_from))
			continue;

		obs_data_t *setting = NULL;
		const size_t count = obs_data_array_count(array);
		for (size_t i = 0; i < count; i++) {
			obs_data_t *item2 = obs_data_array_item(array, i);
			const char *setting_name2 =
				obs_data_get_string(item2, S_SETTING_NAME);
			if (strcmp(setting_name2, name) == 0) {
				setting = item2;
			}
		}

		const enum obs_property_type prop_type =
			obs_property_get_type(prop_from);
		if (prop_type == OBS_PROPERTY_GROUP) {
			load_properties(obs_property_group_content(prop_from),
					array, settings_to, settings_from);
		} else if (prop_type == OBS_PROPERTY_INT) {
			if (!setting) {
				setting = obs_data_create();
				obs_data_set_string(setting, S_SETTING_NAME,
						    name);
				obs_data_array_push_back(array, setting);
			}
			obs_data_set_int(setting, S_VALUE_TYPE, MOVE_VALUE_INT);
			if (obs_data_has_default_value(settings_from, name))
				obs_data_set_default_int(
					settings_to, name,
					obs_data_get_default_int(settings_from,
								 name));
			const long long to =
				obs_data_get_int(settings_to, name);
			obs_data_set_int(setting, S_SETTING_TO, to);
			const long long from =
				obs_data_get_int(settings_from, name);
			obs_data_set_int(setting, S_SETTING_FROM, from);
		} else if (prop_type == OBS_PROPERTY_FLOAT) {
			if (!setting) {
				setting = obs_data_create();
				obs_data_set_string(setting, S_SETTING_NAME,
						    name);
				obs_data_array_push_back(array, setting);
			}
			obs_data_set_int(setting, S_VALUE_TYPE,
					 MOVE_VALUE_FLOAT);
			if (obs_data_has_default_value(settings_from, name))
				obs_data_set_default_double(
					settings_to, name,
					obs_data_get_default_double(
						settings_from, name));
			const double to =
				obs_data_get_double(settings_to, name);
			obs_data_set_double(setting, S_SETTING_TO, to);
			const double from =
				obs_data_get_double(settings_from, name);
			obs_data_set_double(setting, S_SETTING_FROM, from);
		} else if (prop_type == OBS_PROPERTY_COLOR ||
			   prop_type == OBS_PROPERTY_COLOR_ALPHA) {
			if (!setting) {
				setting = obs_data_create();
				obs_data_set_string(setting, S_SETTING_NAME,
						    name);
				obs_data_array_push_back(array, setting);
			}
			obs_data_set_int(setting, S_VALUE_TYPE,
					 MOVE_VALUE_COLOR);
			if (obs_data_has_default_value(settings_from, name))
				obs_data_set_default_int(
					settings_to, name,
					obs_data_get_default_int(settings_from,
								 name));
			obs_data_set_int(setting, S_SETTING_TO,
					 obs_data_get_int(settings_to, name));
			const long long from =
				obs_data_get_int(settings_from, name);
			obs_data_set_int(setting, S_SETTING_FROM, from);
		}
	}
}

void move_values_load_properties(struct move_value_info *move_value,
				 obs_source_t *source, obs_data_t *settings)
{
	if (source && source != move_value->move_filter.source) {
		obs_properties_t *sps = obs_source_properties(source);
		size_t index = 0;
		while (index < obs_data_array_count(move_value->settings)) {
			obs_data_t *item = obs_data_array_item(
				move_value->settings, index);
			const char *setting_name =
				obs_data_get_string(item, S_SETTING_NAME);
			if (obs_properties_get(sps, setting_name) == NULL) {
				obs_data_array_erase(move_value->settings,
						     index);
			} else {
				index++;
			}
		}
		obs_data_t *data_from = obs_source_get_settings(source);
		load_properties(sps, move_value->settings, settings, data_from);
		obs_data_release(data_from);
		obs_properties_destroy(sps);
	} else {
		while (obs_data_array_count(move_value->settings)) {
			obs_data_array_erase(move_value->settings, 0);
		}
	}
}

long long rand_between(long long a, long long b)
{
	return b > a ? a + rand() % (b - a) : b + rand() % (a - b);
}

float rand_between_float(float a, float b)
{
	return b > a ? a + (b - a) * (float)rand() / (float)RAND_MAX
		     : b + (a - b) * (float)rand() / (float)RAND_MAX;
}
double rand_between_double(double a, double b)
{
	return b > a ? a + (b - a) * (double)rand() / (double)RAND_MAX
		     : b + (a - b) * (double)rand() / (double)RAND_MAX;
}

double parse_text(long long format_type, const char *format, const char *text)
{
	double value = 0.0;
	if (format_type == MOVE_VALUE_FORMAT_FLOAT) {

		sscanf(text, format, &value);
	} else if (format_type == MOVE_VALUE_FORMAT_TIME) {
		char *pos;
		unsigned int sec = 0;
		unsigned int min = 0;
		unsigned int hour = 0;
		if (((pos = strstr(format, "%X"))) ||
		    ((pos = strstr(format, "%H:%M:%S")))) {
			if ((size_t)(pos - format) < strlen(text))
				sscanf(text + (pos - format), "%u:%u:%u", &hour,
				       &min, &sec);
		} else if (((pos = strstr(format, "%R"))) ||
			   ((pos = strstr(format, "%H:%M")))) {
			if ((size_t)(pos - format) < strlen(text))
				sscanf(text + (pos - format), "%u:%u", &hour,
				       &min);
		} else if ((pos = strstr(format, "%M:%S"))) {
			if ((size_t)(pos - format) < strlen(text))
				sscanf(text + (pos - format), "%u:%u", &min,
				       &sec);
		} else {
			if ((pos = strstr(format, "%S"))) {
				sscanf(text + (pos - format), "%u", &sec);
			}
			if ((pos = strstr(format, "%M"))) {
				sscanf(text + (pos - format), "%u", &min);
			}
			if ((pos = strstr(format, "%H"))) {
				sscanf(text + (pos - format), "%u", &hour);
			}
		}
		value = hour * 3600 + min * 60 + sec;
	} else {
		value = strtod(text, NULL);
	}
	return value;
}

void move_value_start(struct move_value_info *move_value)
{
	if (!move_value->filter && move_value->setting_filter_name &&
	    strlen(move_value->setting_filter_name)) {
		obs_source_t *parent =
			obs_filter_get_parent(move_value->move_filter.source);
		if (parent) {
			obs_source_t *filter = obs_source_get_filter_by_name(
				parent, move_value->setting_filter_name);
			move_value->filter = obs_source_get_weak_source(filter);
			obs_source_release(filter);
		} else {
			return;
		}
	}
	if (!move_filter_start_internal(&move_value->move_filter))
		return;

	if (!move_value->setting_filter_name) {
		obs_source_update(move_value->move_filter.source, NULL);
	}
	if (move_value->move_filter.reverse)
		return;
	obs_source_t *source = NULL;
	if (move_value->setting_filter_name &&
	    strlen(move_value->setting_filter_name)) {
		source = obs_weak_source_get_source(move_value->filter);
		obs_source_release(source);
	} else {
		source = obs_filter_get_parent(move_value->move_filter.source);
	}

	obs_data_t *ss = obs_source_get_settings(source);
	if (move_value->settings) {
		obs_data_t *settings =
			obs_source_get_settings(move_value->move_filter.source);
		move_values_load_properties(move_value, source, settings);
		obs_data_release(settings);
	} else if (move_value->value_type == MOVE_VALUE_INT) {
		if (strcmp(move_value->setting_name, VOLUME_SETTING) == 0) {
			move_value->int_from =
				(long long)(obs_source_get_volume(source) *
					    100.0f);
		} else {
			move_value->int_from =
				obs_data_get_int(ss, move_value->setting_name);
		}

		if (move_value->move_value_type == MOVE_VALUE_TYPE_RANDOM) {
			move_value->int_to = rand_between(move_value->int_min,
							  move_value->int_max);
		} else if (move_value->move_value_type ==
			   MOVE_VALUE_TYPE_SETTING_ADD) {
			move_value->int_to =
				move_value->int_from + move_value->int_value;
		} else {
			move_value->int_to = move_value->int_value;
		}
	} else if (move_value->value_type == MOVE_VALUE_FLOAT) {
		if (strcmp(move_value->setting_name, VOLUME_SETTING) == 0) {
			move_value->double_from =
				obs_source_get_volume(source) * 100.0f;
		} else {
			move_value->double_from = obs_data_get_double(
				ss, move_value->setting_name);
		}
		if (move_value->move_value_type == MOVE_VALUE_TYPE_RANDOM) {
			move_value->double_to = rand_between_double(
				move_value->double_min, move_value->double_max);
		} else if (move_value->move_value_type ==
			   MOVE_VALUE_TYPE_SETTING_ADD) {
			move_value->double_to = move_value->double_from +
						move_value->double_value;
		} else {
			move_value->double_to = move_value->double_value;
		}
	} else if (move_value->value_type == MOVE_VALUE_COLOR) {
		vec4_from_rgba(&move_value->color_from,
			       (uint32_t)obs_data_get_int(
				       ss, move_value->setting_name));
		gs_float3_srgb_nonlinear_to_linear(move_value->color_from.ptr);
		if (move_value->move_value_type == MOVE_VALUE_TYPE_RANDOM) {
			move_value->color_to.w =
				rand_between_float(move_value->color_min.w,
						   move_value->color_max.w);
			move_value->color_to.x =
				rand_between_float(move_value->color_min.x,
						   move_value->color_max.x);
			move_value->color_to.y =
				rand_between_float(move_value->color_min.y,
						   move_value->color_max.y);
			move_value->color_to.z =
				rand_between_float(move_value->color_min.z,
						   move_value->color_max.z);
		} else if (move_value->move_value_type ==
			   MOVE_VALUE_TYPE_SETTING_ADD) {
			move_value->color_to.w = move_value->color_from.w +
						 move_value->color_value.w;
			move_value->color_to.x = move_value->color_from.x +
						 move_value->color_value.x;
			move_value->color_to.y = move_value->color_from.y +
						 move_value->color_value.y;
			move_value->color_to.z = move_value->color_from.z +
						 move_value->color_value.z;
		} else {
			vec4_copy(&move_value->color_to,
				  &move_value->color_value);
		}
		gs_float3_srgb_nonlinear_to_linear(move_value->color_to.ptr);

	} else if (move_value->value_type == MOVE_VALUE_TEXT) {
		const char *text_from =
			obs_data_get_string(ss, move_value->setting_name);
		move_value->double_from = parse_text(
			move_value->format_type, move_value->format, text_from);

		if (move_value->move_value_type == MOVE_VALUE_TYPE_RANDOM) {
			move_value->double_to = rand_between_double(
				move_value->double_min, move_value->double_max);
		} else if (move_value->move_value_type ==
			   MOVE_VALUE_TYPE_SETTING_ADD) {
			move_value->double_to = move_value->double_from +
						move_value->double_value;
		} else if (move_value->move_value_type ==
			   MOVE_VALUE_TYPE_TYPING) {
			bfree(move_value->text_from);
			move_value->text_from = bstrdup(text_from);
			move_value->text_from_len = strlen(text_from);
			move_value->text_step = 0;
			move_value->text_same = 0;
			while (move_value->text_same <
				       move_value->text_from_len &&
			       move_value->text_same <
				       move_value->text_to_len &&
			       move_value->text_from[move_value->text_same] ==
				       move_value
					       ->text_to[move_value->text_same])
				move_value->text_same++;
			move_value->text_steps = (move_value->text_from_len -
						  move_value->text_same) +
						 (move_value->text_to_len -
						  move_value->text_same);
		} else {
			move_value->double_to = move_value->double_value;
		}
	} else {
		if (strcmp(move_value->setting_name, VOLUME_SETTING) == 0) {
			move_value->int_from =
				(long long)(obs_source_get_volume(source) *
					    100.0f);
			move_value->double_from =
				(double)obs_source_get_volume(source) * 100.0;
		} else {
			move_value->int_from =
				obs_data_get_int(ss, move_value->setting_name);
			move_value->double_from = obs_data_get_double(
				ss, move_value->setting_name);
		}

		move_value->int_to = move_value->int_value;
		move_value->double_to = move_value->double_value;
	}
	obs_data_release(ss);
}

bool move_value_start_button(obs_properties_t *props, obs_property_t *property,
			     void *data)
{
	struct move_value_info *move_value = data;
	move_value_start(move_value);
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(property);
	return false;
}

void move_value_start_hotkey(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey,
			     bool pressed)
{
	if (!pressed)
		return;
	struct move_value_info *move_value = data;

	move_filter_start_hotkey(&move_value->move_filter);

	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
}

void move_value_update(void *data, obs_data_t *settings)
{
	struct move_value_info *move_value = data;

	move_filter_update(&move_value->move_filter, settings);
	obs_source_t *parent =
		obs_filter_get_parent(move_value->move_filter.source);
	if (parent &&
	    move_value->move_filter.move_start_hotkey ==
		    OBS_INVALID_HOTKEY_ID &&
	    move_value->move_filter.filter_name) {
		move_value->move_filter.move_start_hotkey =
			obs_hotkey_register_source(
				parent, move_value->move_filter.filter_name,
				move_value->move_filter.filter_name,
				move_value_start_hotkey, data);
	}

	const char *setting_filter_name =
		obs_data_get_string(settings, S_FILTER);
	if (!move_value->setting_filter_name ||
	    strcmp(move_value->setting_filter_name, setting_filter_name) != 0) {
		obs_weak_source_release(move_value->filter);
		move_value->filter = NULL;
		if (parent) {
			bfree(move_value->setting_filter_name);
			move_value->setting_filter_name =
				bstrdup(setting_filter_name);
			obs_source_t *filter = obs_source_get_filter_by_name(
				parent, move_value->setting_filter_name);
			move_value->filter = obs_source_get_weak_source(filter);
			obs_source_release(filter);
		}
	}

	const char *setting_name =
		obs_data_get_string(settings, S_SETTING_NAME);
	if (!move_value->setting_name ||
	    strcmp(move_value->setting_name, setting_name) != 0) {
		bfree(move_value->setting_name);

		move_value->setting_name = bstrdup(setting_name);
	}

	if (obs_data_has_user_value(settings, S_SINGLE_SETTING)) {
		obs_data_set_int(settings, S_MOVE_VALUE_TYPE,
				 obs_data_get_bool(settings, S_SINGLE_SETTING)
					 ? MOVE_VALUE_TYPE_SINGLE_SETTING
					 : MOVE_VALUE_TYPE_SETTINGS);
		obs_data_unset_user_value(settings, S_SINGLE_SETTING);
	}

	if (obs_data_get_int(settings, S_MOVE_VALUE_TYPE) !=
	    MOVE_VALUE_TYPE_SETTINGS) {
		obs_data_array_release(move_value->settings);
		move_value->settings = NULL;
	} else if (parent) {
		if (!move_value->settings)
			move_value->settings = obs_data_array_create();
		obs_source_t *source = NULL;
		if (move_value->setting_filter_name &&
		    strlen(move_value->setting_filter_name)) {
			source = obs_weak_source_get_source(move_value->filter);
			obs_source_release(source);
		} else {
			source = parent;
		}
		move_values_load_properties(move_value, source, settings);
	}

	move_value->move_value_type =
		obs_data_get_int(settings, S_MOVE_VALUE_TYPE);
	move_value->value_type = obs_data_get_int(settings, S_VALUE_TYPE);
	move_value->format_type =
		obs_data_get_int(settings, S_SETTING_FORMAT_TYPE);
	char *format = (char *)obs_data_get_string(settings, S_SETTING_FORMAT);
	if (move_value->format_type == MOVE_VALUE_FORMAT_FLOAT &&
	    strlen(format) == 0) {
		format = "%f";
		obs_data_set_string(settings, S_SETTING_FORMAT, format);
	}
	if (move_value->format_type == MOVE_VALUE_FORMAT_TIME &&
	    strlen(format) == 0) {
		format = "%X";
		obs_data_set_string(settings, S_SETTING_FORMAT, format);
	}
	if (!move_value->format || strcmp(move_value->format, format) != 0) {
		bfree(move_value->format);

		move_value->format = bstrdup(format);
	}
	move_value->decimals =
		(int)obs_data_get_int(settings, S_SETTING_DECIMALS);
	move_value->int_value = obs_data_get_int(settings, S_SETTING_INT);
	move_value->int_min = obs_data_get_int(settings, S_SETTING_INT_MIN);
	move_value->int_max = obs_data_get_int(settings, S_SETTING_INT_MAX);
	move_value->double_value =
		obs_data_get_double(settings, S_SETTING_FLOAT);
	move_value->double_min =
		obs_data_get_double(settings, S_SETTING_FLOAT_MIN);
	move_value->double_max =
		obs_data_get_double(settings, S_SETTING_FLOAT_MAX);
	vec4_from_rgba(&move_value->color_value,
		       (uint32_t)obs_data_get_int(settings, S_SETTING_COLOR));
	vec4_from_rgba(&move_value->color_min,
		       (uint32_t)obs_data_get_int(settings,
						  S_SETTING_COLOR_MIN));
	vec4_from_rgba(&move_value->color_max,
		       (uint32_t)obs_data_get_int(settings,
						  S_SETTING_COLOR_MAX));

	const char *text_to = obs_data_get_string(settings, S_SETTING_TEXT);
	if (!move_value->text_to || strcmp(move_value->text_to, text_to) != 0) {
		bfree(move_value->text_to);
		move_value->text_to = bstrdup(text_to);
		move_value->text_to_len = strlen(text_to);
	}

	if (move_value->move_filter.start_trigger == START_TRIGGER_LOAD) {
		move_value_start(move_value);
	}
}

static void *move_value_create(obs_data_t *settings, obs_source_t *source)
{
	struct move_value_info *move_value =
		bzalloc(sizeof(struct move_value_info));
	move_filter_init(&move_value->move_filter, source,
			 (void (*)(void *))move_value_start);
	move_value_update(move_value, settings);
	return move_value;
}

static void move_value_destroy(void *data)
{
	struct move_value_info *move_value = data;
	obs_weak_source_release(move_value->filter);
	move_filter_destroy(&move_value->move_filter);
	move_value->filter = NULL;

	bfree(move_value->format);
	bfree(move_value->text_from);
	bfree(move_value->text_to);
	bfree(move_value->setting_filter_name);
	bfree(move_value->setting_name);
	obs_data_array_release(move_value->settings);
	bfree(move_value);
}

void prop_list_add_filter(obs_source_t *parent, obs_source_t *child, void *data)
{
	UNUSED_PARAMETER(parent);
	obs_property_t *p = data;
	const char *name = obs_source_get_name(child);
	obs_property_list_add_string(p, name, name);
}

void prop_list_add_move_value_filter(obs_source_t *parent, obs_source_t *child,
				     void *data)
{
	UNUSED_PARAMETER(parent);
	if (strcmp(obs_source_get_unversioned_id(child),
		   MOVE_VALUE_FILTER_ID) != 0 &&
	    strcmp(obs_source_get_unversioned_id(child),
		   MOVE_AUDIO_VALUE_FILTER_ID) != 0)
		return;
	obs_property_t *p = data;
	const char *name = obs_source_get_name(child);
	obs_property_list_add_string(p, name, name);
}

bool move_value_get_value(obs_properties_t *props, obs_property_t *property,
			  void *data)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(property);
	struct move_value_info *move_value = data;
	bool settings_changed = false;
	obs_source_t *source;
	if (move_value->filter) {
		source = obs_weak_source_get_source(move_value->filter);
		obs_source_release(source);
	} else {
		source = obs_filter_get_parent(move_value->move_filter.source);
	}
	if (source == NULL || source == move_value->move_filter.source)
		return settings_changed;

	obs_data_t *settings =
		obs_source_get_settings(move_value->move_filter.source);
	if (strcmp(move_value->setting_name, VOLUME_SETTING) == 0) {
		const double value =
			(double)obs_source_get_volume(source) * 100.0;
		obs_data_set_double(settings, S_SETTING_FLOAT, value);
		obs_data_set_double(settings, S_SETTING_FLOAT_MIN, value);
		obs_data_set_double(settings, S_SETTING_FLOAT_MAX, value);
		obs_data_release(settings);
		return true;
	}
	obs_properties_t *sps = obs_source_properties(source);
	obs_property_t *sp = obs_properties_get(sps, move_value->setting_name);

	obs_data_t *ss = obs_source_get_settings(source);

	const enum obs_property_type prop_type = obs_property_get_type(sp);

	if (prop_type == OBS_PROPERTY_INT) {
		const long long value =
			obs_data_get_int(ss, move_value->setting_name);
		obs_data_set_int(settings, S_SETTING_INT, value);
		obs_data_set_int(settings, S_SETTING_INT_MIN, value);
		obs_data_set_int(settings, S_SETTING_INT_MAX, value);
		settings_changed = true;
	} else if (prop_type == OBS_PROPERTY_FLOAT) {
		const double value =
			obs_data_get_double(ss, move_value->setting_name);
		obs_data_set_double(settings, S_SETTING_FLOAT, value);
		obs_data_set_double(settings, S_SETTING_FLOAT_MIN, value);
		obs_data_set_double(settings, S_SETTING_FLOAT_MAX, value);
		settings_changed = true;
	} else if (prop_type == OBS_PROPERTY_COLOR ||
		   prop_type == OBS_PROPERTY_COLOR_ALPHA) {
		const long long color =
			obs_data_get_int(ss, move_value->setting_name);
		obs_data_set_int(settings, S_SETTING_COLOR, color);
		obs_data_set_int(settings, S_SETTING_COLOR_MIN, color);
		obs_data_set_int(settings, S_SETTING_COLOR_MAX, color);
		settings_changed = true;
	} else if (prop_type == OBS_PROPERTY_TEXT) {
		const char *text =
			obs_data_get_string(ss, move_value->setting_name);
		if (move_value->move_value_type == MOVE_VALUE_TYPE_TYPING) {
			obs_data_set_string(settings, S_SETTING_TEXT, text);
		} else {
			const double value = parse_text(move_value->format_type,
							move_value->format,
							text);
			obs_data_set_double(settings, S_SETTING_FLOAT, value);
			obs_data_set_double(settings, S_SETTING_FLOAT_MIN,
					    value);
			obs_data_set_double(settings, S_SETTING_FLOAT_MAX,
					    value);
		}
		settings_changed = true;
	}
	obs_data_release(settings);
	obs_properties_destroy(sps);
	return settings_changed;
}

bool move_value_get_values(obs_properties_t *props, obs_property_t *property,
			   void *data)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(property);
	struct move_value_info *move_value = data;
	obs_source_t *source;
	if (move_value->filter) {
		source = obs_weak_source_get_source(move_value->filter);
		obs_source_release(source);
	} else {
		source = obs_filter_get_parent(move_value->move_filter.source);
	}
	if (source == NULL || source == move_value->move_filter.source)
		return false;

	obs_data_t *settings =
		obs_source_get_settings(move_value->move_filter.source);
	obs_data_t *ss = obs_source_get_settings(source);

	const size_t count = obs_data_array_count(move_value->settings);
	for (size_t i = 0; i < count; i++) {
		obs_data_t *item = obs_data_array_item(move_value->settings, i);
		const char *name = obs_data_get_string(item, S_SETTING_NAME);
		const long long value_type =
			obs_data_get_int(item, S_VALUE_TYPE);
		if (value_type == MOVE_VALUE_INT) {
			const long long value = obs_data_get_int(ss, name);
			obs_data_set_int(settings, name, value);
		} else if (value_type == MOVE_VALUE_FLOAT) {
			const double value = obs_data_get_double(ss, name);
			obs_data_set_double(settings, name, value);
		} else if (value_type == MOVE_VALUE_COLOR) {
			const long long color = obs_data_get_int(ss, name);
			obs_data_set_int(settings, name, color);
		} else if (value_type == MOVE_VALUE_TEXT) {
			const char *text = obs_data_get_string(ss, name);
			const double value = parse_text(move_value->format_type,
							move_value->format,
							text);
			obs_data_set_double(settings, name, value);
		}
	}

	if (count > 0) {
		obs_properties_t *sps = obs_source_properties(source);
		load_properties(sps, move_value->settings, settings, ss);
		obs_properties_destroy(sps);
	}
	obs_data_release(ss);
	obs_data_release(settings);
	return count > 0;
}

void copy_properties(obs_properties_t *props_from, obs_properties_t *props_to,
		     obs_data_t *data_from, obs_data_t *data_to,
		     obs_property_t *setting_list)
{
	obs_property_t *prop_from = obs_properties_first(props_from);
	for (; prop_from != NULL; obs_property_next(&prop_from)) {
		const char *name = obs_property_name(prop_from);
		const char *description = obs_property_description(prop_from);
		if (!obs_property_visible(prop_from))
			continue;
		obs_property_t *prop_to = NULL;
		const enum obs_property_type prop_type =
			obs_property_get_type(prop_from);
		if (prop_type == OBS_PROPERTY_GROUP) {
			obs_properties_t *group_to = obs_properties_create();
			copy_properties(obs_property_group_content(prop_from),
					group_to, data_from, data_to,
					setting_list);
			if (obs_properties_first(group_to) == NULL) {
				obs_properties_destroy(group_to);
			} else {
				prop_to = obs_properties_add_group(
					props_to, name, description,
					obs_property_group_type(prop_from),
					group_to);
			}

		} else if (prop_type == OBS_PROPERTY_INT) {
			obs_property_list_add_string(setting_list, description,
						     name);
			if (obs_property_int_type(prop_from) ==
			    OBS_NUMBER_SLIDER) {
				prop_to = obs_properties_add_int_slider(
					props_to, name, description,
					obs_property_int_min(prop_from),
					obs_property_int_max(prop_from),
					obs_property_int_step(prop_from));
			} else {
				prop_to = obs_properties_add_int(
					props_to, name, description,
					obs_property_int_min(prop_from),
					obs_property_int_max(prop_from),
					obs_property_int_step(prop_from));
			}
			if (obs_data_has_default_value(data_from, name))
				obs_data_set_default_int(
					data_to, name,
					obs_data_get_default_int(data_from,
								 name));
			obs_property_int_set_suffix(
				prop_to, obs_property_int_suffix(prop_from));

		} else if (prop_type == OBS_PROPERTY_FLOAT) {
			obs_property_list_add_string(setting_list, description,
						     name);
			if (obs_property_float_type(prop_from) ==
			    OBS_NUMBER_SLIDER) {
				prop_to = obs_properties_add_float_slider(
					props_to, name, description,
					obs_property_float_min(prop_from),
					obs_property_float_max(prop_from),
					obs_property_float_step(prop_from));
			} else {
				prop_to = obs_properties_add_float(
					props_to, name, description,
					obs_property_float_min(prop_from),
					obs_property_float_max(prop_from),
					obs_property_float_step(prop_from));
			}
			if (obs_data_has_default_value(data_from, name))
				obs_data_set_default_double(
					data_to, name,
					obs_data_get_default_double(data_from,
								    name));
			obs_property_float_set_suffix(
				prop_to, obs_property_float_suffix(prop_from));
		} else if (prop_type == OBS_PROPERTY_COLOR ||
			   prop_type == OBS_PROPERTY_COLOR_ALPHA) {
			obs_property_list_add_string(setting_list, description,
						     name);
			prop_to = obs_properties_add_color(props_to, name,
							   description);
			if (obs_data_has_default_value(data_from, name))
				obs_data_set_default_int(
					data_to, name,
					obs_data_get_default_int(data_from,
								 name));
		} else if (prop_type == OBS_PROPERTY_TEXT) {
			obs_property_list_add_string(setting_list, description,
						     name);
		}
	}
}

bool move_value_filter_changed(void *data, obs_properties_t *props,
			       obs_property_t *property, obs_data_t *settings)
{
	UNUSED_PARAMETER(property);
	struct move_value_info *move_value = data;
	bool refresh = false;

	obs_source_t *parent =
		obs_filter_get_parent(move_value->move_filter.source);

	const char *filter_name = obs_data_get_string(settings, S_FILTER);
	if (!move_value->setting_filter_name ||
	    strcmp(move_value->setting_filter_name, filter_name) != 0 ||
	    (!move_value->filter && strlen(filter_name))) {
		bfree(move_value->setting_filter_name);
		move_value->setting_filter_name = bstrdup(filter_name);
		obs_weak_source_release(move_value->filter);
		obs_source_t *filter =
			obs_source_get_filter_by_name(parent, filter_name);
		move_value->filter = obs_source_get_weak_source(filter);
		obs_source_release(filter);
	}

	refresh = true;
	obs_property_t *p = obs_properties_get(props, S_SETTING_NAME);
	obs_property_list_clear(p);
	obs_property_list_add_string(p, obs_module_text("Setting.None"), "");

	obs_property_t *ps = obs_properties_get(props, S_SETTINGS);
	obs_properties_t *g = obs_property_group_content(ps);
	obs_property_t *i = obs_properties_first(g);
	while (i != NULL) {
		const char *name = obs_property_name(i);
		obs_property_next(&i);
		if (strcmp(name, "values_get") == 0)
			continue;
		obs_properties_remove_by_name(g, name);
	}

	obs_source_t *source;
	if (move_value->filter) {
		source = obs_weak_source_get_source(move_value->filter);
		obs_source_release(source);
	} else {
		source = parent;
	}
	obs_data_t *s = obs_source_get_settings(source);
	if (!s || source == move_value->move_filter.source)
		return refresh;

	if (obs_source_get_type(source) == OBS_SOURCE_TYPE_INPUT &&
	    (obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO))
		obs_property_list_add_string(
			p, obs_module_text("Setting.Volume"), VOLUME_SETTING);

	obs_properties_t *sps = obs_source_properties(source);
	copy_properties(sps, g, s, settings, p);
	obs_properties_destroy(sps);

	obs_data_release(s);
	return refresh;
}

bool move_value_format_type_changed(void *data, obs_properties_t *props,
				    obs_property_t *property,
				    obs_data_t *settings)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(property);
	obs_property_t *prop_format =
		obs_properties_get(props, S_SETTING_FORMAT);
	obs_property_t *prop_decimals =
		obs_properties_get(props, S_SETTING_DECIMALS);
	obs_property_set_visible(prop_format, false);
	obs_property_set_visible(prop_decimals, false);
	if (obs_data_get_int(settings, S_VALUE_TYPE) == MOVE_VALUE_TEXT &&
	    obs_data_get_int(settings, S_MOVE_VALUE_TYPE) !=
		    MOVE_VALUE_TYPE_TYPING) {
		const long long format_type =
			obs_data_get_int(settings, S_SETTING_FORMAT_TYPE);
		if (format_type == MOVE_VALUE_FORMAT_DECIMALS) {
			obs_property_set_visible(prop_decimals, true);
		} else {
			obs_property_set_visible(prop_format, true);
		}
	}
	return true;
}

bool move_value_setting_changed(void *data, obs_properties_t *props,
				obs_property_t *property, obs_data_t *settings)
{
	UNUSED_PARAMETER(property);
	struct move_value_info *move_value = data;
	bool refresh = false;
	const char *setting_name =
		obs_data_get_string(settings, S_SETTING_NAME);
	if (!move_value->setting_name ||
	    strcmp(move_value->setting_name, setting_name) != 0) {
		refresh = true;

		bfree(move_value->setting_name);
		move_value->setting_name = bstrdup(setting_name);
	}

	obs_source_t *source;
	if (move_value->filter) {
		source = obs_weak_source_get_source(move_value->filter);
		obs_source_release(source);
	} else {
		source = obs_filter_get_parent(move_value->move_filter.source);
	}
	if (source == move_value->move_filter.source)
		return refresh;

	obs_property_t *prop_int = obs_properties_get(props, S_SETTING_INT);
	obs_property_t *prop_int_min =
		obs_properties_get(props, S_SETTING_INT_MIN);
	obs_property_t *prop_int_max =
		obs_properties_get(props, S_SETTING_INT_MAX);
	obs_property_t *prop_float = obs_properties_get(props, S_SETTING_FLOAT);
	obs_property_t *prop_float_min =
		obs_properties_get(props, S_SETTING_FLOAT_MIN);
	obs_property_t *prop_float_max =
		obs_properties_get(props, S_SETTING_FLOAT_MAX);
	obs_property_t *prop_format_type =
		obs_properties_get(props, S_SETTING_FORMAT_TYPE);

	obs_property_t *prop_color = obs_properties_get(props, S_SETTING_COLOR);
	obs_property_t *prop_color_min =
		obs_properties_get(props, S_SETTING_COLOR_MIN);
	obs_property_t *prop_color_max =
		obs_properties_get(props, S_SETTING_COLOR_MAX);
	obs_property_t *prop_text = obs_properties_get(props, S_SETTING_TEXT);
	obs_property_set_visible(prop_int, false);
	obs_property_set_visible(prop_int_min, false);
	obs_property_set_visible(prop_int_max, false);
	obs_property_set_visible(prop_float, false);
	obs_property_set_visible(prop_float_min, false);
	obs_property_set_visible(prop_float_max, false);
	obs_property_set_visible(prop_format_type, false);
	obs_property_set_visible(prop_color, false);
	obs_property_set_visible(prop_color_min, false);
	obs_property_set_visible(prop_color_max, false);
	obs_property_set_visible(prop_text, false);
	const long long move_value_type =
		obs_data_get_int(settings, S_MOVE_VALUE_TYPE);

	if (strcmp(move_value->setting_name, VOLUME_SETTING) == 0) {
		if (move_value_type == MOVE_VALUE_TYPE_SINGLE_SETTING) {
			obs_property_set_visible(prop_float, true);
			obs_property_float_set_limits(prop_float, VOLUME_MIN,
						      VOLUME_MAX, VOLUME_STEP);
			obs_property_float_set_suffix(prop_float, "%");
			if (refresh)
				obs_data_set_double(
					settings, S_SETTING_FLOAT,
					obs_source_get_volume(source) * 100.0);
		} else if (move_value_type == MOVE_VALUE_TYPE_SETTING_ADD) {
			obs_property_set_visible(prop_float, true);
			obs_property_float_set_limits(prop_float, -VOLUME_MAX,
						      VOLUME_MAX, VOLUME_STEP);
			obs_property_float_set_suffix(prop_float, "%");
		} else if (move_value_type == MOVE_VALUE_TYPE_RANDOM) {
			obs_property_set_visible(prop_float_min, true);
			obs_property_set_visible(prop_float_max, true);
			obs_property_float_set_limits(prop_float_min,
						      VOLUME_MIN, VOLUME_MAX,
						      VOLUME_STEP);
			obs_property_float_set_limits(prop_float_max,
						      VOLUME_MIN, VOLUME_MAX,
						      VOLUME_STEP);
			obs_property_float_set_suffix(prop_float_min, "%");
			obs_property_float_set_suffix(prop_float_max, "%");
			if (refresh) {
				obs_data_set_double(
					settings, S_SETTING_FLOAT_MIN,
					obs_source_get_volume(source) * 100.0);
				obs_data_set_double(
					settings, S_SETTING_FLOAT_MAX,
					obs_source_get_volume(source) * 100.0);
			}
		}
		obs_data_set_int(settings, S_VALUE_TYPE, MOVE_VALUE_FLOAT);
		return true;
	}
	obs_data_t *ss = obs_source_get_settings(source);
	obs_properties_t *sps = obs_source_properties(source);
	obs_property_t *sp = obs_properties_get(sps, setting_name);
	const enum obs_property_type prop_type = obs_property_get_type(sp);
	if (prop_type == OBS_PROPERTY_INT) {
		if (move_value_type == MOVE_VALUE_TYPE_SINGLE_SETTING) {
			obs_property_set_visible(prop_int, true);
			obs_property_int_set_limits(prop_int,
						    obs_property_int_min(sp),
						    obs_property_int_max(sp),
						    obs_property_int_step(sp));
			obs_property_int_set_suffix(
				prop_int, obs_property_int_suffix(sp));
			if (refresh)
				obs_data_set_int(
					settings, S_SETTING_INT,
					obs_data_get_int(ss, setting_name));

		} else if (move_value_type == MOVE_VALUE_TYPE_SETTING_ADD) {
			obs_property_set_visible(prop_int, true);
			obs_property_int_set_limits(prop_int, -1000, 1000,
						    obs_property_int_step(sp));
			obs_property_int_set_suffix(
				prop_int, obs_property_int_suffix(sp));
		} else if (move_value_type == MOVE_VALUE_TYPE_RANDOM) {
			obs_property_set_visible(prop_int_min, true);
			obs_property_set_visible(prop_int_max, true);
			obs_property_int_set_limits(prop_int_min,
						    obs_property_int_min(sp),
						    obs_property_int_max(sp),
						    obs_property_int_step(sp));
			obs_property_int_set_limits(prop_int_max,
						    obs_property_int_min(sp),
						    obs_property_int_max(sp),
						    obs_property_int_step(sp));
			obs_property_int_set_suffix(
				prop_int_min, obs_property_int_suffix(sp));
			obs_property_int_set_suffix(
				prop_int_max, obs_property_int_suffix(sp));
			if (refresh) {
				obs_data_set_int(
					settings, S_SETTING_INT_MIN,
					obs_data_get_int(ss, setting_name));
				obs_data_set_int(
					settings, S_SETTING_INT_MAX,
					obs_data_get_int(ss, setting_name));
			}
		}
		obs_data_set_int(settings, S_VALUE_TYPE, MOVE_VALUE_INT);
	} else if (prop_type == OBS_PROPERTY_FLOAT) {
		if (move_value_type == MOVE_VALUE_TYPE_SINGLE_SETTING) {
			obs_property_set_visible(prop_float, true);
			obs_property_float_set_limits(
				prop_float, obs_property_float_min(sp),
				obs_property_float_max(sp),
				obs_property_float_step(sp));
			obs_property_float_set_suffix(
				prop_float, obs_property_float_suffix(sp));
			if (refresh)
				obs_data_set_double(
					settings, S_SETTING_FLOAT,
					obs_data_get_double(ss, setting_name));
		} else if (move_value_type == MOVE_VALUE_TYPE_SETTING_ADD) {
			obs_property_set_visible(prop_float, true);
			obs_property_float_set_limits(
				prop_float, -1000.0, 1000.0,
				obs_property_float_step(sp));
			obs_property_float_set_suffix(
				prop_float, obs_property_float_suffix(sp));
		} else if (move_value_type == MOVE_VALUE_TYPE_RANDOM) {
			obs_property_set_visible(prop_float_min, true);
			obs_property_set_visible(prop_float_max, true);
			obs_property_float_set_limits(
				prop_float_min, obs_property_float_min(sp),
				obs_property_float_max(sp),
				obs_property_float_step(sp));
			obs_property_float_set_limits(
				prop_float_max, obs_property_float_min(sp),
				obs_property_float_max(sp),
				obs_property_float_step(sp));
			obs_property_float_set_suffix(
				prop_float_min, obs_property_float_suffix(sp));
			obs_property_float_set_suffix(
				prop_float_max, obs_property_float_suffix(sp));
			if (refresh) {
				obs_data_set_double(
					settings, S_SETTING_FLOAT_MIN,
					obs_data_get_double(ss, setting_name));
				obs_data_set_double(
					settings, S_SETTING_FLOAT_MAX,
					obs_data_get_double(ss, setting_name));
			}
		}
		obs_data_set_int(settings, S_VALUE_TYPE, MOVE_VALUE_FLOAT);
	} else if (prop_type == OBS_PROPERTY_COLOR ||
		   prop_type == OBS_PROPERTY_COLOR_ALPHA) {
		if (move_value_type == MOVE_VALUE_TYPE_SINGLE_SETTING) {
			obs_property_set_visible(prop_color, true);
			if (refresh)
				obs_data_set_int(
					settings, S_SETTING_COLOR,
					obs_data_get_int(ss, setting_name));
		} else if (move_value_type == MOVE_VALUE_TYPE_SETTING_ADD) {
			obs_property_set_visible(prop_color, true);
		} else if (move_value_type == MOVE_VALUE_TYPE_RANDOM) {
			obs_property_set_visible(prop_color_min, true);
			obs_property_set_visible(prop_color_max, true);
			if (refresh) {
				obs_data_set_int(
					settings, S_SETTING_COLOR_MIN,
					obs_data_get_int(ss, setting_name));
				obs_data_set_int(
					settings, S_SETTING_COLOR_MAX,
					obs_data_get_int(ss, setting_name));
			}
		}
		obs_data_set_int(settings, S_VALUE_TYPE, MOVE_VALUE_COLOR);
	} else if (prop_type == OBS_PROPERTY_TEXT) {
		if (move_value_type == MOVE_VALUE_TYPE_SINGLE_SETTING) {
			obs_property_set_visible(prop_format_type, true);
			obs_property_set_visible(prop_float, true);
			obs_property_float_set_limits(prop_float, -DBL_MAX,
						      DBL_MAX, 1.0);
			obs_property_float_set_suffix(prop_float, NULL);
			if (refresh) {
				const char *text_val =
					obs_data_get_string(ss, setting_name);
				const double val = strtod(text_val, NULL);
				obs_data_set_double(settings, S_SETTING_FLOAT,
						    val);
			}
		} else if (move_value_type == MOVE_VALUE_TYPE_SETTING_ADD) {
			obs_property_set_visible(prop_format_type, true);
			obs_property_set_visible(prop_float, true);
			obs_property_float_set_limits(prop_float, -DBL_MAX,
						      DBL_MAX, 1.0);
			obs_property_float_set_suffix(prop_float, NULL);
		} else if (move_value_type == MOVE_VALUE_TYPE_RANDOM) {
			obs_property_set_visible(prop_format_type, true);
			obs_property_set_visible(prop_float_min, true);
			obs_property_set_visible(prop_float_max, true);
			obs_property_float_set_limits(prop_float_min, -DBL_MAX,
						      DBL_MAX, 1.0);
			obs_property_float_set_limits(prop_float_max, -DBL_MAX,
						      DBL_MAX, 1.0);
			obs_property_float_set_suffix(prop_float_min, NULL);
			obs_property_float_set_suffix(prop_float_max, NULL);
			if (refresh) {
				const char *text_val =
					obs_data_get_string(ss, setting_name);
				const double val = strtod(text_val, NULL);
				obs_data_set_double(settings,
						    S_SETTING_FLOAT_MIN, val);
				obs_data_set_double(settings,
						    S_SETTING_FLOAT_MAX, val);
			}
		} else if (move_value_type == MOVE_VALUE_TYPE_TYPING) {
			obs_property_set_visible(prop_text, true);
		}
		obs_data_set_int(settings, S_VALUE_TYPE, MOVE_VALUE_TEXT);
	} else {
		obs_data_set_int(settings, S_VALUE_TYPE, MOVE_VALUE_UNKNOWN);
	}
	obs_data_release(ss);
	obs_properties_destroy(sps);
	move_value_format_type_changed(data, props, property, settings);
	return true;
}

bool move_value_type_changed(void *data, obs_properties_t *props,
			     obs_property_t *property, obs_data_t *settings)
{
	bool refresh = false;
	const long long move_value_type =
		obs_data_get_int(settings, S_MOVE_VALUE_TYPE);

	obs_property_t *p = obs_properties_get(props, S_SETTING_VALUE);
	if (obs_property_visible(p) !=
	    (move_value_type != MOVE_VALUE_TYPE_SETTINGS)) {
		obs_property_set_visible(p, move_value_type !=
						    MOVE_VALUE_TYPE_SETTINGS);
		refresh = true;
	}
	p = obs_properties_get(props, S_SETTINGS);
	if (obs_property_visible(p) !=
	    (move_value_type == MOVE_VALUE_TYPE_SETTINGS)) {
		obs_property_set_visible(p, move_value_type ==
						    MOVE_VALUE_TYPE_SETTINGS);
		refresh = true;
	}
	return move_value_setting_changed(data, props, property, settings) ||
	       refresh;
}

bool move_value_decimals_changed(void *data, obs_properties_t *props,
				 obs_property_t *property, obs_data_t *settings)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(property);
	const int decimals =
		(int)obs_data_get_int(settings, S_SETTING_DECIMALS);
	const double step = pow(10.0, -1.0 * (double)decimals);
	obs_property_t *prop_float = obs_properties_get(props, S_SETTING_FLOAT);
	obs_property_t *prop_float_min =
		obs_properties_get(props, S_SETTING_FLOAT_MIN);
	obs_property_t *prop_float_max =
		obs_properties_get(props, S_SETTING_FLOAT_MAX);

	obs_property_float_set_limits(prop_float, -DBL_MAX, DBL_MAX, step);
	obs_property_float_set_limits(prop_float_min, -DBL_MAX, DBL_MAX, step);
	obs_property_float_set_limits(prop_float_max, -DBL_MAX, DBL_MAX, step);

	return true;
}

static obs_properties_t *move_value_properties(void *data)
{
	obs_properties_t *ppts = obs_properties_create();
	struct move_value_info *move_value = data;
	obs_source_t *parent =
		obs_filter_get_parent(move_value->move_filter.source);
	obs_property_t *p = obs_properties_add_list(ppts, S_FILTER,
						    obs_module_text("Filter"),
						    OBS_COMBO_TYPE_LIST,
						    OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(p, obs_module_text("Filter.None"), "");
	obs_source_enum_filters(parent, prop_list_add_filter, p);

	obs_property_set_modified_callback2(p, move_value_filter_changed, data);

	p = obs_properties_add_list(ppts, S_MOVE_VALUE_TYPE,
				    obs_module_text("MoveValueType"),
				    OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(
		p, obs_module_text("MoveValueType.SingleSetting"),
		MOVE_VALUE_TYPE_SINGLE_SETTING);
	obs_property_list_add_int(p, obs_module_text("MoveValueType.Settings"),
				  MOVE_VALUE_TYPE_SETTINGS);
	obs_property_list_add_int(p, obs_module_text("MoveValueType.Random"),
				  MOVE_VALUE_TYPE_RANDOM);
	obs_property_list_add_int(p,
				  obs_module_text("MoveValueType.SettingAdd"),
				  MOVE_VALUE_TYPE_SETTING_ADD);
	obs_property_list_add_int(p, obs_module_text("MoveValueType.Type"),
				  MOVE_VALUE_TYPE_TYPING);

	obs_property_set_modified_callback2(p, move_value_type_changed, data);

	obs_properties_t *setting_value = obs_properties_create();

	p = obs_properties_add_list(setting_value, S_SETTING_NAME,
				    obs_module_text("Setting"),
				    OBS_COMBO_TYPE_LIST,
				    OBS_COMBO_FORMAT_STRING);

	obs_property_list_add_string(p, obs_module_text("Setting.None"), "");

	obs_property_set_modified_callback2(p, move_value_setting_changed,
					    data);

	p = obs_properties_add_list(setting_value, S_SETTING_FORMAT_TYPE,
				    obs_module_text("FormatType"),
				    OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, obs_module_text("FormatType.Decimals"),
				  MOVE_VALUE_FORMAT_DECIMALS);
	obs_property_list_add_int(p, obs_module_text("FormatType.Float"),
				  MOVE_VALUE_FORMAT_FLOAT);
	obs_property_list_add_int(p, obs_module_text("FormatType.Time"),
				  MOVE_VALUE_FORMAT_TIME);
	obs_property_set_visible(p, false);

	obs_property_set_modified_callback2(p, move_value_format_type_changed,
					    data);

	p = obs_properties_add_text(setting_value, S_SETTING_FORMAT,
				    obs_module_text("Format"),
				    OBS_TEXT_DEFAULT);
	obs_property_set_visible(p, false);
	p = obs_properties_add_int(setting_value, S_SETTING_DECIMALS,
				   obs_module_text("Decimals"), -10, 10, 1);
	obs_property_set_visible(p, false);
	obs_property_set_modified_callback2(p, move_value_decimals_changed,
					    data);

	p = obs_properties_add_int(setting_value, S_SETTING_INT,
				   obs_module_text("Value"), 0, 0, 0);
	obs_property_set_visible(p, false);
	p = obs_properties_add_int(setting_value, S_SETTING_INT_MIN,
				   obs_module_text("MinValue"), 0, 0, 0);
	obs_property_set_visible(p, false);
	p = obs_properties_add_int(setting_value, S_SETTING_INT_MAX,
				   obs_module_text("MaxValue"), 0, 0, 0);
	obs_property_set_visible(p, false);
	p = obs_properties_add_float(setting_value, S_SETTING_FLOAT,
				     obs_module_text("Value"), 0, 0, 0);
	obs_property_set_visible(p, false);
	p = obs_properties_add_float(setting_value, S_SETTING_FLOAT_MIN,
				     obs_module_text("MinValue"), 0, 0, 0);
	obs_property_set_visible(p, false);
	p = obs_properties_add_float(setting_value, S_SETTING_FLOAT_MAX,
				     obs_module_text("MaxValue"), 0, 0, 0);
	obs_property_set_visible(p, false);
	p = obs_properties_add_color(setting_value, S_SETTING_COLOR,
				     obs_module_text("Value"));
	obs_property_set_visible(p, false);
	p = obs_properties_add_color(setting_value, S_SETTING_COLOR_MIN,
				     obs_module_text("MinValue"));
	obs_property_set_visible(p, false);
	p = obs_properties_add_color(setting_value, S_SETTING_COLOR_MAX,
				     obs_module_text("MaxValue"));
	obs_property_set_visible(p, false);

	p = obs_properties_add_text(setting_value, S_SETTING_TEXT,
				    obs_module_text("Text"),
				    OBS_TEXT_MULTILINE);
	obs_property_set_visible(p, false);

	obs_properties_add_button(setting_value, "value_get",
				  obs_module_text("GetValue"),
				  move_value_get_value);

	obs_properties_add_group(ppts, S_SETTING_VALUE,
				 obs_module_text("Setting"), OBS_GROUP_NORMAL,
				 setting_value);

	obs_properties_t *settings = obs_properties_create();
	obs_properties_add_button(settings, "values_get",
				  obs_module_text("GetValues"),
				  move_value_get_values);
	obs_properties_add_group(ppts, S_SETTINGS, obs_module_text("Settings"),
				 OBS_GROUP_NORMAL, settings);

	move_filter_properties(&move_value->move_filter, ppts);

	return ppts;
}
void move_value_defaults(obs_data_t *settings)
{
	obs_data_set_default_bool(settings, S_SINGLE_SETTING, true);
	obs_data_set_default_bool(settings, S_CUSTOM_DURATION, true);
	obs_data_set_default_int(settings, S_DURATION, 300);
	obs_data_set_default_int(settings, S_EASING_MATCH, EASE_IN_OUT);
	obs_data_set_default_int(settings, S_EASING_FUNCTION_MATCH,
				 EASING_CUBIC);
	obs_data_set_default_bool(settings, S_ENABLED_MATCH_MOVING, true);
}

void move_value_video_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);
	struct move_value_info *filter = data;
	obs_source_skip_video_filter(filter->move_filter.source);
}

static const char *move_value_get_name(void *type_data)
{
	UNUSED_PARAMETER(type_data);
	return obs_module_text("MoveValueFilter");
}

float get_eased(float f, long long easing, long long easing_function);
void vec2_bezier(struct vec2 *dst, struct vec2 *begin, struct vec2 *control,
		 struct vec2 *end, const float t);

void move_value_stop(struct move_value_info *move_value)
{
	move_filter_stop(&move_value->move_filter);
}

void move_value_tick(void *data, float seconds)
{
	struct move_value_info *move_value = data;

	if (move_value->move_filter.filter_name &&
	    move_value->move_filter.move_start_hotkey ==
		    OBS_INVALID_HOTKEY_ID) {
		obs_source_t *parent =
			obs_filter_get_parent(move_value->move_filter.source);
		if (parent)
			move_value->move_filter.move_start_hotkey =
				obs_hotkey_register_source(
					parent,
					move_value->move_filter.filter_name,
					move_value->move_filter.filter_name,
					move_value_start_hotkey, data);
	}
	float t;
	if (!move_filter_tick(&move_value->move_filter, seconds, &t))
		return;

	obs_source_t *source;
	if (move_value->filter) {
		source = obs_weak_source_get_source(move_value->filter);
		obs_source_release(source);
	} else {
		source = obs_filter_get_parent(move_value->move_filter.source);
	}
	if (!source)
		return;
	obs_data_t *ss = obs_source_get_settings(source);
	bool update = true;
	if (move_value->settings) {
		const size_t count = obs_data_array_count(move_value->settings);
		for (size_t i = 0; i < count; i++) {
			obs_data_t *item =
				obs_data_array_item(move_value->settings, i);
			const char *setting_name =
				obs_data_get_string(item, S_SETTING_NAME);
			const long long value_type =
				obs_data_get_int(item, S_VALUE_TYPE);
			if (value_type == MOVE_VALUE_INT) {
				const long long int_from =
					obs_data_get_int(item, S_SETTING_FROM);
				const long long int_to =
					obs_data_get_int(item, S_SETTING_TO);
				const long long value_int =
					(long long)((1.0 - t) *
							    (double)int_from +
						    t * (double)int_to);
				obs_data_set_int(ss, setting_name, value_int);
			} else if (value_type == MOVE_VALUE_FLOAT) {
				const double double_from = obs_data_get_double(
					item, S_SETTING_FROM);
				const double double_to =
					obs_data_get_double(item, S_SETTING_TO);
				const double value_double =
					((1.0 - t) * double_from +
					 t * double_to);
				obs_data_set_double(ss, setting_name,
						    value_double);
			} else if (value_type == MOVE_VALUE_COLOR) {
				struct vec4 color_from;
				vec4_from_rgba(&color_from,
					       (uint32_t)obs_data_get_int(
						       item, S_SETTING_FROM));
				gs_float3_srgb_nonlinear_to_linear(
					color_from.ptr);
				struct vec4 color_to;
				vec4_from_rgba(&color_to,
					       (uint32_t)obs_data_get_int(
						       item, S_SETTING_TO));
				gs_float3_srgb_nonlinear_to_linear(
					color_to.ptr);
				struct vec4 color;
				color.w = (1.0f - t) * color_from.w +
					  t * color_to.w;
				color.x = (1.0f - t) * color_from.x +
					  t * color_to.x;
				color.y = (1.0f - t) * color_from.y +
					  t * color_to.y;
				color.z = (1.0f - t) * color_from.z +
					  t * color_to.z;
				gs_float3_srgb_linear_to_nonlinear(color.ptr);
				const long long value_int =
					vec4_to_rgba(&color);
				obs_data_set_int(ss, setting_name, value_int);
			}
		}
	} else if (move_value->value_type == MOVE_VALUE_INT) {
		const long long value_int =
			(long long)((1.0 - t) * (double)move_value->int_from +
				    t * (double)move_value->int_to);
		if (strcmp(move_value->setting_name, VOLUME_SETTING) == 0) {
			obs_source_set_volume(source,
					      (float)value_int / 100.0f);
			update = false;
		} else {
			obs_data_set_int(ss, move_value->setting_name,
					 value_int);
		}
	} else if (move_value->value_type == MOVE_VALUE_FLOAT) {
		const double value_double =
			(1.0 - t) * move_value->double_from +
			t * move_value->double_to;
		if (strcmp(move_value->setting_name, VOLUME_SETTING) == 0) {
			obs_source_set_volume(source,
					      (float)(value_double / 100.0));
			update = false;
		} else {
			obs_data_set_double(ss, move_value->setting_name,
					    value_double);
		}
	} else if (move_value->value_type == MOVE_VALUE_COLOR) {
		struct vec4 color;
		color.w = (1.0f - t) * move_value->color_from.w +
			  t * move_value->color_to.w;
		color.x = (1.0f - t) * move_value->color_from.x +
			  t * move_value->color_to.x;
		color.y = (1.0f - t) * move_value->color_from.y +
			  t * move_value->color_to.y;
		color.z = (1.0f - t) * move_value->color_from.z +
			  t * move_value->color_to.z;
		gs_float3_srgb_linear_to_nonlinear(color.ptr);
		const long long value_int = vec4_to_rgba(&color);
		obs_data_set_int(ss, move_value->setting_name, value_int);
	} else if (move_value->value_type == MOVE_VALUE_TEXT &&
		   move_value->move_value_type == MOVE_VALUE_TYPE_TYPING) {
		if (t * move_value->text_steps <= move_value->text_step &&
		    move_value->move_filter.moving) {
			obs_data_release(ss);
			return;
		}
		move_value->text_step =
			(size_t)(t * (float)move_value->text_steps);
		char *text = NULL;
		if (move_value->text_step <
		    move_value->text_from_len - move_value->text_same) {
			text = bstrdup_n(move_value->text_from,
					 move_value->text_from_len -
						 move_value->text_step);
		} else {
			text = bstrdup_n(move_value->text_to,
					 move_value->text_same +
						 move_value->text_step -
						 (move_value->text_from_len -
						  move_value->text_same));
		}
		obs_data_set_string(ss, move_value->setting_name, text);
		bfree(text);
	} else if (move_value->value_type == MOVE_VALUE_TEXT) {
		double value_double = (1.0 - t) * move_value->double_from +
				      t * move_value->double_to;
		char text[TEXT_BUFFER_SIZE];
		if (move_value->format_type == MOVE_VALUE_FORMAT_FLOAT) {
			if (snprintf(text, TEXT_BUFFER_SIZE, move_value->format,
				     value_double) == 0)
				text[0] = '\0';

		} else if (move_value->format_type == MOVE_VALUE_FORMAT_TIME) {
			long long t = (long long)value_double;
			struct tm *tm_info = gmtime((const time_t *)&t);
			if (strftime(text, TEXT_BUFFER_SIZE, move_value->format,
				     tm_info) == 0)
				text[0] = '\0';
		} else {
			if (move_value->decimals >= 0) {
				char format[10];
				snprintf(format, 10, "%%.%df",
					 move_value->decimals);
				snprintf(text, TEXT_BUFFER_SIZE, format,
					 value_double);
			} else {
				double factor = pow(
					10,
					-1.0 * (double)move_value->decimals);
				value_double =
					floor(value_double / factor) * factor;
				snprintf(text, TEXT_BUFFER_SIZE, "%.0f",
					 value_double);
			}
		}
		obs_data_set_string(ss, move_value->setting_name, text);
	} else {
		obs_data_item_t *item =
			obs_data_item_byname(ss, move_value->setting_name);
		const enum obs_data_number_type item_type =
			obs_data_item_numtype(item);
		if (item_type == OBS_DATA_NUM_INT) {
			const long long value_int =
				(long long)((1.0 -
					     t) * (double)move_value->int_from +
					    t * (double)move_value->int_to);
			if (strcmp(move_value->setting_name, VOLUME_SETTING) ==
			    0) {
				obs_source_set_volume(source, (float)value_int /
								      100.0f);
				update = false;
			} else {
				obs_data_set_int(ss, move_value->setting_name,
						 value_int);
			}
		} else if (item_type == OBS_DATA_NUM_DOUBLE) {
			const double value_double =
				(1.0 - t) * move_value->double_from +
				t * move_value->double_to;
			if (strcmp(move_value->setting_name, VOLUME_SETTING) ==
			    0) {
				obs_source_set_volume(
					source, (float)value_double / 100.0f);
				update = false;
			} else {
				obs_data_set_double(ss,
						    move_value->setting_name,
						    value_double);
			}
		}
		obs_data_item_release(&item);
	}
	obs_data_release(ss);
	if (update)
		obs_source_update(source, NULL);
	if (!move_value->move_filter.moving) {
		move_filter_ended(&move_value->move_filter);
	}
}

struct obs_source_info move_value_filter = {
	.id = MOVE_VALUE_FILTER_ID,
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = move_value_get_name,
	.create = move_value_create,
	.destroy = move_value_destroy,
	.get_properties = move_value_properties,
	.get_defaults = move_value_defaults,
	.video_render = move_value_video_render,
	.video_tick = move_value_tick,
	.update = move_value_update,
	.load = move_value_update,
	.activate = move_filter_activate,
	.deactivate = move_filter_deactivate,
	.show = move_filter_show,
	.hide = move_filter_hide,
};

struct obs_source_info move_audio_value_filter = {
	.id = MOVE_AUDIO_VALUE_FILTER_ID,
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_AUDIO,
	.get_name = move_value_get_name,
	.create = move_value_create,
	.destroy = move_value_destroy,
	.get_properties = move_value_properties,
	.get_defaults = move_value_defaults,
	.video_render = move_value_video_render,
	.video_tick = move_value_tick,
	.update = move_value_update,
	.load = move_value_update,
	.activate = move_filter_activate,
	.deactivate = move_filter_deactivate,
	.show = move_filter_show,
	.hide = move_filter_hide,
};
