
#define XJSON_H_IMPLEMENTATION
#include "xjson.h"
#include <stdio.h>

typedef struct vec2 {
    float x, y;
} vec2;

typedef struct simple_struct {
    uint32_t a;
    float b[3];
    char* c;
    bool d;

    vec2 pos;
    vec2 delta;
} simple_struct;

char* allocate_string(const char* str, size_t str_len, void* mem_ctx)
{
    char* new_str = malloc(str_len + 1); // zero-terminate!
    memcpy(new_str, str, str_len);
    new_str[str_len] = '\0';
    return new_str;
}

const char* json_sample = "{ \"a\": 20, \"b\": [2.0, 1.0, 3.0], \"c\": \"A test string!\", \"d\": false, \"pos\": { \"x\": 4, \"y\": 10.5 }, \"delta\": { \"x\": 20.3331, \"y\": 8 }}";

int main(int argc, char* argv[])
{
    simple_struct obj = {
        .a = 10,
        .b = { 0.1, 10.0, 15.0 },
        .c = "Test String!",
        .d = true,
        .pos = { .x=1.0, .y=2.0 },
        .delta = { .x=10, .y=10 }
    };
    bool read = true;
    char json_str[2048];
    xjson* json = malloc(sizeof(xjson));
    memset(json, 0, sizeof(xjson));
    
    xjson_set_string_allocator(json, allocate_string);

    if(read)
    {
        xjson_setup_read(json, json_sample, strlen(json_sample));
    }
    else 
    {
        xjson_setup_write(json, true, json_str, 2048);
    }

    xjson_object_begin(json, NULL);
    {
        xjson_u32(json, "a", &obj.a);
        
        xjson_array_begin(json, "b");
        // Unsure how we should handle this better??
        for(int i=0; !xjson_array_reached_end(json, i, 3); i++)
        {
            xjson_float(json, NULL, &obj.b[i]);
        }
        xjson_array_end(json);

        xjson_string(json, "c", &obj.c);

        xjson_bool(json, "d", &obj.d);

        xjson_object_begin(json, "pos");
        {
            xjson_float(json, "x", &obj.pos.x);
            xjson_float(json, "y", &obj.pos.y);
        }
        xjson_object_end(json);

        xjson_object_begin(json, "delta");
        {
            char* key_x = "x";
            char* key_y = "y";

            // This is doing it the hard way, by reading key first
            // Can be helpful working with hash maps for example
            xjson_key(json, &key_x);
            xjson_float(json, NULL, &obj.delta.x);

            xjson_key(json, &key_y);
            xjson_float(json, NULL, &obj.delta.y);
        }
        xjson_object_end(json);
    }
    xjson_object_end(json);

    if(!read)
        puts(json_str);

    if(json->error)
    {
        puts(json->error_message);
    }
}