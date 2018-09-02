#ifndef LEPTJSON_H__
#define LEPTJSON_H__

#include <stddef.h>

typedef enum { MYJSON_NULL, MYJSON_FALSE, MYJSON_TRUE, MYJSON_NUMBER, MYJSON_STRING, MYJSON_ARRAY, MYJSON_OBJECT } myjson_type;

#define MYJSON_KEY_NOT_EXIST ((size_t)-1)

typedef struct myjson_value myjson_value;
typedef struct myjson_member myjson_member;

struct myjson_value {
    // double n;
    union {
        struct { myjson_member *m; size_t size, capacity; } obj;
        struct { myjson_value *e; size_t size, capacity; } arr;
        struct { char *s; size_t len; } s;
        double n;
    } val;
    myjson_type type;
};

struct myjson_member {
    char *key;
    size_t klen;
    myjson_value v;
};

enum {
    MYJSON_PARSE_OK = 0,
    MYJSON_PARSE_EXPECT_VALUE,
    MYJSON_PARSE_INVALID_VALUE,
    MYJSON_PARSE_ROOT_NOT_SINGULAR,
    MYJSON_PARSE_NUMBER_TOO_BIG,
    MYJSON_PARSE_MISS_QUOTATION_MARK,
    MYJSON_PARSE_INVALID_STRING_ESCAPE,
    MYJSON_PARSE_INVALID_STRING_CHAR,
    MYJSON_PARSE_INVALID_UNICODE_HEX,
    MYJSON_PARSE_INVALID_UNICODE_SURROGATE,
    MYJSON_PARSE_MISS_COMMA_OR_SQUARE_BRACKET,
    MYJSON_PARSE_MISS_KEY,
    MYJSON_PARSE_MISS_COLON,
    MYJSON_PARSE_MISS_COMMA_OR_CURLY_BRACKET
};

#define myjson_init(v) do { (v)->type = MYJSON_NULL; } while(0)

int myjson_parse(myjson_value *v, const char *json);
char *myjson_stringify(const myjson_value *v, size_t *length);

void myjson_copy(myjson_value* dst, const myjson_value* src);
void myjson_move(myjson_value* dst, myjson_value* src);
void myjson_swap(myjson_value* lhs, myjson_value* rhs);

void myjson_free(myjson_value *v);

myjson_type myjson_get_type(const myjson_value *v);
int myjson_is_equal(const myjson_value* lhs, const myjson_value* rhs);

#define myjson_set_null(v) myjson_free(v)

int myjson_get_boolean(const myjson_value* v);
void myjson_set_boolean(myjson_value* v, int b);

double myjson_get_number(const myjson_value *v);
void myjson_set_number(myjson_value *v, double n);

const char* myjson_get_string(const myjson_value* v);
size_t myjson_get_string_length(const myjson_value* v);
void myjson_set_string(myjson_value* v, const char* s, size_t len);

void myjson_set_array(myjson_value* v, size_t capacity);
size_t myjson_get_array_size(const myjson_value* v);
size_t myjson_get_array_capacity(const myjson_value* v);
void myjson_reserve_array(myjson_value* v, size_t capacity);
void myjson_shrink_array(myjson_value* v);
void myjson_clear_array(myjson_value* v);
myjson_value *myjson_get_array_element(myjson_value *v, size_t index);
myjson_value* myjson_pushback_array_element(myjson_value* v);
void myjson_popback_array_element(myjson_value* v);
myjson_value* myjson_insert_array_element(myjson_value* v, size_t index);
void myjson_erase_array_element(myjson_value* v, size_t index, size_t count);

void myjson_set_object(myjson_value* v, size_t capacity);
size_t myjson_get_object_size(const myjson_value *v);
size_t myjson_get_object_capacity(const myjson_value* v);
void myjson_reserve_object(myjson_value* v, size_t capacity);
void myjson_shrink_object(myjson_value* v);
void myjson_clear_object(myjson_value* v);
const char *myjson_get_object_key(const myjson_value *v, size_t index);
size_t myjson_get_object_key_length(const myjson_value *v, size_t index);
myjson_value *myjson_get_object_value(const myjson_value *v, size_t index);
size_t myjson_find_object_index(const myjson_value* v, const char* key, size_t klen);
myjson_value* myjson_find_object_value(myjson_value* v, const char* key, size_t klen);
myjson_value* myjson_set_object_value(myjson_value* v, const char* key, size_t klen);
void myjson_remove_object_value(myjson_value* v, size_t index);

#endif