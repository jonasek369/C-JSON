#include "parser.h"

int main(void)
{
    Arena arena = {0};


    // Example of parsing file
    JsonValue json = {0};
    jsonFileLoad("./tests/t.json", &arena, &json);
    json_print(&json, 4, 0);

    // Making json in code
    JsonValue code_json = {0}; 
    json_init_object(&code_json); // or initialise object as JsonValue code_json = {.type=JSON_OBJECT}
    json_add_child(&code_json, "first_key", json_new_string(&arena, "hello"));
    json_add_child(&code_json, "second_key", json_new_number(&arena, 32.1253));
    json_add_child(&code_json, "third_key", json_new_sarray(&arena, STR_ARR("a", "b", "c", "d")));

    JsonValue nested = {.type=JSON_OBJECT};
    json_add_child(&nested, "some_key", json_new_bool(&arena, true));
    json_add_child(&code_json, "third_key", &nested);

    json_print(&code_json, 4, 0);

    json_free(&json);
    json_free(&code_json);
    arena_free(&arena);   
	return 0;
}