
/*

    

*/

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include <inttypes.h> // for PRIi64 macros

#define XJSON_MALLOC malloc
#define XJSON_FREE free

#include <assert.h>
#define XJSON_ASSERT(c) assert(c)

#define XJSON_LOG(s) puts(s)

// Public API
typedef struct xjson xjson;

void xjson_setup_read(xjson* json, const char* json_str, size_t len);
void xjson_setup_write(xjson* json, bool pretty_print, char* buffer, size_t len);
void xjson_set_string_allocator(xjson* json, char* (*string_allocator)(const char* str, size_t size));

void xjson_object_begin(xjson* json, const char* key);
void xjson_object_end(xjson* json);
void xjson_array_begin(xjson* json, const char* key);
void xjson_array_end(xjson* json);
bool xjson_array_size(xjson* json, int size);

void xjson_u8(xjson* json, const char* key, uint8_t* val);
void xjson_u16(xjson* json, const char* key, uint16_t* val);
void xjson_u32(xjson* json, const char* key, uint32_t* val);
void xjson_u64(xjson* json, const char* key, uint64_t* val);
void xjson_i8(xjson* json, const char* key, int8_t* val);
void xjson_i16(xjson* json, const char* key, int16_t* val);
void xjson_i32(xjson* json, const char* key, int32_t* val);
void xjson_i64(xjson* json, const char* key, int64_t* val);
void xjson_float(xjson* json, const char* key, float* val);
void xjson_double(xjson* json, const char* key, double* val);
void xjson_bool(xjson* json, const char* key, bool* val);
void xjson_string(xjson* json, const char* key, const char** str);

//----------------------------------------------------------------------------------
// API implementation
//----------------------------------------------------------------------------------
typedef enum xjson_state
{
    XJSON_STATE_UNITIALIZED = 0,
    XJSON_STATE_READ,
    XJSON_STATE_WRITE
} xjson_state;

typedef enum xjson_int_type
{
    XJSON_INT_TYPE_U8,
    XJSON_INT_TYPE_U16,
    XJSON_INT_TYPE_U32,
    XJSON_INT_TYPE_U64,
    XJSON_INT_TYPE_I8,
    XJSON_INT_TYPE_I16,
    XJSON_INT_TYPE_I32,
    XJSON_INT_TYPE_I64
} xjson_int_type;

typedef struct xjson
{
    uint32_t _start_canary;

    // Current state of this xjson object. Is XJSON_STATE_UNITIALIZED by default
    xjson_state mode;
    // Will output json with newline/tab.
    bool pretty_print;
    int intendation;

    // These point to the beginning/end and current location in either the write or read buffer
    uint8_t* current;
    uint8_t* end;
    uint8_t* start;

    char* (*string_allocator)(const char* str, size_t size);

    // Error handling. Set to true on error + appropriate message in error_message.
    bool error;
    char error_message[256];

    uint32_t _end_canary;
} xjson;

void xjson_error(xjson* json, const char* message)
{
    json->error = true;
    if(json->mode == XJSON_STATE_READ)
    {
        // get line number and line index
        int line = 0;
        int line_begin = 0;

        for (int i=0; i < json->current-json->start; i++)
        {
            if (*(json->start+i) == '\n') 
            {
                line++;
                line_begin = i + 1;
            }
        }

        int character_offset = json->current-(json->start+line_begin);
        char* sample_range_start = json->current - 8 < json->start ? json->start : json->current - 8;
        char* sample_range_end = json->current + 8 > json->end ? json->end : json->current + 8;
        // print formatted line data
        sprintf(json->error_message, "Error (%i, %i): %s\n\t%.*s\n\t%.*s^", line, character_offset, message, sample_range_end-sample_range_start, sample_range_start, json->current-sample_range_start, "-------------");
    }
    else 
    {
        strcpy(json->error_message, message);
    }
}

bool xjson_is_space(char c)
{
    return (c == ' ') |
		(c == '\t') |
		(c == '\n') |
		(c == '\v') |
		(c == '\f') |
		(c == '\r');
}

char xjson_consume(xjson* json)
{
    if(json->error) return 0;

    if(json->current == json->end)
        return 0;

    // Consume any white space that might be there
    char c = *(++json->current);
    while(xjson_is_space(c))
    {
        if(json->current == json->end)
            return 0;
        c = *(++json->current);
    }

    return c;
}

char xjson_peek(xjson* json)
{
    if(json->error) return 0;

    uint8_t* ptr = json->current + 1;
    while(ptr < json->end && xjson_is_space(*ptr))
    {
        ptr++;
    }

    return ptr < json->end ? *ptr : 0;
}

void xjson_try(xjson* json, char expected_character)
{
    if(json->error) return;

    if(*json->current == expected_character)
        xjson_consume(json);
}

void xjson_expect(xjson* json, char expected_character)
{
    if(json->error) return;

    if(*json->current != expected_character){
        xjson_error(json, "Unexpected token found.");
        return;
    }

    xjson_consume(json);
}

void xjson_expect_token(xjson* json, const char* token, size_t len)
{
    if(json->error) return;

    for(int i=0; i<len; i++){
        if(*(json->current+i) != token[i]) {
           xjson_error(json, "Unexpected token found.");
           return;
        }
    }

    json->current += len;
    if(xjson_is_space(*json->current)){
        xjson_consume(json);
    }
}

void xjson_expect_key(xjson* json, const char* key)
{
    xjson_expect(json, '\"');
    if(json->error) return;

    // We find the size of the key and advance the current pointer to the end of key
    const char* key_start = json->current;
    while(*json->current != '\"') {
        xjson_consume(json);
        if(json->error) return;
    }
    size_t key_len = json->current - key_start;

    // Then we compare the two to make sure they match
    if(memcmp(key, key_start, key_len) != 0){
        xjson_error(json, "Expected key does not match.");
        return;
    }

    xjson_expect(json, '\"');
    xjson_expect(json, ':');
}

void xjson_expect_and_parse_int(xjson* json, int64_t* out_value)
{
    if(json->error) return;

    char* end_ptr;
    int64_t value = strtoll(json->current, &end_ptr, 10);
    if(json->current == end_ptr)
    {
        // This means we couldn't parse the integer properly.
        xjson_error(json, "Invalid integer found. Couldn't parse value.");
        return;
    }
    *out_value = value;

    // Move pointer to end of number value + ensure all white space is consumed
    json->current = end_ptr;
    if(xjson_is_space(*json->current)){
        xjson_consume(json);
    }
}

void xjson_expect_and_parse_double(xjson* json, double* out_value)
{
    if(json->error) return;

    char* end_ptr;
    double value = strtod(json->current, &end_ptr);
    if(json->current == end_ptr)
    {
        // This means we couldn't parse the integer properly.
        xjson_error(json, "Invalid double found. Couldn't parse value.");
        return;
    }
    *out_value = value;

    // Move pointer to end of number value
    json->current = end_ptr;
    if(xjson_is_space(*json->current)){
        xjson_consume(json);
    }
}

void xjson_expect_and_parse_string(xjson* json, const char** str)
{
    xjson_expect(json, '\"');
    if(json->error) return;

    char* str_start = json->current;

    while(*json->current != '\"')
    {
        xjson_consume(json);
        if(json->error) return;
    }

    size_t str_len = json->current - str_start;
    *str = json->string_allocator(str_start, str_len);
    
    xjson_expect(json, '\"');
}

void xjson_expect_and_parse_bool(xjson* json, bool* val)
{
    if(json->error) return;

    if(*json->current == 't')
    {
        xjson_expect_token(json, "true", 4);
        if(!json->error)
            *val = true;
    }
    else if(*json->current == 'f')
    {
        xjson_expect_token(json, "false", 5);
        if(!json->error)
            *val = false;
    }
    else 
    {
        xjson_error(json, "Unexpected token whilst parsing bool.");
    }
}

void xjson_print_token(xjson* json, const char* token, size_t len)
{
    if(json->error) return;

    if(json->current + len > json->end)
    {
       xjson_error(json, "Write buffer is too small to write to. Abort.");
    }

    strncpy(json->current, token, len);
    json->current += len;
}

void xjson_print_key(xjson* json, const char* key)
{
    if(json->error) return;
    xjson_print_token(json, "\"", 1);
    xjson_print_token(json, key, strlen(key));
    xjson_print_token(json, "\"", 1);
    xjson_print_token(json, ":", 1);
}

void xjson_print_new_line(xjson* json)
{
    if(json->error) return;
    xjson_print_token(json, "\n", 1);
    for(int i=0; i<json->intendation; i++)
    {
        xjson_print_token(json, "\t", 1);
    }
}

void xjson_setup_read(xjson* json, const char* str, size_t len)
{
    XJSON_ASSERT(json);
    XJSON_ASSERT(str);

    json->start = str;
    json->current = str;
    json->end = str+len;
    json->mode = XJSON_STATE_READ;
}

void xjson_setup_write(xjson* json, bool pretty_print, char* buffer, size_t len)
{
    XJSON_ASSERT(json);
    XJSON_ASSERT(buffer);

    json->pretty_print = pretty_print;
    json->start = buffer;
    json->current = buffer;
    json->end = buffer+len;
    json->mode = XJSON_STATE_WRITE;
}

void xjson_set_string_allocator(xjson* json, char* (*string_allocator)(const char* str, size_t size))
{
    XJSON_ASSERT(json);
    XJSON_ASSERT(string_allocator);

    json->string_allocator = string_allocator;
}

void xjson_object_begin(xjson* json, const char* key)
{
    XJSON_ASSERT(json);
    XJSON_ASSERT(json->mode != XJSON_STATE_UNITIALIZED);

    if(json->error) return;

    if(json->mode == XJSON_STATE_READ){
        if(key != NULL){
            xjson_expect_key(json, key);
        }

        xjson_expect(json, '{');
    }
    else {
        if(key != NULL){
            if(json->pretty_print) xjson_print_new_line(json);
            xjson_print_key(json, key);
        }

        xjson_print_token(json, "{", 1);
    }
    json->intendation += 1;
}

void xjson_object_end(xjson* json)
{
    XJSON_ASSERT(json);
    XJSON_ASSERT(json->mode != XJSON_STATE_UNITIALIZED);
    XJSON_ASSERT(json->intendation > 0);

    if(json->error) return;

    json->intendation -= 1;

    if(json->mode == XJSON_STATE_READ)
    {
        xjson_expect(json, '}');
        xjson_try(json, ',');
    }
    else 
    {
        // This is to overwrite the previous ',' insertion
        json->current--;

        // Special case for empty objects;
        if(*json->current == '{')
            json->current++;

        if(json->pretty_print) xjson_print_new_line(json);
        xjson_print_token(json, "}", 1);
        xjson_print_token(json, ",", 1);
    }

    // Special case for closing the root object, null-terminate the output string
    if(json->intendation == 0 && json->mode == XJSON_STATE_WRITE)
    {
        *(--json->current) = '\0';
    }
}

void xjson_array_begin(xjson* json, const char* key)
{
    XJSON_ASSERT(json);
    XJSON_ASSERT(json->mode != XJSON_STATE_UNITIALIZED);

    if(json->error) return;

    if(json->mode == XJSON_STATE_READ)
    {
        if(key != NULL){
            xjson_expect_key(json, key);
        }

        xjson_expect(json, '[');
    }
    else 
    {
        if(key != NULL){
            if(json->pretty_print) xjson_print_new_line(json);
            xjson_print_key(json, key);
        }

        xjson_print_token(json, "[", 1);
    }
    json->intendation += 1;
}

void xjson_array_end(xjson* json)
{
    XJSON_ASSERT(json);
    XJSON_ASSERT(json->mode != XJSON_STATE_UNITIALIZED);
    XJSON_ASSERT(json->intendation > 0);

    if(json->error) return;

    json->intendation -= 1;

    if(json->mode == XJSON_STATE_READ)
    {
        xjson_expect(json, ']');
        xjson_try(json, ',');
    }
    else 
    {
        json->current--;
        
        // Special case for empty arrays;
        if(*json->current == '[')
            json->current++;
        
        if(json->pretty_print) xjson_print_new_line(json);
        xjson_print_token(json, "]", 1);
        xjson_print_token(json, ",", 1);
    }
}

// TODO: Find a better way to handle this. It's not very nice :(
bool xjson_array_reached_end(xjson* json, int current, int size)
{
    if(json->mode == XJSON_STATE_READ)
    {
        if(*json->current == ']' || json->error)
            return true;
        
        return false;
    }
    else 
    {
        return current >= size;
    }

    return true;
}

void xjson_integer(xjson* json, const char* key, void* val, xjson_int_type type)
{
    XJSON_ASSERT(json);
    XJSON_ASSERT(json->mode != XJSON_STATE_UNITIALIZED);
    XJSON_ASSERT(val);

    if(json->mode == XJSON_STATE_READ)
    {
        if(key != NULL)
        {
            xjson_expect_key(json, key);
        }

        int64_t number;
        xjson_expect_and_parse_int(json, &number);
        if(json->error) return;

        switch (type)
        {
        case XJSON_INT_TYPE_U8:
            *(uint8_t*)val = (uint8_t)number;
            break;
        case XJSON_INT_TYPE_U16:
            *(uint16_t*)val = (uint16_t)number;
            break;
        case XJSON_INT_TYPE_U32:
            *(uint32_t*)val = (uint32_t)number;
            break;
        case XJSON_INT_TYPE_U64:
            *(uint64_t*)val = (uint64_t)number;
            break;
        case XJSON_INT_TYPE_I8:
            *(int8_t*)val = (int8_t)number;
            break;
        case XJSON_INT_TYPE_I16:
            *(int16_t*)val = (int16_t)number;
            break;
        case XJSON_INT_TYPE_I32:
            *(int32_t*)val = (int32_t)number;
            break;
        case XJSON_INT_TYPE_I64:
            *(int64_t*)val = (int64_t)number;
            break;
        default:
            // TODO: error
            break;
        }
        
        xjson_try(json, ',');
    }
    else
    {
        if(json->pretty_print) xjson_print_new_line(json);

        if(key != NULL)
        {
            xjson_print_key(json, key);
        }

        int len = 0;
        switch (type)
        {
        case XJSON_INT_TYPE_U8:
            len = sprintf(json->current, "%" PRIu8, *(uint8_t*)val);
            break;
        case XJSON_INT_TYPE_U16:
            len = sprintf(json->current, "%" PRIu16, *(uint16_t*)val);
            break;
        case XJSON_INT_TYPE_U32:
            len = sprintf(json->current, "%" PRIu32, *(uint32_t*)val);
            break;
        case XJSON_INT_TYPE_U64:
            len = sprintf(json->current, "%" PRIu64, *(uint64_t*)val);
            break;
        case XJSON_INT_TYPE_I8:
            len = sprintf(json->current, "%" PRIi8, *(int8_t*)val);
            break;
        case XJSON_INT_TYPE_I16:
            len = sprintf(json->current, "%" PRIi16, *(int16_t*)val);
            break;
        case XJSON_INT_TYPE_I32:
            len = sprintf(json->current, "%" PRIi32, *(int32_t*)val);
            break;
        case XJSON_INT_TYPE_I64:
            len = sprintf(json->current, "%" PRIi64, *(int64_t*)val);
            break;
        default:
            // TODO: error
            break;
        }
        json->current += len;

        xjson_print_token(json, ",", 1);
    }
}

void xjson_u8(xjson* json, const char* key, uint8_t* val)
{
    xjson_integer(json, key, val, XJSON_INT_TYPE_U8);
}

void xjson_u16(xjson* json, const char* key, uint16_t* val)
{
    xjson_integer(json, key, val, XJSON_INT_TYPE_U16);
}

void xjson_u32(xjson* json, const char* key, uint32_t* val)
{
    xjson_integer(json, key, val, XJSON_INT_TYPE_U32);
}

void xjson_u64(xjson* json, const char* key, uint64_t* val)
{
    xjson_integer(json, key, val, XJSON_INT_TYPE_U64);
}

void xjson_i8(xjson* json, const char* key, int8_t* val)
{
    xjson_integer(json, key, val, XJSON_INT_TYPE_I8);
}

void xjson_i16(xjson* json, const char* key, int16_t* val)
{
    xjson_integer(json, key, val, XJSON_INT_TYPE_I16);
}

void xjson_i32(xjson* json, const char* key, int32_t* val)
{
    xjson_integer(json, key, val, XJSON_INT_TYPE_I32);
}

void xjson_i64(xjson* json, const char* key, int64_t* val)
{
    xjson_integer(json, key, val, XJSON_INT_TYPE_I64);
}

void xjson_float(xjson* json, const char* key, float* val)
{
    XJSON_ASSERT(json);
    XJSON_ASSERT(json->mode != XJSON_STATE_UNITIALIZED);

    if(json->mode == XJSON_STATE_READ)
    {
        if(key != NULL)
        {
            xjson_expect_key(json, key);
        }

        double out;
        xjson_expect_and_parse_double(json, &out);
        if(!json->error)
            *val = (float)out;

        xjson_try(json, ',');
    }
    else
    {
        if(json->pretty_print) xjson_print_new_line(json);

        if(key != NULL)
        {
            xjson_print_key(json, key);
        }

        int len = sprintf(json->current, "%f", *val);
        json->current += len;

        xjson_print_token(json, ",", 1);
    }
}

void xjson_double(xjson* json, const char* key, double* val)
{
    XJSON_ASSERT(json);
    XJSON_ASSERT(json->mode != XJSON_STATE_UNITIALIZED);

    if(json->mode == XJSON_STATE_READ)
    {
        if(key != NULL)
        {
            xjson_expect_key(json, key);
        }

        xjson_expect_and_parse_double(json, val);
        xjson_try(json, ',');
    }
    else
    {
        if(json->pretty_print) xjson_print_new_line(json);

        if(key != NULL)
        {
            xjson_print_key(json, key);
        }

        int len = sprintf(json->current, "%f", *val);
        json->current += len;

        xjson_print_token(json, ",", 1);
    }
}

void xjson_bool(xjson* json, const char* key, bool* val)
{
    XJSON_ASSERT(json);
    XJSON_ASSERT(json->mode != XJSON_STATE_UNITIALIZED);

    if(json->mode == XJSON_STATE_READ)
    {
        if(key != NULL)
        {
            xjson_expect_key(json, key);
        }

        xjson_expect_and_parse_bool(json, val);
        xjson_try(json, ',');
    }
    else
    {
        if(json->pretty_print) xjson_print_new_line(json);

        if(key != NULL)
        {
            xjson_print_key(json, key);
        }

        int len = sprintf(json->current, "%s", *val == true ? "true" : "false");
        json->current += len;

        xjson_print_token(json, ",", 1);
    }
}

void xjson_string(xjson* json, const char* key, const char** str)
{
    XJSON_ASSERT(json);
    XJSON_ASSERT(json->mode != XJSON_STATE_UNITIALIZED);

    if(json->mode == XJSON_STATE_READ)
    {
        if(key != NULL)
        {
            xjson_expect_key(json, key);
        }

        xjson_expect_and_parse_string(json, str);
        xjson_try(json, ',');
    }
    else
    {
        if(json->pretty_print) xjson_print_new_line(json);

        if(key != NULL)
        {
            xjson_print_key(json, key);
        }

        int len = sprintf(json->current, "\"%s\"", *str);
        json->current += len;
        
        xjson_print_token(json, ",", 1);
    }
}
