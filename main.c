#include "parser.h"

int main(void)
{
    // Example of parsing file
    JsonValue* json = malloc(sizeof(JsonValue));
    jsonFileLoad("./tests/basic.json", json);
    json_print(json, 4, 0);

    // Making json in code
    JsonValue* code_json = malloc(sizeof(JsonValue)); 
    json_init_object(code_json); // or initialise object as JsonValue code_json = {.type=JSON_OBJECT}
    json_add_child(code_json, "first_key", json_new_string("hello"));
    json_add_child(code_json, "second_key", json_new_number(32.1253));
    json_add_child(code_json, "third_key", json_new_sarray(STR_ARR("a", "b", "c", "d")));

    JsonValue* nested = malloc(sizeof(JsonValue)); 
    json_init_object(nested); // or initialise object as JsonValue code_json = {.type=JSON_OBJECT}
    json_add_child(nested, "some_key", json_new_bool(true));
    json_add_child(code_json, "third_key", nested);

    json_print(code_json, 4, 0);

    json_free(json);
    json_free(code_json);
	return 0;
}