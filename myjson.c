#include "myjson.h"
#include <assert.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <malloc.h>
#include <string.h>

#ifndef MYJSON_PARSR_STACK_INIT_SIZE
#define MYJSON_PARSR_STACK_INIT_SIZE 256
#endif

#ifndef MYJSON_PARSE_STRINGIFY_INIT_SIZE
#define MYJSON_PARSE_STRINGIFY_INIT_SIZE 256
#endif

#define EXPECT(c, ch) do { assert(*c->json == (ch)); c->json++; } while(0)
#define ISDIGITAL(ch) ((ch) >= '0' && (ch) <= '9')
#define ISDIGITAL1TO9(ch) ((ch) >= '1' && (ch) <= '9')
#define PUTC(c, ch) do { *(char*)myjson_context_push(c, sizeof(char)) = (ch); } while(0)
#define PUTS(c, s, len) memcpy(myjson_context_push(c, len), s, len)

#define STRING_ERROR(ret) do { c->top = head; return ret; } while(0)

typedef struct {
    const char *json;
    char *stack;
    size_t size, top;
} myjson_context;

static void *myjson_context_push(myjson_context *c, size_t size) {
    void *ret;
    assert(size > 0);
    if (c->top + size >= c->size) {
        if (c->size == 0)
            c->size = MYJSON_PARSR_STACK_INIT_SIZE;
        while(c->top + size >= c->size)
            c->size += c->size >> 1;
        c->stack = (char *)realloc(c->stack, c->size);
    }
    ret = c->stack + c->top;
    c->top += size;
    return ret;
}

static void* myjson_context_pop(myjson_context *c, size_t size) {
    assert(c->top >= size);
    return c->stack + (c->top -= size);
}

static void myjson_parse_whitespace(myjson_context *c) {
    const char *p = c->json;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    c->json = p;
}

static int myjson_parse_true(myjson_context *c, myjson_value *v) {
    EXPECT(c, 't');
    if (c->json[0] != 'r' || c->json[1] != 'u' || c->json[2] != 'e')
        return MYJSON_PARSE_INVALID_VALUE;
    c->json += 3;
    v->type = MYJSON_TRUE;
    return MYJSON_PARSE_OK;
}

static int myjson_parse_false(myjson_context *c, myjson_value *v) {
    EXPECT(c, 'f');
    if (c->json[0] != 'a' || c->json[1] != 'l' || c->json[2] != 's' || c->json[3] != 'e')
        return MYJSON_PARSE_INVALID_VALUE;
    c->json += 4;
    v->type = MYJSON_FALSE;
    return MYJSON_PARSE_OK;
}

static int myjson_parse_null(myjson_context *c, myjson_value *v) {
    EXPECT(c, 'n');
    if (c->json[0] != 'u' || c->json[1] == 'l' || c->json[2] != 'l')
        return MYJSON_PARSE_INVALID_VALUE;
    c->json += 3;
    v->type = MYJSON_NULL;
    return MYJSON_PARSE_OK;
}

static int myjson_parse_literal(myjson_context *c, myjson_value *v, const char *literal, myjson_type type) {
    size_t i;
    EXPECT(c,  literal[0]);
    for (i = 0; literal[i + 1]; i++)
        if (c->json[i] != literal[i + 1])
            return MYJSON_PARSE_INVALID_VALUE;
    c->json += i;
    v->type = type;
    return MYJSON_PARSE_OK;
}

static int myjson_parse_number(myjson_context *c, myjson_value *v) {
    const char *p = c->json;
    if (*p == '-') p++;
    if (*p == '0') p++;
    else {
        if (!ISDIGITAL1TO9(*p)) return MYJSON_PARSE_INVALID_VALUE;
        p++;
        while(ISDIGITAL1TO9(*p))
            ++p;
    }
    if (*p == '.') {
        p++;
        if (!ISDIGITAL(*p)) return MYJSON_PARSE_INVALID_VALUE;
        p++;
        while(ISDIGITAL(*p))
            ++p;
    }
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '-' || *p == '+') p++;
        if (!ISDIGITAL(*p)) return MYJSON_PARSE_INVALID_VALUE;
        p++;
        while(ISDIGITAL(*p))
            ++p;
    }

    errno = 0;
    v->val.n = strtod(c->json, NULL);

    if (errno == ERANGE && (v->val.n == HUGE_VAL || v->val.n == -HUGE_VAL))
        return MYJSON_PARSE_NUMBER_TOO_BIG;
    c->json = p;
    v->type = MYJSON_NUMBER;
    return MYJSON_PARSE_OK;
}

static const char *myjson_parse_hex4(const char *p, unsigned *u) {
    int i;
    *u = 0;
    for (i = 0; i < 4; i++) {
        char ch = *p++;
        *u <<= 4;
        if (ch >= '0' && ch <= '9') *u |= ch - '0';
        else if (ch >= 'A' && ch <= 'F') *u |= ch - ('A' - 10);
        else if (ch >= 'a' && ch <= 'f') *u |= ch - ('a' - 10);
        else return NULL;
    }
    return p;
}

static void myjson_encode_utf8(myjson_context *c, unsigned u) {
    if (u <= 0x7F)
        PUTC(c, u & 0xFF);
    else if (u <= 0x7FF) {
        PUTC(c, 0xC0 | ((u >> 6) & 0xFF));
        PUTC(c, 0x80 | (u & 0x3F));
    }
    else if (u <= 0xFFFF) {
        PUTC(c, 0xE0 | ((u >> 12) & 0xFF));
        PUTC(c, 0x80 | ((u >> 6) & 0x3F));
        PUTC(c, 0x80 | (u & 0x3F));
    }
    else {
        assert(u <= 0x10FFFF);
        PUTC(c, 0xF0 | ((u >> 18) & 0xFF));
        PUTC(c, 0x80 | ((u >> 12) & 0x3F));
        PUTC(c, 0x80 | ((u >> 6) & 0x3F));
        PUTC(c, 0x80 | (u & 0x3F));
    }
}

static int myjson_parse_string_raw(myjson_context *c, char **str, size_t *len) {
    size_t head = c->top;
    unsigned u, u2;
    const char *p;
    EXPECT(c, '\"');
    p = c->json;
    for(;;) {
        char ch = *p++;
        switch (ch) {
            case '\"':
                *len = c->top - head;
                *str = myjson_context_pop(c, *len);
                c->json = p;
                return MYJSON_PARSE_OK;
            case '\\':
                switch (*p++) {
                    case '\"': PUTC(c, '\"'); break;
                    case '\\': PUTC(c, '\\'); break;
                    case '/':  PUTC(c, '/' ); break;
                    case 'b':  PUTC(c, '\b'); break;
                    case 'f':  PUTC(c, '\f'); break;
                    case 'n':  PUTC(c, '\n'); break;
                    case 'r':  PUTC(c, '\r'); break;
                    case 't':  PUTC(c, '\t'); break;
                    case 'u':
                        if (!(p = myjson_parse_hex4(p, &u)))
                            STRING_ERROR(MYJSON_PARSE_INVALID_UNICODE_HEX);
                        if (u >= 0xD800 && u <= 0xDBFF) { /* surrogate pair */
                            if (*p++ != '\\')
                                STRING_ERROR(MYJSON_PARSE_INVALID_UNICODE_SURROGATE);
                            if (*p++ != 'u')
                                STRING_ERROR(MYJSON_PARSE_INVALID_UNICODE_SURROGATE);
                            if (!(p = myjson_parse_hex4(p, &u2)))
                                STRING_ERROR(MYJSON_PARSE_INVALID_UNICODE_HEX);
                            if (u2 < 0xDC00 || u2 > 0xDFFF)
                                STRING_ERROR(MYJSON_PARSE_INVALID_UNICODE_SURROGATE);
                            u = (((u - 0xD800) << 10) | (u2 - 0xDC00)) + 0x10000;
                        }
                        myjson_encode_utf8(c, u);
                        break;
                    default:
                        STRING_ERROR(MYJSON_PARSE_INVALID_STRING_ESCAPE);
                }
                break;
            case '\0':
                STRING_ERROR(MYJSON_PARSE_MISS_QUOTATION_MARK);
            default:
                if ((unsigned char)ch < 0x20)
                    STRING_ERROR(MYJSON_PARSE_INVALID_STRING_CHAR);
                PUTC(c, ch);
        }
    }
}

static int myjson_parse_string(myjson_context* c, myjson_value* v) {
    int ret;
    char *s;
    size_t len;
    if ((ret = myjson_parse_string_raw(c, &s, &len)) == MYJSON_PARSE_OK)
        myjson_set_string(v, s, len);
    return ret;
}

static int myjson_parse_value(myjson_context *c, myjson_value *v);

static int myjson_parse_array(myjson_context *c, myjson_value *v) {
    size_t i, size = 0;
    int ret;
    EXPECT(c, '[');
    myjson_parse_whitespace(c);
    if (*c->json == ']') {
        c->json++;
        v->type = MYJSON_ARRAY;
        v->val.arr.size = 0;
        v->val.arr.e = NULL;
        return MYJSON_PARSE_OK;
    }

    for (;;) {
        myjson_value e;
        myjson_init(&e);
        if ((ret = myjson_parse_value(c, &e)) != MYJSON_PARSE_OK)
            break;
        memcpy(myjson_context_push(c, sizeof(myjson_value)), &e, sizeof(myjson_value));
        size++;
        myjson_parse_whitespace(c);
        if (*c->json == ',') {
            c->json++;
            myjson_parse_whitespace(c);
        }
        else if (*c->json == ']') {
            c->json++;
            v->type = MYJSON_ARRAY;
            v->val.arr.size = size;
            size *= sizeof(myjson_value);
            memcpy(v->val.arr.e = (myjson_value*)malloc(size), myjson_context_pop(c, size), size);
            return MYJSON_PARSE_OK;
        }
        else {
            ret = MYJSON_PARSE_MISS_COMMA_OR_SQUARE_BRACKET;
            break;
        }
    }

    for (i = 0; i < size; i++) {
        myjson_free((myjson_value*)myjson_context_pop(c, sizeof(myjson_value)));
    }
    return ret;
}

static int myjson_parse_object(myjson_context *c, myjson_value *v) {
    size_t i, size;
    myjson_member m;
    int ret;
    EXPECT(c, '{');
    myjson_parse_whitespace(c);
    if (*c->json == '}') {
        c->json++;
        v->type = MYJSON_OBJECT;
        v->val.obj.m = 0;
        v->val.obj.size = 0;
        return MYJSON_PARSE_OK;
    }
    m.key = NULL;
    size = 0;
    for(;;) {
        char *str;
        myjson_init(&m.v);
        // parse key
        if (*c->json != '"') {
            ret = MYJSON_PARSE_MISS_KEY;
            break;
        }
        if ((ret = myjson_parse_string_raw(c, &str, &m.klen)) != MYJSON_PARSE_OK)
            break;
        memcpy(m.key = (char *)malloc(m.klen + 1), str, m.klen);
        m.key[m.klen] = '\0';
        // parse colon
        myjson_parse_whitespace(c);
        if (*c->json != ':') {
            ret = MYJSON_PARSE_MISS_COLON;
            break;
        }
        c->json++;
        myjson_parse_whitespace(c);
        // parse value
        if ((ret = myjson_parse_value(c, &m.v)) != MYJSON_PARSE_OK)
            break;
        memcpy(myjson_context_push(c, sizeof(myjson_member)), &m, sizeof(myjson_member));
        size++;
        m.key = NULL;

        myjson_parse_whitespace(c);
        if (*c->json == ',') {
            c->json++;
            myjson_parse_whitespace(c);
        }
        else if (*c->json == '}') {
            size_t s = sizeof(myjson_member) * size;
            c->json++;
            v->type = MYJSON_OBJECT;
            v->val.obj.size = size;
            memcpy(v->val.obj.m = (myjson_member *)malloc(s), myjson_context_pop(c, s), s);
            return MYJSON_PARSE_OK;
        }
        else {
            ret = MYJSON_PARSE_MISS_COMMA_OR_CURLY_BRACKET;
            break;
        }
    }
    // pop and free stack
    free(m.key);
    for (i = 0; i < size; i++) {
        myjson_member *m = (myjson_member *)myjson_context_pop(c, sizeof(myjson_member));
        free(m->key);
        myjson_free(&m->v);
    }
    v->type = MYJSON_NULL;
    return ret;
}

static int myjson_parse_value(myjson_context *c, myjson_value *v) { 
    switch(*c->json) {
        // case 't': return myjson_parse_true(c, v);
        // case 'f': return myjson_parse_false(c, v);
        // case 'n': return myjson_parse_null(c, v);
        case 't': return myjson_parse_literal(c, v, "true", MYJSON_TRUE);
        case 'f': return myjson_parse_literal(c, v, "false", MYJSON_FALSE);
        case 'n': return myjson_parse_literal(c, v, "null", MYJSON_NULL);
        default: return myjson_parse_number(c, v);
        case '"': return myjson_parse_string(c, v);
        case '[': return myjson_parse_array(c, v);
        case '{': return myjson_parse_object(c, v);
        case '\0': return MYJSON_PARSE_EXPECT_VALUE;
    }
}

int myjson_parse(myjson_value *v, const char *json) {
    myjson_context c;
    int ret;
    assert(v != NULL);
    c.json = json;
    c.stack = NULL;
    c.size = c.top = 0;
    myjson_init(v);
    myjson_parse_whitespace(&c);
    if ((ret = myjson_parse_value(&c, v)) == MYJSON_PARSE_OK) {
        myjson_parse_whitespace(&c);
        if (*c.json != '\0') {
            v->type = MYJSON_NULL;
            ret = MYJSON_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    assert(c.top == 0);
    free(c.stack);
    return ret;
}

static void myjson_stringify_string(myjson_context *c, const char *s, size_t len) {
    static const char hex_digits[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
    size_t i, size;
    char *head, *p;
    assert(s != NULL);
    p = head = myjson_context_push(c, size = len * 6 + 2);
    *p++ = '"';
    for (i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)s[i];
        switch (ch) {
            case '\"': *p++ = '\\'; *p++ = '\"'; break;
            case '\\': *p++ = '\\'; *p++ = '\\'; break;
            case '\b': *p++ = '\\'; *p++ = 'b'; break;
            case '\f': *p++ = '\\'; *p++ = 'f'; break;
            case '\n': *p++ = '\\'; *p++ = 'n'; break;
            case '\r': *p++ = '\\'; *p++ = 'r'; break;
            case '\t': *p++ = '\\'; *p++ = 't'; break;
            default:
                if (ch < 0x20) {
                    *p++ = '\\';
                    *p++ = 'u';
                    *p++ = '0';
                    *p++ = '0';
                    *p++ = hex_digits[ch >> 4];
                    *p++ = hex_digits[ch & 15];
                }
                else {
                    *p++ = s[i];
                }
        }
    }
    *p++ = '"';
    c->top -= size - (p - head);
}

static void myjson_stringify_value(myjson_context *c, const myjson_value *v) {
    size_t i;
    switch (v->type) {
        case MYJSON_NULL: PUTS(c, "null", 4); break;
        case MYJSON_FALSE: PUTS(c, "false", 5); break;
        case MYJSON_TRUE: PUTS(c, "true", 4); break;
        case MYJSON_NUMBER: c->top -= 32 - sprintf(myjson_context_push(c, 32), "%.17g", v->val.n); break;
        case MYJSON_STRING: myjson_stringify_string(c, v->val.s.s, v->val.s.len); break;
        case MYJSON_ARRAY:
            PUTC(c, '[');
            for (i = 0; i < v->val.arr.size; i++) {
                if (i > 0)
                    PUTC(c, ',');
                myjson_stringify_value(c, &v->val.arr.e[i]);
            }
            PUTC(c, ']');
            break;
        case MYJSON_OBJECT:
            PUTC(c, '{');
            for (i = 0; i < v->val.obj.size; i++) {
                if (i > 0)
                    PUTC(c, ',');
                myjson_stringify_string(c, v->val.obj.m[i].key, v->val.obj.m[i].klen);
                PUTC(c, ':');
                myjson_stringify_value(c, &v->val.obj.m[i].v);
            }
            PUTC(c, '}');
            break;
        default: assert(0 && "invalid type");
    }
}

char *myjson_stringify(const myjson_value *v, size_t *length) {
    myjson_context c;
    assert(v != NULL);
    c.stack = (char *)malloc(c.size = MYJSON_PARSE_STRINGIFY_INIT_SIZE);
    c.top = 0;
    myjson_stringify_value(&c, v);
    if (length)
       *length = c.top;
    PUTC(&c, '\0');
    return c.stack; 
}

void myjson_copy(myjson_value* dst, const myjson_value* src) {
    assert(src != NULL &&dst != NULL && src != NULL);
    switch(src->type) {
        case MYJSON_STRING:
            myjson_set_string(dst, src->val.s.s, src->val.s.len);
            break;
        case MYJSON_ARRAY:
            // incomplete
            break;
        case MYJSON_OBJECT:
            // incomplete
            break;
        default:
            myjson_free(dst);
            memcpy(dst, src, sizeof(myjson_value));
            break;
    }
}

void myjson_move(myjson_value* dst, myjson_value* src) {
    assert(dst != NULL && src != NULL);
    myjson_free(dst);
    memcpy(dst, src, sizeof(myjson_value));
    myjson_free(src);
    myjson_init(src);
}

void myjson_swap(myjson_value* lhs, myjson_value* rhs) {
    assert(lhs != NULL && rhs != NULL);
    if (lhs != rhs) {
        myjson_value temp;
        memcpy(&temp, lhs, sizeof(myjson_value));
        memcpy(lhs, rhs, sizeof(myjson_value));
        memcpy(rhs, &temp, sizeof(myjson_value));
    }
}

void myjson_free(myjson_value *v) {
    size_t i;
    assert( v != NULL);
    switch (v->type) {
        case MYJSON_STRING:
            free(v->val.s.s);
            break;
        case MYJSON_ARRAY:
            for (i = 0; i < v->val.arr.size; i++)
                myjson_free(&v->val.arr.e[i]);
            free(v->val.arr.e);
            break;
        case MYJSON_OBJECT:
            for (i = 0; i < v->val.obj.size; i++) {
                free(v->val.obj.m[i].key);
                myjson_free(&v->val.obj.m[i].v);
            }
            free(v->val.obj.m);
            break;
        default: break;
    }
    v->type = MYJSON_NULL;
}

myjson_type myjson_get_type(const myjson_value *v) {
    assert(v != NULL);
    return v->type;
}

int myjson_is_equal(const myjson_value* lhs, const myjson_value* rhs) {
    size_t i;
    assert(lhs != NULL && rhs != NULL);
    if (lhs->type != rhs->type)
        return 0;
    switch (lhs->type) {
        case MYJSON_STRING:
            return lhs->val.s.len == rhs->val.s.len && memcmp(lhs->val.s.s, rhs->val.s.s, lhs->val.s.len);
        case MYJSON_NUMBER:
            return lhs->val.n == rhs->val.n;
        case MYJSON_ARRAY:
            if (lhs->val.arr.size != rhs->val.arr.size)
                return 0;
            for (i = 0; i < lhs->val.arr.size; i++)
                if (!myjson_is_equal(&lhs->val.arr.e[i], &rhs->val.arr.e[i])) 
                    return 0;
            return 1;
        case MYJSON_OBJECT:
            // incomplete
            return 1;
        default:
            return 1;
    }
}

int myjson_get_boolean(const myjson_value* v) {
    assert(v != NULL && (v->type == MYJSON_TRUE || v->type == MYJSON_FALSE));
    return v->type == MYJSON_TRUE;
}

void myjson_set_boolean(myjson_value* v, int b) {
    myjson_free(v);
    v->type = b ? MYJSON_TRUE : MYJSON_FALSE;
}

double myjson_get_number(const myjson_value *v) {
    assert(v != NULL && v->type == MYJSON_NUMBER);
    return v->val.n;
}

void myjson_set_number(myjson_value *v, double n) {
    myjson_free(v);
    v->val.n = n;
    v->type = MYJSON_NUMBER;
}

const char* myjson_get_string(const myjson_value* v) {
    assert(v != NULL && v->type == MYJSON_STRING);
    return v->val.s.s;
}

size_t myjson_get_string_length(const myjson_value* v) {
    assert(v != NULL && v->type == MYJSON_STRING);
    return v->val.s.len;
}

void myjson_set_string(myjson_value* v, const char* s, size_t len) {
    assert(v != NULL && (s != NULL || len == 0));
    myjson_free(v);
    v->val.s.s = (char *)malloc(len + 1);
    memcpy(v->val.s.s, s, len);
    v->val.s.s[len] = '\0';
    v->val.s.len = len;
    v->type = MYJSON_STRING;
}

size_t myjson_get_array_size(const myjson_value *v) {
    assert(v != NULL && v->type == MYJSON_ARRAY);
    return v->val.arr.size;
}

void myjson_set_array(myjson_value *v, size_t capacity) {
    assert(v != NULL);
    myjson_free(v);
    v->type = MYJSON_ARRAY;
    v->val.arr.size = 0;
    v->val.arr.capacity = capacity;
    v->val.arr.e = capacity > 0 ? (myjson_value*)malloc(capacity * sizeof(myjson_value)) : NULL;
}

size_t myjson_get_array_capacity(const myjson_value* v) {
    assert(v != NULL && v->type == MYJSON_ARRAY);
    return v->val.arr.capacity;
}

void myjson_reserve_array(myjson_value* v, size_t capacity) {
    assert(v != NULL && v->type == MYJSON_ARRAY);
    if (v->val.arr.capacity < capacity) {
        v->val.arr.capacity = capacity;
        v->val.arr.e = (myjson_value*)realloc(v->val.arr.e, v->val.arr.capacity * sizeof(myjson_value));
    }
}

void myjson_shrink_array(myjson_value* v) {
    assert(v != NULL && v->type == MYJSON_ARRAY);
    if (v->val.arr.capacity > v->val.arr.size) {
        v->val.arr.capacity = v->val.arr.size;
        v->val.arr.e = (myjson_value*)realloc(v->val.arr.e, v->val.arr.capacity * sizeof(myjson_value));
    }
}

void myjson_clear_array(myjson_value* v) {
    assert(v != NULL && v->type == MYJSON_ARRAY);
    myjson_erase_array_element(v, 0, v->val.arr.size);
}

myjson_value* myjson_get_array_element(myjson_value* v, size_t index) {
    assert(v != NULL && v->type == MYJSON_ARRAY);
    assert(index < v->val.arr.size);
    return &v->val.arr.e[index];
}

myjson_value* myjson_pushback_array_element(myjson_value* v) {
    assert(v != NULL && v->type == MYJSON_ARRAY);
    if (v->val.arr.size == v->val.arr.capacity)
        myjson_reserve_array(v, v->val.arr.capacity == 0 ? 1 : v->val.arr.capacity * 2);
    myjson_init(&v->val.arr.e[v->val.arr.size]);
    return &v->val.arr.e[v->val.arr.size++];
}

void myjson_popback_array_element(myjson_value* v) {
    assert(v != NULL && v->type == MYJSON_ARRAY && v->val.arr.size > 0);
    myjson_free(&v->val.arr.e[--v->val.arr.size]);
}

myjson_value* my_json_insert_array_element(myjson_value* v, size_t index) {
    assert(v != NULL && v->type == MYJSON_ARRAY && index <= v->val.arr.size);
    // incomplete
    return NULL;
}

void myjson_erase_array_element(myjson_value* v, size_t index, size_t count) {
    assert(v != NULL && v->type == MYJSON_ARRAY && index + count <= v->val.arr.size);
    //incomplete
}

void myjson_set_object(myjson_value* v, size_t capacity) {
    assert(v != NULL);
    myjson_free(v);
    v->type = MYJSON_OBJECT;
    v->val.obj.size = 0;
    v->val.obj.capacity = capacity;
    v->val.obj.m = capacity > 0 ? (myjson_member *)malloc(capacity * sizeof(myjson_member)) : NULL;
} 

size_t myjson_get_object_size(const myjson_value *v) {
    assert(v != NULL && v->type == MYJSON_OBJECT);
    return v->val.obj.size;
}

size_t myjson_get_object_capacity(const myjson_value* v) {
    assert(v != NULL && v->type == MYJSON_OBJECT);
    // incomplete
}

void myjson_reserve_object(myjson_value* v, size_t capacity) {
    assert(v != NULL && v->type == MYJSON_OBJECT);
    // incomplete
}

void myjson_shrink_object(myjson_value *v) {
    assert(v != NULL && v->type == MYJSON_OBJECT);
    // incomplete
}

void myjson_clear_object(myjson_value* v) {
    assert(v != NULL && v->type == MYJSON_OBJECT);
    // incomplete
}

const char *myjson_get_object_key(const myjson_value *v, size_t index) {
    assert(v != NULL && v->type == MYJSON_OBJECT);
    assert(index < v->val.obj.size);
    return v->val.obj.m[index].key;
}

size_t myjson_get_object_key_length(const myjson_value *v, size_t index) {
    assert(v != NULL && v->type == MYJSON_OBJECT);
    assert(index < v->val.obj.size);
    return v->val.obj.m[index].klen;
}

myjson_value *myjson_get_object_value(const myjson_value *v, size_t index) {
    assert(v != NULL && v->type == MYJSON_OBJECT);
    assert(index < v->val.obj.size);
    return &v->val.obj.m[index].v;
}

size_t myjson_find_object_index(const myjson_value* v, const char* key, size_t klen) {
    size_t i;
    assert(v != NULL && v->type == MYJSON_OBJECT && key != NULL);
    for (i = 0; i < v->val.obj.size; i++)
        if (v->val.obj.m[i],klen == klen && memcmp(v->val.obj.m[i].key, key, klen) == 0)
            return i;
    return MYJSON_KEY_NOT_EXIST;
}

myjson_value* myjson_find_object_value(myjson_value* v, const char* key, size_t klen) {
    size_t index = myjson_find_object_index(v, key, klen);
    return index != MYJSON_KEY_NOT_EXIST ? &v->val.obj.m[index].v : NULL;
}

myjson_value* myjson_set_object_value(myjson_value* v, const char* key, size_t klen) {
    assert(v != NULL && v->type == MYJSON_OBJECT && key != NULL);
    // incomplete
    return NULL;
}

void myjson_remove_object_value(myjson_value* v, size_t index) {
    assert(v != NULL && v->type == MYJSON_OBJECT && index < v->val.obj.size);
    // incomplete
}