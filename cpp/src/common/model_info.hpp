#pragma once

#include "common.hpp"
#include <xxh3.h>
#include <memory>

/*
    Feature type list.
*/

#define FEATURE_TYPE_NUMERIC 0
#define FEATURE_TYPE_NOMINAL 2
#define FEATURE_TYPE_BOOLEAN 3
#define FEATURE_TYPE_CATEGORICAL 4
#define FEATURE_TYPE_NONE 0xFF


typedef uint64_t feature_type_t;

#define FEATURE_TYPE_BIT_WIDTH 4
#define FEATURE_TYPE_MAX (1ull << FEATURE_TYPE_BIT_WIDTH)
#define FEATURE_TYPE_MASK (FEATURE_TYPE_MAX - 1)

#define FEATURE_LIST_CHUNK_BIT_WIDTH 64
#define FEATURE_LIST_CHUNK_BYTE_WIDTH (FEATURE_LIST_CHUNK_BIT_WIDTH / 8)
typedef uint64_t feature_list_chunk_t;

#define FEATURE_LIST_ELEMENT_COUNT 0x100
#define FEATURE_LIST_CHUNK_SIZE (FEATURE_LIST_CHUNK_BIT_WIDTH / FEATURE_TYPE_BIT_WIDTH)
#define FEATURE_TYPE_CHUNK_MASK (FEATURE_LIST_CHUNK_SIZE - 1)
#define FEATURE_LIST_CHUNK_COUNT (FEATURE_LIST_ELEMENT_COUNT / FEATURE_LIST_CHUNK_SIZE)

typedef struct feature_type_list_s {
    feature_list_chunk_t chunk_list[FEATURE_LIST_CHUNK_COUNT];
} feature_type_list_t;

static inline void feature_type_list_reset(
        feature_type_list_t *const feature_type_list
        )
{
    memset(
            feature_type_list,
            0,
            sizeof(*feature_type_list)
            );
}

static inline void feature_type_list_set_element(
        feature_type_list_t *const feature_type_list,
        const size_t feature_index,
        const feature_type_t feature_type
        )
{
    const size_t chunk_index = feature_index / FEATURE_LIST_CHUNK_SIZE;
    const size_t decal = (FEATURE_TYPE_CHUNK_MASK & (feature_index ^ 1)) * FEATURE_TYPE_BIT_WIDTH;
    feature_type_list->chunk_list[chunk_index] |= ((uint64_t)feature_type << decal);
}

static inline feature_type_t feature_type_list_get_element(
        const feature_type_list_t *const feature_type_list,
        const size_t feature_index
        )
{
    const size_t chunk_index = feature_index / FEATURE_LIST_CHUNK_SIZE;
    const size_t decal = (FEATURE_TYPE_CHUNK_MASK & feature_index) * FEATURE_TYPE_BIT_WIDTH;
    return FEATURE_TYPE_MASK & (feature_type_list->chunk_list[chunk_index] >> decal);
}

/*
    Feedback delay list.
*/

#define MAXIMUM_FEEDBACK_LEVEL 8

typedef uint32_t feedback_delay_t;
typedef struct feedback_delay_list_s {
    feedback_delay_t elements[MAXIMUM_FEEDBACK_LEVEL];
} feedback_delay_list_t;

static inline void feedback_delay_list_reset(
        feedback_delay_list_t *const feedback_delay_list
        )
{
    memset(
            feedback_delay_list,
            0,
            sizeof(*feedback_delay_list)
            );
}

/*
    Feedback id list.
*/

typedef uint64_t feedback_id_t;

typedef struct feedback_id_list_s {
    feedback_id_t elements[MAXIMUM_FEEDBACK_LEVEL];
} feedback_id_list_t;

static inline void feedback_id_list_reset(
        feedback_id_list_t *const feedback_id_list
        )
{
    memset(
            feedback_id_list,
            0,
            sizeof(*feedback_id_list)
            );
}

/*
    Model info.
*/

#define UNDEFINED_APPLICATION_ID 9999

typedef uint64_t model_id_t;
typedef int16_t application_id_t;

typedef struct model_info_s {
    model_id_t model_id;
    application_id_t application_id;
    size_t feedback_count;
    size_t feature_type_count;
    feedback_id_list_t feedback_id_list;
    feedback_delay_list_t feedback_delay_list;
    feature_type_list_t feature_type_list;
    ip_set_t compatible_node_set;
} model_info_t;

static inline model_id_t model_info_get_model_id(
        const model_info_t *const model_info
        )
{
    return model_info->model_id;
}

static inline void model_info_set_model_id(
        model_info_t *const model_info,
        const model_id_t model_id
        )
{
    model_info->model_id = model_id;
}

static inline void model_info_set_application_id(
        model_info_t *const model_info,
        const application_id_t application_id
        )
{
    model_info->application_id = application_id;
}

static inline void model_info_unset_application_id(
        model_info_t *const model_info
        )
{
    model_info->application_id = UNDEFINED_APPLICATION_ID;
}

static inline application_id_t model_info_get_application_id(
        const model_info_t *const model_info
        )
{
    return model_info->application_id;
}

static inline size_t model_info_get_feedback_count(
        const model_info_t *const model_info
        )
{
    return model_info->feedback_count;
}

static inline size_t model_info_get_feature_type_count(
        const model_info_t *const model_info
        )
{
    return model_info->feature_type_count;
}

static inline feedback_id_t model_info_get_feedback_id(
        const model_info_t *const model_info,
        const size_t index
        )
{
    return model_info->feedback_id_list.elements[index];
}

static inline size_t model_info_find_feedback_level(
        const model_info_t *const model_info,
        const char* const feedback_name
        )
{
    const feedback_id_t feedback_id = XXH3_64bits(
            feedback_name,
            strlen(feedback_name)
            );
    size_t feedback_level = 0;
    while (feedback_level < MAXIMUM_FEEDBACK_LEVEL && feedback_id != model_info_get_feedback_id(
                model_info,
                feedback_level
                )) {
        ++feedback_level;
    }
    return feedback_level;
}

static inline void model_info_clear_feedback_list(
        model_info_t *const model_info
        )
{
    model_info->feedback_count = 0;
}

static inline void model_info_add_feedback(
        model_info_t *const model_info,
        const feedback_id_t feedback_id,
        const feedback_delay_t feedback_delay
        )
{
    const size_t feedback_count = model_info->feedback_count;
    model_info->feedback_delay_list.elements[feedback_count] = feedback_delay;
    model_info->feedback_id_list.elements[feedback_count] = feedback_id;
    model_info->feedback_count = feedback_count + 1;
}

static inline int64_t model_info_get_feedback_delay(
        const model_info_t *const model_info,
        const size_t index
        )
{
    return model_info->feedback_delay_list.elements[index];
}

static inline void model_info_set_feedback_list(
        model_info_t *const model_info,
        const feedback_id_list_t *const feedback_id_list,
        const feedback_delay_list_t *const feedback_delay_list,
        const size_t feedback_count
        )
{
    for (size_t feedback_index = 0; feedback_index < MAXIMUM_FEEDBACK_LEVEL; ++feedback_index) {
        model_info->feedback_id_list.elements[feedback_index] = feedback_id_list->elements[feedback_index];
        model_info->feedback_delay_list.elements[feedback_index] = feedback_delay_list->elements[feedback_index];
    }
    model_info->feedback_count = feedback_count;
}

static inline bool model_info_feature_type_lists_are_identical(
        const model_info_t *const model_info,
        const feature_type_list_t *const feature_type_list
        )
{
    for (size_t index = 0; index < FEATURE_LIST_CHUNK_COUNT; ++index) {
        if (model_info->feature_type_list.chunk_list[index] != feature_type_list->chunk_list[index]) {
            return false;
        }
    }
    return true;
}

static inline void model_info_set_feature_type_list(
        model_info_t *const model_info,
        const feature_type_list_t *const feature_type_list,
        const size_t feature_type_count
        )
{
    memcpy(
            &model_info->feature_type_list,
            feature_type_list,
            sizeof(*feature_type_list)
            );
    model_info->feature_type_count = feature_type_count;
}

static inline void model_info_add_sharding(
        model_info_t *const model_info,
        const uint32_t ip
        )
{
    model_info->compatible_node_set.insert(ip);
}

static inline void model_info_remove_sharding(
        model_info_t *const model_info,
        const uint32_t ip
        )
{
    model_info->compatible_node_set.erase(ip);
}

static inline bool model_info_has_metadata(
        const model_info_t *const model_info
        )
{
    return UNDEFINED_APPLICATION_ID != model_info->application_id;
}

static inline bool model_info_has_sharding(
        const model_info_t *const model_info
        )
{
    return 0 != model_info->compatible_node_set.size();
}

static inline bool model_info_has_sharding_for_ip(
        const model_info_t *const model_info,
        const uint32_t ip
        )
{
    return (model_info->compatible_node_set.find(ip) != model_info->compatible_node_set.end());
}

static inline bool model_info_is_empty(
        const model_info_t *const model_info
        )
{
    return !model_info_has_metadata(model_info)
        && !model_info_has_sharding(model_info);
}

static inline void model_info_initialize(
        model_info_t *const model_info,
        const model_id_t model_id
        )
{
    model_info_unset_application_id(model_info);
    model_info_set_model_id(
            model_info,
            model_id
            );
}


static inline feature_type_t model_info_feature_type_list_get_element(
        const model_info_t *const model_info,
        const size_t feature_index
        )
{
    const size_t chunk_index = feature_index / FEATURE_LIST_CHUNK_SIZE;
    const size_t decal = (FEATURE_TYPE_CHUNK_MASK & feature_index) * FEATURE_TYPE_BIT_WIDTH;
    return FEATURE_TYPE_MASK & (model_info->feature_type_list.chunk_list[chunk_index] >> decal);
}


static inline void model_info_copy(
        model_info_t *dst,
        const model_info_t *const src
        ) 
{
    model_info_initialize(dst, src->model_id);
    model_info_set_application_id(dst, src->application_id);
    model_info_set_feedback_list(dst, &src->feedback_id_list, &src->feedback_delay_list, src->feedback_count);
    model_info_set_feature_type_list(dst, &src->feature_type_list, src->feature_type_count);
    dst->compatible_node_set.clear();
    dst->compatible_node_set = src->compatible_node_set;
}

typedef std::shared_ptr<model_info_t> model_info_ref_t;

inline model_info_ref_t model_info_allocate() noexcept 
{
    return std::make_shared<model_info_t>();  // ← constructor + destructor handled correctly
}