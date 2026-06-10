#pragma once

#include <stdarg.h>
#include <cstring>

/*
 * Temporary buffers.
 */

#define BUFFER_MAX_SIZE 0x10000
#define BUFFER_COUNT 0x10

typedef struct {
    size_t size;
    size_t content_index;
    uint8_t content[BUFFER_MAX_SIZE];
} ubiquity_test_buffer_t;

ubiquity_test_buffer_t ubiquity_test_buffers[BUFFER_COUNT];

#define ubiquity_test_align(buffer_id, alignment) do {\
    ubiquity_test_buffer_t *const buffer = ubiquity_test_buffers + buffer_id;\
    buffer->content_index += (~buffer->content_index + 1) & (alignment - 1);\
    if (BUFFER_MAX_SIZE <= buffer->content_index) {\
        fprintf(stderr,"Error : Maximum buffer size exceeded for buffer %i.\n", buffer_id);\
        exit(1);\
    }\
} while (false);

#define ubiquity_test_push(buffer_id, value, value_type) do {\
    ubiquity_test_buffer_t *const buffer = ubiquity_test_buffers + buffer_id;\
    if (BUFFER_MAX_SIZE <= buffer->content_index + sizeof(value_type)) {\
        fprintf(stderr,"Error : Maximum buffer size exceeded for buffer %i.\n", buffer_id);\
        exit(1);\
    }\
    *(value_type *)(buffer->content + buffer->content_index) = value;\
    buffer->content_index += sizeof(value_type);\
} while (false);

#define buffer_align(buffer_id, alignment) ubiquity_test_align(buffer_id, alignment);
#define buffer_push_size(buffer_id, value) ubiquity_test_push(buffer_id, value, size_t);
#define buffer_push_u8(buffer_id, value)   ubiquity_test_push(buffer_id, value, uint8_t);
#define buffer_push_u16(buffer_id, value)  ubiquity_test_push(buffer_id, value, uint16_t);
#define buffer_push_u32(buffer_id, value)  ubiquity_test_push(buffer_id, value, uint32_t);
#define buffer_push_u64(buffer_id, value)  ubiquity_test_push(buffer_id, value, uint64_t);
#define buffer_push_i8(buffer_id, value)   ubiquity_test_push(buffer_id, value, int8_t);
#define buffer_push_i16(buffer_id, value)  ubiquity_test_push(buffer_id, value, int16_t);
#define buffer_push_i32(buffer_id, value)  ubiquity_test_push(buffer_id, value, int32_t);
#define buffer_push_i64(buffer_id, value)  ubiquity_test_push(buffer_id, value, int64_t);
#define buffer_push_f32(buffer_id, value)  ubiquity_test_push(buffer_id, value, float);
#define buffer_push_f64(buffer_id, value)  ubiquity_test_push(buffer_id, value, double);
#define buffer_push_mem(buffer_id, source, source_size) do {\
    ubiquity_test_buffer_t *const buffer = ubiquity_test_buffers + buffer_id;\
    if (BUFFER_MAX_SIZE <= buffer->content_index + (source_size)) {\
        fprintf(stderr,"Error : Maximum buffer size exceeded for buffer %i.\n", buffer_id);\
        exit(1);\
    }\
    memcpy(buffer->content + buffer->content_index, (source), (source_size));\
    buffer->content_index += (source_size);\
} while (false);

#define buffer_clear(buffer_id) ubiquity_test_buffers[buffer_id].content_index = 0;
#define buffer_size(buffer_id) (ubiquity_test_buffers[buffer_id].content_index)
#define buffer_content(buffer_id) (ubiquity_test_buffers[buffer_id].content)
#define buffer_copy(buffer_id, destination) memcpy((destination), ubiquity_test_buffers[buffer_id].content, buffer_size(buffer_id));

#define buffer_push_buffer(destination_buffer_id, source_buffer_id) buffer_push_mem((destination_buffer_id), buffer_content(source_buffer_id), buffer_size(source_buffer_id));

/*
 * Event type declaration.
 */

#define MAX_EVENT_TYPE_COUNT 0x100
#define MAX_EVENT_TYPE_ARGUMENT_COUNT 0x10

typedef struct {
    const char *name;
    const char *type;
} event_type_argument_t;

typedef struct {
    const char *name;
    size_t argument_count;
    event_type_argument_t arguments[MAX_EVENT_TYPE_ARGUMENT_COUNT];
} event_type_t;

event_type_t event_types[MAX_EVENT_TYPE_COUNT];
size_t event_type_count = 0;

void ubiquity_test_declare(const char *const event_name, size_t arg_count, ...) {
    event_type_t *const event_type = event_types + event_type_count++;
    event_type->name = event_name;
    event_type->argument_count = 0;

    va_list arg_list;
    va_start(arg_list, arg_count);
    for (size_t cursor = 0; cursor < arg_count; ++cursor) {
        event_type_argument_t *const event_type_argument = event_type->arguments + event_type->argument_count++;
        event_type_argument->name = va_arg(arg_list, const char *);
        event_type_argument->type = va_arg(arg_list, const char *);
    }

    va_end(arg_list);
}

static inline event_type_t *get_event_type_by_index(
        const char *const filename,
        const char *const func,
        const int line,
        const char*const event_name
        )
{
    size_t cursor = 0;
    while (cursor < event_type_count) {
        if (event_types[cursor].name == event_name) {
            return event_types + cursor;
        }
        ++cursor;
    }
    if (cursor == event_type_count) {
        fprintf(stderr,"Error (%s -> %s -> %i): Event type `%s` not declared.\n", filename, func, line, event_name);
        exit(1);
    }
    return NULL;
}


/*
 * Event emission.
 */


#define MAX_EVENT_COUNT 0x100000
#define MAX_EVENT_ARGUMENT_COUNT 0x10

typedef struct {
    size_t size;
    uint64_t value;
} event_argument_t;

typedef struct {
    const char *name;
    uint64_t argument_map;
    bool consumed;
    event_argument_t arguments[MAX_EVENT_ARGUMENT_COUNT];
} event_t;

event_t emitted_events[MAX_EVENT_COUNT];
size_t g_emitted_event_count = 0;
size_t g_consumed_event_count = 0;

event_t expected_events[MAX_EVENT_COUNT];
size_t g_expected_event_count = 0;

size_t get_event_type_argument_index(
        const char *const filename,
        const char *const func,
        const int line,
        const event_type_t *const event_type,
        const char *argument_name
        )
{
    size_t cursor = 0;
    while (cursor < event_type->argument_count) {
        const event_type_argument_t *const event_type_argument = event_type->arguments + cursor;
        if (event_type_argument->name == argument_name) {
            return cursor;
        }
        ++cursor;
    }
    fprintf(stderr,"get_event_type_argument_index `%s`\n", argument_name);
    if (cursor == event_type->argument_count) {
        fprintf(stderr,"Error (%s -> %s -> %i): Argument `%s` not declared in event type `%s`. Available options include :", filename, func, line, argument_name, event_type->name);
        fprintf(stderr,"`%s`", event_type->arguments[0].name);
        for (cursor = 1; cursor < event_type->argument_count; ++cursor) {
            fprintf(stderr,", `%s`", event_type->arguments[cursor].name);
        }
        fprintf(stderr,".\n");
        exit(1);
    }
    return cursor;
}

const char *const SIZE_TYPE = "size";
const char *const U8_TYPE = "u8";
const char *const U16_TYPE = "u16";
const char *const U32_TYPE = "u32";
const char *const U64_TYPE = "u64";
const char *const I8_TYPE = "i8";
const char *const I16_TYPE = "i16";
const char *const I32_TYPE = "i32";
const char *const I64_TYPE = "i64";
const char *const F32_TYPE = "f32";
const char *const F64_TYPE = "f64";
const char *const POINTER_TYPE = "pointer";
const char *const BUFFER_TYPE = "buffer";
const char *const STRING_TYPE = "string";

event_type_t *ubiquity_test_fill_event(
        const char *const filename,
        const char *const func,
        const int line,
        event_t *const event,
        const char *const event_name,
        size_t argument_count,
        va_list arg_list
        )
{
    event_type_t *const event_type = get_event_type_by_index(filename, func, line, event_name);

    event->name = event_name;
    event->argument_map = 0;
    event->consumed = 0;

    if (0 == argument_count) {
        argument_count = event_type->argument_count;
    }
    for (size_t cursor = 0; cursor < argument_count; ++cursor) {
        const char *argument_name = va_arg(arg_list, const char *);
        //fprintf(stderr,"ubiquity_test_fill_event `%lu` : `%s`\n", cursor, argument_name);
        const size_t argument_index = get_event_type_argument_index(filename, func, line, event_type, argument_name);
        event->argument_map |= 1ull << argument_index;
        const event_type_argument_t *const argument = event_type->arguments + argument_index;
        event_argument_t *const event_argument = event->arguments + argument_index;
        
        event_argument->value = 0;  // zero out first — clean slate for narrow types
        event_argument->size = 0;
        
        if (SIZE_TYPE == argument->type) {
            size_t tmp = va_arg(arg_list, size_t);
            memcpy(&event_argument->value, &tmp, sizeof(tmp));
        } else if (U8_TYPE == argument->type) {
            uint8_t tmp = (uint8_t)va_arg(arg_list, uint64_t);
            memcpy(&event_argument->value, &tmp, sizeof(tmp));
        } else if (U16_TYPE == argument->type) {
            uint16_t tmp = (uint16_t)va_arg(arg_list, uint64_t);
            memcpy(&event_argument->value, &tmp, sizeof(tmp));
        } else if (U32_TYPE == argument->type) {
            uint32_t tmp = (uint32_t)va_arg(arg_list, uint64_t);
            memcpy(&event_argument->value, &tmp, sizeof(tmp));
        } else if (U64_TYPE == argument->type) {
            uint64_t tmp = va_arg(arg_list, uint64_t);
            memcpy(&event_argument->value, &tmp, sizeof(tmp));
        } else if (I8_TYPE == argument->type) {
            int8_t tmp = (int8_t)va_arg(arg_list, int64_t);
            memcpy(&event_argument->value, &tmp, sizeof(tmp));
        } else if (I16_TYPE == argument->type) {
            int16_t tmp = (int16_t)va_arg(arg_list, int64_t);
            memcpy(&event_argument->value, &tmp, sizeof(tmp));
        } else if (I32_TYPE == argument->type) {
            int32_t tmp = (int32_t)va_arg(arg_list, int64_t);
            memcpy(&event_argument->value, &tmp, sizeof(tmp));
        } else if (I64_TYPE == argument->type) {
            int64_t tmp = va_arg(arg_list, int64_t);
            memcpy(&event_argument->value, &tmp, sizeof(tmp));
        } else if (F32_TYPE == argument->type) {
            float tmp = (float)va_arg(arg_list, double);
            memcpy(&event_argument->value, &tmp, sizeof(tmp));
        } else if (F64_TYPE == argument->type) {
            double tmp = va_arg(arg_list, double);
            memcpy(&event_argument->value, &tmp, sizeof(tmp));
        } else if (POINTER_TYPE == argument->type) {
            void *tmp = va_arg(arg_list, void *);
            memcpy(&event_argument->value, &tmp, sizeof(tmp));
        } else if (BUFFER_TYPE == argument->type) {
            event_argument->size = va_arg(arg_list, size_t);
            const char *buffer = va_arg(arg_list, const char *);
            char *const new_buffer = (char *)malloc(event_argument->size);
            memcpy(new_buffer, buffer, event_argument->size);
            memcpy(&event_argument->value, &new_buffer, sizeof(new_buffer));
        } else if (STRING_TYPE == argument->type) {
            const char *string = va_arg(arg_list, const char *);
            event_argument->size = strlen(string);
            char *const new_string = (char *)malloc(event_argument->size + 1);
            strcpy(new_string, string);
            memcpy(&event_argument->value, &new_string, sizeof(new_string));
        } else {
            fprintf(stderr,"Error (%s -> %s -> %i): Invalid argument type `%s` for argument `%s` registered in event type `%s`.\n",
                    filename, func, line,
                    argument->type, argument->name, event_type->name);
            exit(1);
        }
    }

    va_end(arg_list);

    return event_type;
}

void ubiquity_test_release_event(
        const char *const filename,
        const char *const func,
        const int line,
        const event_t *const event
        )
{
    event_type_t *const event_type = get_event_type_by_index(filename, func, line, event->name);
    for (size_t argument_index = 0; argument_index < event_type->argument_count; ++argument_index) {
        if (0 == (event->argument_map & (1ull << argument_index))) {
            continue;
        }
        const event_argument_t *const event_argument = event->arguments + argument_index;
        const event_type_argument_t *const event_type_argument = event_type->arguments + argument_index;
        const char *const type = event_type_argument->type;

        if (BUFFER_TYPE == type || STRING_TYPE == type) {
            void *ptr;
            memcpy(&ptr, &event_argument->value, sizeof(ptr));
            free(ptr);
        } else if (SIZE_TYPE  == type ||
                   U8_TYPE    == type || U16_TYPE == type || U32_TYPE == type || U64_TYPE == type ||
                   I8_TYPE    == type || I16_TYPE == type || I32_TYPE == type || I64_TYPE == type ||
                   F32_TYPE   == type || F64_TYPE == type ||
                   POINTER_TYPE == type) {
            /* nothing to release */
        } else {
            fprintf(stderr, "Error (%s -> %s -> %i): Invalid argument type `%s` for argument `%s` registered in event type `%s`.\n",
                    filename, func, line,
                    event_type_argument->type, event_type_argument->name, event_type->name);
            exit(1);
        }
    }
}

bool ubiquity_test_assert_almost_equal_f(
        const char *const filename,
        const char *const func,
        const int line,
        double error_margin,
        const size_t argument_count,
        ...
        )
{
    va_list arg_list;
    va_start(arg_list, argument_count);
    int64_t min_value = INT64_MAX;
    int64_t max_value = INT64_MIN;
    for (size_t cursor = 0; cursor < argument_count; ++cursor) {
        const int64_t value = va_arg(arg_list, int64_t);
        if (value < min_value) {
            min_value = value;
        }
        if (value > max_value) {
            max_value = value;
        }
    }
    const double mean_value = ((double)min_value + (double)max_value) / 2.0;
    const bool almost_equal = mean_value * (1.0 + error_margin) >= (double)max_value
        && mean_value * (1.0 - error_margin) <= (double)min_value;
    if (!almost_equal) {
        fprintf(stderr,"Error (%s -> %s -> %i): Expected all values to be equal within an error margin of `%lf` but got minimum `%ld` and maximum `%ld` instead.\n", filename, func, line, error_margin, min_value, max_value);
        return false;
    }
    return true;
}

bool ubiquity_test_assert_almost_equal_array_f(
        const char *const filename,
        const char *const func,
        const int line,
        double error_margin,
        const size_t argument_count,
        int64_t *const array
        )
{
    int64_t min_value = INT64_MAX;
    int64_t max_value = INT64_MIN;
    for (size_t cursor = 0; cursor < argument_count; ++cursor) {
        const int64_t value = array[cursor];
        if (value < min_value) {
            min_value = value;
        }
        if (value > max_value) {
            max_value = value;
        }
    }
    const double mean_value = ((double)min_value + (double)max_value) / 2.0;
    const bool almost_equal = mean_value * (1.0 + error_margin) >= (double)max_value
        && mean_value * (1.0 - error_margin) <= (double)min_value;
    if (!almost_equal) {
        fprintf(stderr,"Error (%s -> %s -> %i): Expected all values to be equal within an error margin of `%lf` but got minimum `%ld` and maximum `%ld` instead.\n", filename, func, line, error_margin, min_value, max_value);
        return false;
    }
    return true;
}

bool ubiquity_test_assert_almost_equal_inversly_weighted_array_f(
        const char *const filename,
        const char *const func,
        const int line,
        const double error_margin,
        const size_t argument_count,
        const int64_t *const array,
        const double *const weights
        )
{
    int64_t min_value = INT64_MAX;
    int64_t max_value = INT64_MIN;
    for (size_t cursor = 0; cursor < argument_count; ++cursor) {
        const int64_t value = (int64_t)((double)array[cursor] / weights[cursor]);
        if (value < min_value) {
            min_value = value;
        }
        if (value > max_value) {
            max_value = value;
        }
    }
    const double mean_value = ((double)min_value + (double)max_value) / 2.0;
    const bool almost_equal = mean_value * (1.0 + error_margin) >= (double)max_value
        && mean_value * (1.0 - error_margin) <= (double)min_value;
    if (!almost_equal) {
        fprintf(stderr,"Error (%s -> %s -> %i): Expected all values to be equal within an error margin of `%lf` but got minimum `%ld` and maximum `%ld` instead.\n", filename, func, line, error_margin, min_value, max_value);
        return false;
    }
    return true;
}

static inline void print_hex_line_text(
        const char *const source,
        const size_t size
        )
{
    static char text[0x11];
    memcpy(text, source, size);
    text[size] = 0;
    for (size_t char_cursor = 0; char_cursor < size; ++char_cursor) {
        if (text[char_cursor] < 0x20) {
            text[char_cursor] = 0x20;
        }
    }
    fprintf(stderr," |%-16s|", text);
}

static inline void print_hex_lines(
        const char *const buffer,
        const size_t size
        )
{
    for (size_t byte_cursor = 0; byte_cursor < size; ++byte_cursor) {
        if (0 == (byte_cursor & 0xful)) {
            if (0x10 <= byte_cursor) {
                print_hex_line_text(buffer + byte_cursor - 0x10, 0x10);
            }
            fprintf(stderr,"\n            ");
        }
        fprintf(stderr,"%02x ", (uint8_t)buffer[byte_cursor]);
    }
    if (0 < size) {
        if (0 != (((uint64_t)buffer + size) & 0xful)) {
            for (size_t byte_cursor = 0; byte_cursor < 0x10 - (size & 0xful); ++byte_cursor) {
                fprintf(stderr,"   ");
            }
            print_hex_line_text(buffer + size - (size & 0xful), (size & 0xful));
        } else {
            print_hex_line_text(buffer + size - 0x10, 0x10);
        }
    }
    fprintf(stderr,"\n");
}

void ubiquity_test_print_event(
        const char *const filename,
        const char *const func,
        const int line,
        const event_t *const event
        )
{
    event_type_t *const event_type = get_event_type_by_index(filename, func, line, event->name);
    fprintf(stderr,"    Event `%s` :\n", event->name);
    for (size_t argument_index = 0; argument_index < event_type->argument_count; ++argument_index) {
        if (0 == (event->argument_map & (1ull << argument_index))) {
            continue;
        }
        const event_type_argument_t *const event_type_argument = event_type->arguments + argument_index;
        const char *const name = event_type_argument->name;
        const uint64_t value = event->arguments[argument_index].value;
        const size_t size = event->arguments[argument_index].size;
        const char *const type = event_type_argument->type;

        if (SIZE_TYPE == type) {
            size_t tmp; memcpy(&tmp, &value, sizeof(tmp));
            fprintf(stderr,"        %s : %lu\n", name, tmp);
        } else if (U8_TYPE == type) {
            fprintf(stderr,"        %s : %u\n",  name, (uint8_t)value);
        } else if (U16_TYPE == type) {
            fprintf(stderr,"        %s : %u\n",  name, (uint16_t)value);
        } else if (U32_TYPE == type) {
            fprintf(stderr,"        %s : %u\n",  name, (uint32_t)value);
        } else if (U64_TYPE == type) {
            fprintf(stderr,"        %s : %lu\n", name, value);
        } else if (I8_TYPE == type) {
            fprintf(stderr,"        %s : %i\n",  name, (int8_t)value);
        } else if (I16_TYPE == type) {
            fprintf(stderr,"        %s : %i\n",  name, (int16_t)value);
        } else if (I32_TYPE == type) {
            fprintf(stderr,"        %s : %i\n",  name, (int32_t)value);
        } else if (I64_TYPE == type) {
            fprintf(stderr,"        %s : %li\n", name, (int64_t)value);
        } else if (F32_TYPE == type) {
            float tmp; memcpy(&tmp, &value, sizeof(tmp));
            fprintf(stderr,"        %s : %f\n",  name, tmp);
        } else if (F64_TYPE == type) {
            double tmp; memcpy(&tmp, &value, sizeof(tmp));
            fprintf(stderr,"        %s : %lf\n", name, tmp);
        } else if (POINTER_TYPE == type) {
            void *tmp; memcpy(&tmp, &value, sizeof(tmp));
            fprintf(stderr,"        %s : %p\n",  name, tmp);
        } else if (BUFFER_TYPE == type) {
            const char *tmp; memcpy(&tmp, &value, sizeof(tmp));
            fprintf(stderr,"        %s (%li) :", name, size);
            print_hex_lines(tmp, size);
        } else if (STRING_TYPE == type) {
            const char *tmp; memcpy(&tmp, &value, sizeof(tmp));
            fprintf(stderr,"        %s : %s\n",  name, tmp);
        } else {
            fprintf(stderr,"Error (%s -> %s -> %i): Error : Invalid argument type `%s` for argument `%s` registered in event type `%s`.\n",
                    filename, func, line,
                    event_type_argument->type, event_type_argument->name, event_type->name);
            exit(1);
        }
    }
}
void ubiquity_test_emit_f(
        const char *const filename,
        const char *const func,
        const int line,
        const char *const event_name,
        ...
        )
{
    //fprintf(stderr," => Emitting event type `%s`\n", event_name);
    if (MAX_EVENT_COUNT <= g_emitted_event_count) {
        fprintf(stderr,"Error (%s -> %s -> %i): Maximum event count (`%i`) exceeded during emission of event type `%s`.\n", filename, func, line, MAX_EVENT_COUNT, event_name);
        exit(1);
    }
    event_t *const emitted_event = emitted_events + g_emitted_event_count++;
    va_list arg_list;
    va_start(arg_list, event_name);
    ubiquity_test_fill_event(filename, func, line, emitted_event, event_name, 0, arg_list);
}

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define ubiquity_test_emit(...) ubiquity_test_emit_f(__FILENAME__, __func__, __LINE__, __VA_ARGS__)
#define ubiquity_test_expect(...) ubiquity_test_expect_f(__FILENAME__, __func__, __LINE__, __VA_ARGS__)
#define ubiquity_test_expect_partial(...) ubiquity_test_expect_partial_f(__FILENAME__, __func__, __LINE__, __VA_ARGS__)
#define ubiquity_test_count(...) ubiquity_test_count_f(__FILENAME__, __func__, __LINE__, __VA_ARGS__)
#define ubiquity_test_count_partial(...) ubiquity_test_count_partial_f(__FILENAME__, __func__, __LINE__, __VA_ARGS__)
#define ubiquity_test_count_filtered_by_arguments(...) ubiquity_test_count_filtered_by_arguments_f(__FILENAME__, __func__, __LINE__, __VA_ARGS__)
#define ubiquity_test_assert_almost_equal(...) if (!ubiquity_test_assert_almost_equal_f(__FILENAME__, __func__, __LINE__, __VA_ARGS__)) return 1;
#define ubiquity_test_assert_almost_equal_array(error_margin, argument_count, array) if (!ubiquity_test_assert_almost_equal_array_f(__FILENAME__, __func__, __LINE__, error_margin, argument_count, array)) return 1;
#define ubiquity_test_assert_almost_equal_inversly_weighted_array(error_margin, argument_count, array, weights) if (!ubiquity_test_assert_almost_equal_inversly_weighted_array_f(__FILENAME__, __func__, __LINE__, error_margin, argument_count, array, weights)) return 1;

#define ubiquity_test_assert_null(value) if (NULL != (value)) {\
            fprintf(stderr,"Error (%s -> %s -> %i): Expected NULL, got NOT NULL.\n", __FILENAME__, __func__, __LINE__);\
            return 1;\
        }

#define ubiquity_test_assert_not_null(value) if (NULL == (value)) {\
            fprintf(stderr,"Error (%s -> %s -> %i): Expected NOT NULL, got NULL.\n", __FILENAME__, __func__, __LINE__);\
            return 1;\
        }

#define ubiquity_test_assert_true(value) if (false == (value)) {\
            fprintf(stderr,"Error (%s -> %s -> %i): Expected true, got false.\n", __FILENAME__, __func__, __LINE__);\
            return 1;\
        }

#define ubiquity_test_assert_false(value) if (true == (value)) {\
            fprintf(stderr,"Error (%s -> %s -> %i): Expected false, got true.\n", __FILENAME__, __func__, __LINE__);\
            return 1;\
        }

#define ubiquity_test_assert_success ubiquity_test_assert_false

#define ubiquity_test_assert_equal(alpha, beta) if ((alpha) != (beta)) {\
            fprintf(stderr,"Error (%s -> %s -> %i): Expected equality, got inequality (%i vs %i).\n", __FILENAME__, __func__, __LINE__, (int)(alpha), (int)(beta));\
            return 1;\
        }

#define ubiquity_test_assert_inequal(alpha, beta) if ((alpha) == (beta)) {\
            fprintf(stderr,"Error (%s -> %s -> %i): Expected inequality, got equality (%i vs %i).\n", __FILENAME__, __func__, __LINE__, (int)(alpha), (int)(beta));\
            return 1;\
        }

#define ubiquity_test_assert_equal_string(alpha, beta) if (0 != strcmp(alpha, beta)) {\
            fprintf(stderr,"Error (%s -> %s -> %i): Expected equality, got inequality (`%s` vs `%s`).\n", __FILENAME__, __func__, __LINE__, alpha, beta);\
            return 1;\
        }

#define ubiquity_test_assert_inequal_string(alpha, beta) if (0 == strcmp(alpha, beta)) {\
            fprintf(stderr,"Error (%s -> %s -> %i): Expected inequality, got equality (`%s` vs `%s`).\n", __FILENAME__, __func__, __LINE__, alpha, beta);\
            return 1;\
        }

/*
 * Event expectation.
 */

bool compare_events(
        const event_t *const alpha,
        const event_t *const beta,
        event_type_t *const event_type
        )
{
    if (alpha->name != beta->name) {
        return false;
    }
    size_t equals = 1;
    const uint64_t argument_map = alpha->argument_map & beta->argument_map;
    for (size_t argument_index = 0; argument_index < event_type->argument_count; ++argument_index) {
        if (0 == (argument_map & (1ull << argument_index))) {
            continue;
        }
        const event_argument_t *const alpha_argument = alpha->arguments + argument_index;
        const event_argument_t *const beta_argument  = beta->arguments  + argument_index;
        const event_type_argument_t *const event_type_argument = event_type->arguments + argument_index;
        const char *const type = event_type_argument->type;

        if (SIZE_TYPE == type) {
            size_t a; memcpy(&a, &alpha_argument->value, sizeof(a));
            size_t b; memcpy(&b, &beta_argument->value,  sizeof(b));
            equals &= (a == b);
        } else if (U8_TYPE == type) {
            equals &= ((uint8_t)alpha_argument->value == (uint8_t)beta_argument->value);
        } else if (U16_TYPE == type) {
            equals &= ((uint16_t)alpha_argument->value == (uint16_t)beta_argument->value);
        } else if (U32_TYPE == type) {
            equals &= ((uint32_t)alpha_argument->value == (uint32_t)beta_argument->value);
        } else if (U64_TYPE == type) {
            equals &= (alpha_argument->value == beta_argument->value);
        } else if (I8_TYPE == type) {
            equals &= ((int8_t)alpha_argument->value == (int8_t)beta_argument->value);
        } else if (I16_TYPE == type) {
            equals &= ((int16_t)alpha_argument->value == (int16_t)beta_argument->value);
        } else if (I32_TYPE == type) {
            equals &= ((int32_t)alpha_argument->value == (int32_t)beta_argument->value);
        } else if (I64_TYPE == type) {
            equals &= ((int64_t)alpha_argument->value == (int64_t)beta_argument->value);
        } else if (F32_TYPE == type) {
            float a; memcpy(&a, &alpha_argument->value, sizeof(a));
            float b; memcpy(&b, &beta_argument->value,  sizeof(b));
            equals &= (a == b);
        } else if (F64_TYPE == type) {
            double a; memcpy(&a, &alpha_argument->value, sizeof(a));
            double b; memcpy(&b, &beta_argument->value,  sizeof(b));
            equals &= (a == b);
        } else if (POINTER_TYPE == type) {
            void *a; memcpy(&a, &alpha_argument->value, sizeof(a));
            void *b; memcpy(&b, &beta_argument->value,  sizeof(b));
            equals &= (a == b);
        } else if (BUFFER_TYPE == type) {
            void *a; memcpy(&a, &alpha_argument->value, sizeof(a));
            void *b; memcpy(&b, &beta_argument->value,  sizeof(b));
            equals &= (alpha_argument->size == beta_argument->size)
                   && 0 == memcmp(a, b, alpha_argument->size);
        } else if (STRING_TYPE == type) {
            const char *a; memcpy(&a, &alpha_argument->value, sizeof(a));
            const char *b; memcpy(&b, &beta_argument->value,  sizeof(b));
            equals &= 0 == strcmp(a, b);
        } else {
            fprintf(stderr,"Error : Invalid argument type `%s` for argument `%s` registered in event type `%s`.\n",
                    event_type_argument->type, event_type_argument->name, event_type->name);
            exit(1);
        }
    }
    return equals;
}
void ubiquity_test_expect_impl(
        const char *const filename,
        const char *const func,
        const int line,
        const char *const event_name,
        const size_t argument_count,
        va_list arg_list
        )
{
    //fprintf(stderr," => Expecting event type `%s`\n", event_name);
    event_t *const expected_event = expected_events + g_expected_event_count;
    event_type_t *const event_type = ubiquity_test_fill_event(filename, func, line, expected_event, event_name, argument_count, arg_list);
    bool found_something = false;
    for (size_t cursor = 0; cursor < g_emitted_event_count; ++cursor) {
        event_t *const emitted_event = emitted_events + cursor;
        if (false == emitted_event->consumed
                && compare_events(emitted_event, expected_event, event_type)) {
            emitted_event->consumed = true;
            ++g_consumed_event_count;
            //fprintf(stderr,"Found event type `%s`\n", event_name);
            found_something = true;
        }
    }
    if (!found_something) {
        fprintf(stderr,"Event NOT FOUND !\n");
        ++g_expected_event_count;
    } else {
        ubiquity_test_release_event(filename, func, line, expected_event);
    }
}

void ubiquity_test_expect_f(
        const char *const filename,
        const char *const func,
        const int line,
        const char *const event_name,
        ...
        )
{
    va_list arg_list;
    va_start(arg_list, event_name);
    ubiquity_test_expect_impl(filename, func, line, event_name, 0, arg_list);
}

void ubiquity_test_expect_partial_f(
        const char *const filename,
        const char *const func,
        const int line,
        const char *const event_name,
        ...
        )
{
    va_list arg_list;
    va_start(arg_list, event_name);
    const size_t argument_count = va_arg(arg_list, size_t);
    ubiquity_test_expect_impl(filename, func, line, event_name, argument_count, arg_list);
}

size_t ubiquity_test_count_impl(
        const char *const filename,
        const char *const func,
        const int line,
        const char *const event_name,
        const size_t argument_count,
        va_list arg_list
        )
{
    //fprintf(stderr," => Counting event type `%s`\n", event_name);
    event_t counted_event;
    event_type_t *const event_type = ubiquity_test_fill_event(filename, func, line, &counted_event, event_name, argument_count, arg_list);
    size_t count = 0;
    for (size_t cursor = 0; cursor < g_emitted_event_count; ++cursor) {
        event_t *const emitted_event = emitted_events + cursor;
        if (compare_events(emitted_event, &counted_event, event_type)) {
            ++count;
        }
    }
    //fprintf(stderr,"Event count : %lu\n", count);
    return count;
}

size_t ubiquity_test_count_f(
        const char *const filename,
        const char *const func,
        const int line,
        const char *const event_name,
        ...
        )
{
    va_list arg_list;
    va_start(arg_list, event_name);
    return ubiquity_test_count_impl(filename, func, line, event_name, 0, arg_list);
}

size_t ubiquity_test_count_partial_f(
        const char *const filename,
        const char *const func,
        const int line,
        const char *const event_name,
        ...
        )
{
    va_list arg_list;
    va_start(arg_list, event_name);
    const size_t argument_count = va_arg(arg_list, size_t);
    return ubiquity_test_count_impl(filename, func, line, event_name, argument_count, arg_list);
}

typedef bool (*ubiquity_test_filter_func_t)(
        const event_argument_t *const argument,
        void *const user_data
        );

bool ubiquity_test_ignore(
        const event_argument_t *const argument,
        void *const user_data
        )
{
    (void)argument;
    (void)user_data;
    return true;
}

size_t ubiquity_test_count_filtered_by_arguments_f(
        const char *const filename,
        const char *const func,
        const int line,
        const char *const event_name,
        ...
        )
{
    static ubiquity_test_filter_func_t filters[64];
    static void *user_data[64];

    va_list arg_list;
    va_start(arg_list, event_name);
    for (size_t cursor = 0; cursor < 64; ++cursor) {
        filters[cursor] = NULL;
        user_data[cursor] = NULL;
    }
    event_type_t *const event_type = get_event_type_by_index(filename, func, line, event_name);
    for (size_t cursor = 0; cursor < event_type->argument_count; ++cursor) {
        const char *argument_name = va_arg(arg_list, const char *);
        //fprintf(stderr,"ubiquity_test_count_filtered_by_arguments_f `%lu` : `%s`\n", cursor, argument_name);
        const size_t argument_index = get_event_type_argument_index(filename, func, line, event_type, argument_name);
        filters[argument_index] = (ubiquity_test_filter_func_t) va_arg(arg_list, void *);
        user_data[argument_index] = va_arg(arg_list, void *);
    }
    //fprintf(stderr," => Counting filtered event type `%s`\n", event_name);
    size_t count = 0;
    for (size_t cursor = 0; cursor < g_emitted_event_count; ++cursor) {
        event_t *const emitted_event = emitted_events + cursor;
        if (event_name != emitted_event->name) {
            continue;
        }
        for (size_t argument_index = 0; argument_index < event_type->argument_count; ++argument_index) {
            if (0 == (emitted_event->argument_map & (1ull << argument_index))) {
                continue;
            }
            if (!filters[argument_index](emitted_event->arguments + argument_index, user_data[argument_index])) {
                goto next;
            }
        }
        ++count;
        next:
        continue;
    }
    //fprintf(stderr,"Event count : %lu\n", count);
    return count;
}

#define ubiquity_test_done_expecting() do {\
    bool errors_happened = 0;\
    size_t unexpected_count = g_emitted_event_count - g_consumed_event_count;\
    /* Print error message if there were unexpected events.*/\
    if (0 != unexpected_count) {\
        fprintf(stderr,"Error (%s -> %s -> %i): %li unexpected events :\n", __FILENAME__, __func__, __LINE__, unexpected_count);\
        errors_happened = true;\
    }\
    /* Print all unexpected events and release them.*/\
    for (size_t cursor = 0; cursor < g_emitted_event_count; ++cursor) {\
        event_t *const event = emitted_events + cursor;\
        if (0 == event->consumed) {\
            ubiquity_test_print_event(__FILENAME__, __func__, __LINE__, event);\
        }\
        ubiquity_test_release_event(__FILENAME__, __func__, __LINE__, event);\
    }\
    /* Print error message if there were unemitted (but expected) events.*/\
    if (0 != g_expected_event_count) {\
        fprintf(stderr,"Error (%s -> %s -> %i): %li missing events :\n", __FILENAME__, __func__, __LINE__, g_expected_event_count);\
        /* Print all unemitted events and release them.*/\
        for (size_t cursor = 0; cursor < g_expected_event_count; ++cursor) {\
            event_t *const event = expected_events + cursor;\
            ubiquity_test_print_event(__FILENAME__, __func__, __LINE__, event);\
            ubiquity_test_release_event(__FILENAME__, __func__, __LINE__, event);\
        }\
        errors_happened = true;\
    }\
    /* Reset all counters.*/\
    g_emitted_event_count = 0;\
    g_consumed_event_count = 0;\
    g_expected_event_count = 0;\
    /* If emitted and expected events did not match, leave containing function call with error.*/\
    if (errors_happened) {\
        return 1;\
    }\
} while(false);

/*
 * Test declaration and processing.
 */

typedef int (*ubiquity_test_function_t)(void);

int ubiquity_test_all(
        const ubiquity_test_function_t *const ubiquity_test_function_list
        )
{
    for (size_t cursor = 0; ubiquity_test_function_list[cursor]; ++cursor) {
        fprintf(stderr," => Starting test function %li.\n", cursor);
        const ubiquity_test_function_t function = ubiquity_test_function_list[cursor];
        if (function()) {
            return 1;
        }
    }
    return 0;
}

