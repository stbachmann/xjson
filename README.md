# xjson

A small header-only json library for C. The "unique" feature is that it allows use of the same code to serialize as well as deserialize, greatly reducing boiler-plate code required. The library also does not make any allocations (strings however need some special treatment, see below). Total line count is ~800 loc so it's easy to read through and pick up.

The library API was inspired by the kv implementation found in the cute framework (https://github.com/RandyGaul/cute_framework).

## Basic seriazlization/deserialization example
```C
// Root object has no key (NULL)
xjson_object_begin(json, NULL);

// Basic xjson usage, a key of "a". Either read or write to obj.a.
xjson_u32(json, "a", &obj.a);

// Begin an array with key "b". A special should be used to allow using the same declarations for both read and write
xjson_array_begin(json, "b");
for(int i=0; !xjson_array_reached_end(json, i, 3); i++)
{
    xjson_float(json, NULL, &obj.b[i]);
}
xjson_array_end(json);

xjson_string(json, "c", &obj.c);
xjson_bool(json, "d", &obj.d);

// A sub vec2 object with two floats
xjson_object_begin(json, "pos");
xjson_float(json, "x", &obj.pos.x);
xjson_float(json, "y", &obj.pos.y);
xjson_object_end(json);

xjson_object_end(json);
```

The interesting part is that this code is valid for both deserializing and serializing json. In order for xjson to know whether you intend on reading or writing json, you must set the correct mode before executing any other xjson functions:

```C
xjson json;
// Sets xjson to read mode using the string in json_string of given length
xjson_setup_read(&json, json_string, strlen(json_sample));

// Sets xjson to write mode, using pretty print. The output string will be written to json_str. 
// It is the caller's responsibility to supply a sufficiently sized buffer. 
// xjson will zero-terminate the string when finished writing.
xjson_setup_write(&json, true, json_str, 2048);
```

## String handling
Because strings always need some special care, xjson does not manage string allocations. Instead it provides the option to specify a string allocation function that allows the caller the define how strings should be allocated. This means it's totally up to you how you want memory to be allocated (big block upfront, using an allocator, etc.).

If no string allocator is supplied, xjson will use malloc to allocate strings and expect the caller to free the memory when no longer needed.

A simple custom allocator using malloc could look like this:
```C
// xjson expects the returned string to be zero-terminated as the supplied strings are not zero-terminated
char* allocate_string(const char* str, size_t str_len, void* mem_ctx)
{
    // mem_ctx is a pointer to some user-defined data. Can be used to pass allocators etc. into the string allocation function
    char* new_str = malloc(str_len + 1); // zero-terminate!
    memcpy(new_str, str, str_len);
    new_str[str_len] = '\0';
    return new_str;
}
```

We can set the custom allocator by calling `xjson_set_string_allocator(json, allocate_string);`.

## Error handling

xjson uses asserts but also generates error messages for things that aren't "breaking". If json encounters an issue in reading or writing, it'll set `error` bool in the xjson struct to true. The code will continue running but not actually process anything. A hopefully useful message will be written to `error_message` inside the xjson object. It's up the caller to decide how to output that error.

```C
if(json->error)
{
    puts(json->error_message);
}
```

## Special case read/write handling

As much as possible xjson allows the same processing for read as well as write. But there may sometimes still be situations that need separate paths. For that purpose you may query the current mode by calling `xjson_get_state(xjson* json)`.

```C
if(xjson_get_state(json) == XJSON_STATE_READ)
{
    // Process for read
}
else 
{
    // Process for write
}
```

## A Full Example

Here's a basic example showcasing how to read/write json using xjson. You may also look at the supplied `sample.c` file.

```C
#define XJSON_H_IMPLEMENTATION
#include "xjson.h"

typedef struct simple_struct {
    uint32_t a;
    float b[3];
} simple_struct;

// The main json processing function used for both read and write
void process_json(xjson* json, simple_struct* obj)
{
  xjson_object_begin(json, NULL);
  
  xjson_u32(json, "a", &obj->a);

  xjson_array_begin(json, "b");
  for(int i=0; !xjson_array_reached_end(json, i, 3); i++)
  {
      xjson_float(json, NULL, &obj->b[i]);
  }
  xjson_array_end(json);

  xjson_object_end(json);
}

int main(int argc, char* argv[])
{
    simple_struct obj = {
        .a = 10,
        .b = { 0.1, 10.0, 15.0 }
    };
     
    // A simple json string to parse
    const char* json_str = "{ \"a\": 20, \"b\": [2.0, 1.0, 3.0] }";
    // A char buffer to write the json to
    char json_out[2048];
    
    // Setup xjson
    xjson json;
    
    // Set xjson to read mode
    xjson_setup_read(json, json_sample, strlen(json_sample));
    process_json(json, &obj);
    
    // Set xjson to write mode
    xjson_setup_write(json, true, json_str, 2048);
    process_json(json, &obj);
    
    // Process any errors
    if(json->error)
    {
        // Print error or handle in some other way
    }
}
```
