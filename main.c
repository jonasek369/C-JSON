#include "parser.h"
const char* token_type_to_string(TokenType type) {
    switch (type) {
        case TOKEN_LEFT_BRACE: return "TOKEN_LEFT_BRACE";
        case TOKEN_RIGHT_BRACE: return "TOKEN_RIGHT_BRACE";
        case TOKEN_LEFT_BRACKET: return "TOKEN_LEFT_BRACKET";
        case TOKEN_RIGHT_BRACKET: return "TOKEN_RIGHT_BRACKET";
        case TOKEN_COMMA: return "TOKEN_COMMA";
        case TOKEN_COLON: return "TOKEN_COLON";
        case TOKEN_STRING: return "TOKEN_STRING";
        case TOKEN_NUMBER: return "TOKEN_NUMBER";
        case TOKEN_TRUE: return "TOKEN_TRUE";
        case TOKEN_FALSE: return "TOKEN_FALSE";
        case TOKEN_NULL: return "TOKEN_NULL";
        case TOKEN_EOF: return "TOKEN_EOF";
        case TOKEN_ERROR: return "TOKEN_ERROR";
        default: return "UNKNOWN_TOKEN";
    }
}

JsonValue* json_get(JsonValue* object, char* key) {
    list* pairs = object->object;
    for (size_t i = 0; i < pairs->size; i++) {
        JsonPair* pair = get_pair(pairs, i);
        if (strcmp(pair->key, key) == 0) {
            return pair->value;
        }
    }
    return NULL;
}

int main(int argc, char const *argv[])
{
	JsonValue* json = load_file("tests/basic.json");
    printf("%s\n", json_get(json, "name")->string);
    printf("%f\n", json_get(json, "age")->number);
    printf("%f\n", json_get(json, "float")->number);
    list* langs = json_get(json, "languages")->array;
    for(size_t i = 0; i < langs->size; i++){
        JsonValue* e = get_value(langs, i);
        printf("%s\n", e->string);
    }
    printf("%i\n", json_get(json, "is_working")->boolean);
    printf("is null %i\n", (json_get(json, "unknown")->type == TOKEN_NULL));
    printf("%i\n", json_get(json, "floor_level")->boolean);
    printf("%s\n", json_get(json_get(json, "sum"), "test")->string);
    printf("%s\n", json_get(json, "empty")->string);
    printf("%f\n", json_get(json, "big_number")->number);

    list* outter = json_get(json, "nested_array")->array;
    for (size_t i = 0; i < outter->size; i++) {
        list* inner = get_value(outter, i)->array;
        for (size_t j = 0; j < inner->size; j++) {
            JsonValue* element = get_value(inner, j);
            printf("%.0f\n", element->number);  // If you expect integers
        }
    }
	return 0;
}