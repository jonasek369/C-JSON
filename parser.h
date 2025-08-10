#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdalign.h>
#include <stdint.h>
#include <ctype.h>
#include <math.h>

#define STB_DS_IMPLEMENTATION
#define STBDS_STRING_KEY
#include "stb_ds.h"

#define ARENA_IMPLEMENTATION
#include "arena.h"

typedef enum {
    JSON_NULL, JSON_BOOL, JSON_NUMBER, JSON_STRING,
    JSON_ARRAY, JSON_OBJECT
} JsonTypeConstants;

typedef uint8_t JsonType;


static const bool is_whitespace[256] = {
  [' '] = 1, ['\t'] = 1, ['\n'] = 1, ['\r'] = 1
};

char* file_read(const char* file_name) {
    FILE* fp = fopen(file_name, "rb");
    if (!fp) {
        fprintf(stderr, "Could not read file\n");
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    rewind(fp);

    if (file_size < 0) {
        fprintf(stderr, "Could not determine file size\n");
        fclose(fp);
        return NULL;
    }

    char* content = NULL;
    arrsetcap(content, file_size + 1);

    arrsetlen(content, file_size); // sets logical size
    size_t read_count = fread(content, 1, file_size, fp);
    fclose(fp);

    if (read_count != file_size) {
        fprintf(stderr, "Could not read entire file\n");
        arrfree(content);
        return NULL;
    }

    arrput(content, '\0');

    return content;
}

typedef enum {
    TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
    TOKEN_LEFT_BRACKET, TOKEN_RIGHT_BRACKET,
    TOKEN_COMMA, TOKEN_COLON,
    TOKEN_STRING, TOKEN_NUMBER,
    TOKEN_TRUE, TOKEN_FALSE, TOKEN_NULL,
    TOKEN_EOF, TOKEN_ERROR
} TokenType;

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


typedef struct {
    TokenType type;
    const char *start;
    size_t length;
} Token;




void parse_string(char* json, size_t* index, Token* tok) {
    size_t start = ++(*index);  // skip opening quote

    while (json[*index] != '"' && json[*index] != '\0') {
        if (json[*index] == '\\' && json[*index + 1] != '\0') {
            (*index) += 2;  // skip escaped character
        } else {
            (*index)++;
        }
    }

    tok->start = &json[start];
    tok->length = (*index) - start;
    tok->type = TOKEN_STRING;

    if (json[*index] == '"') {
        (*index)++;  // skip closing quote
    } else {
        tok->type = TOKEN_ERROR;  // Unterminated string
    }
}

void parse_number(char* json, size_t* index, Token* tok) {
    size_t start = *index;

    // Handle optional leading minus
    if (json[*index] == '-') {
        if (!isdigit(json[*index + 1])) {
            tok->type = TOKEN_ERROR;
            tok->length = 1;
            (*index)++;
            return;
        }
        (*index)++;
    }

    bool found_decimal = false;

    // Parse integer and fractional parts
    while (true) {
        char c = json[*index];

        if (isdigit(c)) {
            (*index)++;
        } else if (c == '.' && !found_decimal) {
            // Check digit after decimal point
            if (!isdigit(json[*index + 1])) {
                tok->type = TOKEN_ERROR;
                tok->length = (*index) - start;
                return;
            }
            found_decimal = true;
            (*index)++;
        } else {
            break;
        }
    }

    // Parse exponent part
    if (json[*index] == 'e' || json[*index] == 'E') {
        (*index)++;

        // Optional sign after 'e'
        if (json[*index] == '+' || json[*index] == '-') {
            (*index)++;
        }

        // Must have digits after exponent
        if (!isdigit(json[*index])) {
            tok->type = TOKEN_ERROR;
            tok->length = (*index) - start;
            return;
        }

        while (isdigit(json[*index])) {
            (*index)++;
        }
    }

    if ((*index) == start) {
        // No digits parsed at all
        tok->type = TOKEN_ERROR;
        tok->length = 0;
    } else {
        tok->type = TOKEN_NUMBER;
        tok->length = (*index) - start;
        tok->start = &json[start];
    }
}

void parse_keyword(char* json, size_t* index, Token* tok) {
    char* ptr = &json[*index];

    if (strncmp(ptr, "true", 4) == 0) {
        tok->type = TOKEN_TRUE;
        tok->length = 4;
        (*index) += 4;
    } else if (strncmp(ptr, "false", 5) == 0) {
        tok->type = TOKEN_FALSE;
        tok->length = 5;
        (*index) += 5;
    } else if (strncmp(ptr, "null", 4) == 0) {
        tok->type = TOKEN_NULL;
        tok->length = 4;
        (*index) += 4;
    } else {
        tok->type = TOKEN_ERROR;
        tok->length = 1;
        (*index) += 1;
    }
}

Token next_token(char* json, size_t* index) {
    while (is_whitespace[(unsigned char)json[*index]]) {
        (*index)++;
    }


    char c = json[*index];

    Token tok;
    tok.start = &json[*index];
    tok.length = 1;

    if (isdigit(c)) {
        parse_number(json, index, &tok);
        return tok;
    }

    switch (c) {
        case '{': tok.type = TOKEN_LEFT_BRACE; (*index)++; break;
        case '}': tok.type = TOKEN_RIGHT_BRACE; (*index)++; break;
        case '[': tok.type = TOKEN_LEFT_BRACKET; (*index)++; break;
        case ']': tok.type = TOKEN_RIGHT_BRACKET; (*index)++; break;
        case ':': tok.type = TOKEN_COLON; (*index)++; break;
        case ',': tok.type = TOKEN_COMMA; (*index)++; break;
        case '"': {
            parse_string(json, index, &tok);
            return tok;
        }
        case 't':
        case 'f':
        case 'n': {
            parse_keyword(json, index, &tok);
            return tok;
        }
        case '\0':
            tok.type = TOKEN_EOF;
            tok.length = 0;
            return tok;
        default:
            tok.type = TOKEN_ERROR;
            return tok;
    }

    return tok;
}


Token* tokenize(char* file_content){
    Token* tokens = NULL;
    size_t index = 0;
    while (true) {
        Token tok = next_token(file_content, &index);
        if (tok.type == TOKEN_EOF || tok.type == TOKEN_ERROR) {
            break;
        }
        arrput(tokens, tok);
    }
    return tokens;
}

typedef struct JsonValue JsonValue;

typedef struct {
    char *key;
    JsonValue *value;
} JsonPair;

struct JsonValue {
    union {
        double number;
        char *string;
        int boolean;
        JsonValue** array;  // list of JsonValue
        JsonPair* object;   // sh of JsonPair
    };
    JsonType type;
};


char* arena_strndup(Arena* arena, const char* str, size_t length) {
    size_t len = strnlen(str, length);
    char* new_str = arena_alloc(arena, len + 1);
    memcpy(new_str, str, len);
    new_str[len] = '\0';
    return new_str;
}

typedef struct {
    Token* tokens;
    size_t index;
} Parser;


void advance(Parser* parser) {
    parser->index++;
}

void parse_object(Parser* parser, JsonValue* json_object, Arena* arena);
void parse_array(Parser* parser, JsonValue* json_object, Arena* arena);


JsonValue parse_value(Parser* parser, Arena* arena){
    Token token = parser->tokens[parser->index];
    JsonValue value;

    switch(token.type){
        case TOKEN_STRING:
            value.type = JSON_STRING;
            value.string = arena_strndup(arena, token.start, token.length);
            advance(parser);
            break;
        case TOKEN_NUMBER:
            value.type = JSON_NUMBER;
            {
                char* number_string = arena_strndup(arena, token.start, token.length);
                value.number = atof(number_string);
            }
            advance(parser);
            break;
        case TOKEN_TRUE:
            value.type = JSON_BOOL;
            value.boolean = 1;
            advance(parser);
            break;
        case TOKEN_FALSE:
            value.type = JSON_BOOL;
            value.boolean = 0;
            advance(parser);
            break;
        case TOKEN_NULL:
            value.type = JSON_NULL;
            advance(parser);
            break;
        case TOKEN_LEFT_BRACE:
            parse_object(parser, &value, arena);
            break;
        case TOKEN_LEFT_BRACKET:
            parse_array(parser, &value, arena);
            break;
        default:
            printf("Unexpected token: %.*s\n", (int)token.length, token.start);
            exit(1);
    }
    return value;
}

Token get_current_token(Parser* parser) {
    return parser->tokens[parser->index];
}

void parse_object(Parser* parser, JsonValue* json_object, Arena* arena) {
    json_object->type = JSON_OBJECT;
    json_object->object = NULL;

    advance(parser); // skip "{"

    while (get_current_token(parser).type != TOKEN_RIGHT_BRACE) {
        Token key_token = get_current_token(parser);

        if (key_token.type != TOKEN_STRING) {
            printf("Expected string key but %s found. at %i\n", token_type_to_string(key_token.type), parser->index);
            exit(1);
        }

        char* key = arena_strndup(arena, key_token.start, key_token.length);
        advance(parser);

        if (get_current_token(parser).type != TOKEN_COLON) {
            printf("Expected ':'\n");
            exit(1);
        }

        advance(parser);

        JsonValue value = parse_value(parser, arena);
        JsonValue* heap_value = arena_alloc(arena, sizeof(JsonValue));
        *heap_value = value;

        shput(json_object->object, key, heap_value);

        if (get_current_token(parser).type == TOKEN_COMMA) {
            advance(parser);
        } else if (get_current_token(parser).type != TOKEN_RIGHT_BRACE) {
            printf("Expected ',' or '}'\n");
            exit(1);
        }
    }

    advance(parser); // Skip '}'
}

void parse_array(Parser* parser, JsonValue* json_object, Arena* arena) {
    json_object->type = JSON_ARRAY;
    json_object->array = NULL;

    advance(parser); // Skip '['

    while (get_current_token(parser).type != TOKEN_RIGHT_BRACKET) {
        JsonValue value = parse_value(parser, arena);
        JsonValue* heap_value = arena_alloc(arena, sizeof(JsonValue));
        *heap_value = value;

        arrput(json_object->array, heap_value);

        if (get_current_token(parser).type == TOKEN_COMMA) {
            advance(parser);
        } else if (get_current_token(parser).type != TOKEN_RIGHT_BRACKET) {
            printf("Expected ',' or ']'\n");
            exit(1);
        }
    }

    advance(parser); // Skip ']'
}

JsonValue parse_json(Token* tokens, Arena* arena){
    Parser parser = { tokens, 0 };
    JsonValue json_object;
    if(tokens[0].type == TOKEN_LEFT_BRACE){
        parse_object(&parser, &json_object, arena);
    } else if(tokens[0].type == TOKEN_LEFT_BRACKET){
        parse_array(&parser, &json_object, arena);
    } else {
        printf("error\n");
        exit(1);
    }

    return json_object;
}



char* get_indentation(int depth, int spaces) {
    if (depth < 0) depth = 0;

    int spacesPerDepth = spaces;
    int totalSpaces = depth * spacesPerDepth;

    char* result = (char*)malloc(totalSpaces + 1);
    if (!result) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }

    memset(result, ' ', totalSpaces);
    result[totalSpaces] = '\0';

    return result;
}

void json_print(JsonValue* json, size_t spaces, size_t depth){
    switch(json->type){
        case JSON_OBJECT: {
            printf("{\n");
            depth++;

            JsonPair *pairs = json->object; // hashmap of pairs
            size_t count = hmlen(pairs);

            size_t i = 0;
            for (size_t slot = 0; slot < hmlen(pairs); slot++) {
                if (pairs[slot].key == NULL) continue;

                printf("%s\"%s\": ", get_indentation(depth, spaces), pairs[slot].key);
                json_print(pairs[slot].value, spaces, depth);
                if (i < count - 1) printf(",");
                printf("\n");
                i++;
            }

            depth--;
            printf("%s}", get_indentation(depth, spaces));
            break;
        }
        case JSON_ARRAY: {
            printf("[\n");
            depth++;

            JsonValue** values = json->array; // dynamic array
            size_t count = arrlen(values);

            for(size_t i = 0; i < count; i++){
                printf("%s", get_indentation(depth, spaces));
                json_print(values[i], spaces, depth);
                if(i < count - 1){
                    printf(",");
                }
                printf("\n");
            }

            depth--;
            printf("%s]", get_indentation(depth, spaces));
            break;
        }
        case JSON_STRING: {
            printf("\"%s\"", json->string);
            break;
        }
        case JSON_NUMBER: {
            if (fabs(json->number - (long long int)json->number) < 1e-9) {
                printf("%lld", (long long int)json->number);
            } else {
                printf("%f", json->number);
            }
            break;
        }
        case JSON_BOOL: {
            printf("%s", json->boolean ? "true" : "false");
            break;
        }
        case JSON_NULL: {
            printf("null");
            break;
        }
    }
}

JsonValue* json_get(JsonValue* json, const char* path[], size_t count){
    JsonValue* current = json;
    for(size_t i = 0; i< count; i++){
        if(current->type != JSON_OBJECT){
            printf("Trying to access value as object! at \"%s\"\n", path[i]);
            return NULL;
        }
        JsonValue* found = shget(current->object, path[i]);
        if(!found){
            printf("Key \"%s\" dose not exist in the json!\n", path[i]);
            return NULL;
        }else{
            current = found;
        }
    }
    return current;
}

JsonValue jsonFileLoad(const char* file_name, Arena* arena){
    char* file_content = file_read(file_name);
    Token* tokens = tokenize(file_content);
    JsonValue json = parse_json(tokens, arena);
    arrfree(file_content);
    arrfree(tokens);
    return json;
}

JsonValue jsonStringLoad(char* file_content, Arena* arena){
    Token* tokens = tokenize(file_content);
    JsonValue json = parse_json(tokens, arena);
    arrfree(tokens);
    return json;
}