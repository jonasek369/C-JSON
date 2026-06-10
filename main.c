#include "parser.h"

int main(void)
{
    // Example of parsing file
    JsonValue* json_basic = malloc(sizeof(JsonValue));
    json_init_object(json_basic);
    if(!jsonFileLoad("./tests/basic.json", json_basic)){
        printf("Failed to load basic.json\n");
        return 1;
    }
    JsonValue* float_num = shget(json_basic->object, "float");
    if(float_num->flags & HAS_FRACTION){
        printf("This is a float!\n");
    }else{
        printf("This wont be executed!\n");
    }
    JsonValue* int_num = shget(json_basic->object, "age");
    if(int_num->flags & HAS_FRACTION){
        printf("This is a float!\n");
    }else{
        printf("This now will be executed!\n");
    }
    json_print(json_basic, 4, 0);


    JsonValue* json_fault = malloc(sizeof(JsonValue));
    json_init_object(json_fault);
    if(!jsonFileLoad("./tests/faulty.json", json_fault)){
        printf("failed to load faulty.json this is expected\n");
    }

    // Making json in code
    JsonValue* code_json = malloc(sizeof(JsonValue)); 
    json_init_object(code_json);
    json_add_child(code_json, "first_key", json_new_string("hello"));
    json_add_child(code_json, "second_key", json_new_float(32.1253));
    json_add_child(code_json, "third_key", json_new_sarray(STR_ARR("a", "b", "c", "d")));

    JsonValue* nested = malloc(sizeof(JsonValue)); 
    json_init_object(nested);
    json_add_child(nested, "some_key", json_new_bool(true));
    json_add_child(code_json, "fourth_key", nested);

    json_print(code_json, 4, 0);

    json_free(json_fault);
    json_free(json_basic);
    json_free(code_json);
	return 0;
}