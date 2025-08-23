#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdalign.h>
#include <stdint.h>
#include <ctype.h>
#include <math.h>

#define STB_DS_IMPLEMENTATION
// #define STBDS_STRING_KEY
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
    long file_size_code = ftell(fp);
    if(file_size_code < 0){
        fprintf(stderr, "Could not determine file size\n");
        fclose(fp);
        return NULL;
    }
    size_t file_size = (size_t)file_size_code;
    rewind(fp);

    char* content = NULL;
    arrsetcap(content, file_size + 1);
    
    size_t read_count = fread(arraddnptr(content, file_size), 1, file_size, fp);
    fclose(fp);
    
    if (read_count != file_size) {
        fprintf(stderr, "Could not read entire file\n");
        arrfree(content);
        return NULL;
    }
    
    arrput(content, '\0'); // appends NUL terminator
    return content;
}

typedef enum {
    TOKEN_EOF=0, TOKEN_ERROR, TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
    TOKEN_LEFT_BRACKET, TOKEN_RIGHT_BRACKET,
    TOKEN_COMMA, TOKEN_COLON,
    TOKEN_STRING, TOKEN_NUMBER,
    TOKEN_TRUE, TOKEN_FALSE, TOKEN_NULL, TOKEN_NOT_INIT
} JsonTokenType;

const char* token_type_to_string(JsonTokenType type) {
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
        case TOKEN_NOT_INIT: return "TOKEN_NOT_INIT";
        default: return "UNKNOWN_TOKEN";
    }
}


typedef struct {
    JsonTokenType type;
    const char *start;
    size_t length;
} Token;

static int utf8_char_len(uint8_t c) {
    if (c < 0x80) return 1;              // ASCII
    else if ((c & 0xE0) == 0xC0) return 2; // 110xxxxx
    else if ((c & 0xF0) == 0xE0) return 3; // 1110xxxx
    else if ((c & 0xF8) == 0xF0) return 4; // 11110xxx
    else return 1;
}


void parse_string(char* json, size_t* index, Token* tok) {
    size_t start = ++(*index);

    while (json[*index] != '"' && json[*index] != '\0') {
        if (json[*index] == '\\' && json[*index + 1] != '\0') {
            (*index) += 2;
        } else {
            int step = utf8_char_len((uint8_t)json[*index]);
            (*index) += step;
        }
    }

    tok->start = &json[start];
    tok->length = (*index) - start;
    tok->type = TOKEN_STRING;

    if (json[*index] == '"') {
        (*index)++;
    } else {
        tok->type = TOKEN_ERROR;
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

static int is_json_delim(char c) {
    return c == '\0' || c == ',' || c == ']' || c == '}' || isspace((uint8_t)c);
}

void parse_keyword(char* json, size_t* index, Token* tok) {
    char* ptr = &json[*index];

    if (strncmp(ptr, "true", 4) == 0 && is_json_delim(ptr[4])) {
        tok->type = TOKEN_TRUE;
        tok->length = 4;
        (*index) += 4;
    } else if (strncmp(ptr, "false", 5) == 0 && is_json_delim(ptr[5])) {
        tok->type = TOKEN_FALSE;
        tok->length = 5;
        (*index) += 5;
    } else if (strncmp(ptr, "null", 4) == 0 && is_json_delim(ptr[4])) {
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
    while (is_whitespace[(uint8_t)json[*index]]) {
        (*index)++;
    }

    char c = json[*index];

    Token tok;
    tok.type = TOKEN_NOT_INIT;
    tok.start = &json[*index];
    tok.length = 1;

    if (isdigit(c)) {
        parse_number(json, index, &tok);
        return tok;
    }

    switch (c) {
        case '{': tok.type = TOKEN_LEFT_BRACE;    (*index)++; break;
        case '}': tok.type = TOKEN_RIGHT_BRACE;   (*index)++; break;
        case '[': tok.type = TOKEN_LEFT_BRACKET;  (*index)++; break;
        case ']': tok.type = TOKEN_RIGHT_BRACKET; (*index)++; break;
        case ':': tok.type = TOKEN_COLON;         (*index)++; break;
        case ',': tok.type = TOKEN_COMMA;         (*index)++; break;
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
            tok.length = 1;
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
        if (tok.type == TOKEN_ERROR) {
            printf("Hit error at token %li. At file_content index %li\n",
                   (long int)arrlenu(tokens), (long int)index);
            break;
        }
        if (tok.type == TOKEN_EOF) {
            arrput(tokens, tok);
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
        bool boolean;
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
    if (parser->tokens[parser->index].type != TOKEN_EOF) {
        parser->index++;
    }
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

            char* number_string = arena_strndup(arena, token.start, token.length);
            value.number = atof(number_string);
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

            printf("Unexpected token: %.*s type=%u\n", (int)token.length, token.start, token.type);
            exit(1);
    }
    return value;
}

Token get_current_token(Parser* parser) {
    size_t len = arrlenu(parser->tokens);
    if (parser->index >= len) {
        printf("Parser index %lu out of bounds (len=%lu)\n", (long int)parser->index, (long int)len);
        Token t = { .type = TOKEN_EOF, .start = NULL, .length = 0 };
        return t;
    }
    return parser->tokens[parser->index];
}

void parse_object(Parser* parser, JsonValue* json_object, Arena* arena) {
    json_object->type = JSON_OBJECT;
    json_object->object = NULL;

    advance(parser); // skip "{"

    while (get_current_token(parser).type != TOKEN_RIGHT_BRACE) {
        Token key_token = get_current_token(parser);

        if (key_token.type != TOKEN_STRING) {
            printf("Expected string key but %s found. at %lu\n", token_type_to_string(key_token.type), (long int)parser->index);
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

void parse_json(Token* tokens, Arena* arena, JsonValue* output) {
    Parser parser = { tokens, 0 };

    if (tokens[0].type == TOKEN_LEFT_BRACE) {
        parse_object(&parser, output, arena);
    } else if (tokens[0].type == TOKEN_LEFT_BRACKET) {
        parse_array(&parser, output, arena);
    } else {
        printf("error: root must be object or array\n");
        exit(1);
    }

    // After parsing, we should be at EOF
    if (get_current_token(&parser).type != TOKEN_EOF) {
        printf("warning: extra tokens after root JSON value at index %lu\n", (long int)parser.index);
    }
}



static void get_indentation(int depth, int spaces, char *buffer, size_t bufsize) {
    if (depth < 0) depth = 0;

    int totalSpaces = depth * spaces;
    if ((size_t)totalSpaces >= bufsize) {
        totalSpaces = (int)bufsize - 1; // prevent overflow
    }

    memset(buffer, ' ', totalSpaces);
    buffer[totalSpaces] = '\0';
}

void json_print(JsonValue* json, size_t spaces, size_t depth) {
    char indent[512]; // max indentation size — adjust as needed

    switch (json->type) {
        case JSON_OBJECT: {
            printf("{\n");
            depth++;

            JsonPair *pairs = json->object; // hashmap of pairs
            size_t count = hmlenu(pairs);

            size_t i = 0;
            for (size_t slot = 0; slot < hmlenu(pairs); slot++) {
                if (pairs[slot].key == NULL) continue;

                get_indentation(depth, spaces, indent, sizeof(indent));
                printf("%s\"%s\": ", indent, pairs[slot].key);
                json_print(pairs[slot].value, spaces, depth);
                if (i < count - 1) printf(",");
                printf("\n");
                i++;
            }

            depth--;
            get_indentation(depth, spaces, indent, sizeof(indent));
            printf("%s}", indent);
            break;
        }
        case JSON_ARRAY: {
            printf("[\n");
            depth++;

            JsonValue** values = json->array; // dynamic array
            size_t count = arrlen(values);

            for (size_t i = 0; i < count; i++) {
                get_indentation(depth, spaces, indent, sizeof(indent));
                printf("%s", indent);
                json_print(values[i], spaces, depth);
                if (i < count - 1) {
                    printf(",");
                }
                printf("\n");
            }

            depth--;
            get_indentation(depth, spaces, indent, sizeof(indent));
            printf("%s]", indent);
            break;
        }
        case JSON_STRING: {
            printf("\"%s\"", json->string);
            break;
        }
        case JSON_NUMBER: {
            printf("%g", json->number);
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
    if(depth == 0){
        printf("\n");
    }
}

void json_free(JsonValue *value) {
    if (!value) return;

    switch (value->type) {
        case JSON_ARRAY:
            if (value->array) {
                for (size_t i = 0; i < arrlenu(value->array); i++) {
                    json_free(value->array[i]);
                }
                arrfree(value->array);
            }
            break;

        case JSON_OBJECT: {
            for(size_t i = 0; i < shlenu(value->object); i++){
                json_free(value->object[i].value);
            }
            shfree(value->object);
            break;
        }

        case JSON_STRING:
        case JSON_NUMBER:
        case JSON_BOOL:
            // Strings are arena allocated (number and bool arent on heap)
            break;

        default:
            break;
    }
}

void jsonFileLoad(const char* file_name, Arena* arena, JsonValue* output){
    char* file_content = file_read(file_name);
    if(!file_content){
        return;
    }
    Token* tokens = tokenize(file_content);
    parse_json(tokens, arena, output);
    arrfree(file_content);
    arrfree(tokens);
}

void jsonStringLoad(char* json_string, Arena* arena, JsonValue* output){
    Token* tokens = tokenize(json_string);
    parse_json(tokens, arena, output);
    arrfree(tokens);
}

void json_init_object(JsonValue* json){
    json->type = JSON_OBJECT;
}

void json_add_child(JsonValue* json, char* key, JsonValue* child){
    if(!child){
        fprintf(stderr, "Tryng to add child that NULL\n");
        return;
    }
    if(json->type == JSON_OBJECT){
        if(key == NULL){
            fprintf(stderr, "Cannot put NULL key into object\n");
            return;
        }
        shput(json->object, key, child);
    }else if(json->type == JSON_ARRAY){
        if(key != NULL){
            fprintf(stderr, "Warn: Trying to add key to array\n");
        }
        arrput(json->array, child);
    }else{
        fprintf(stderr, "Cannot add child to %s\n", token_type_to_string(json->type));
    }
}

bool json_remove_child(JsonValue* json, JsonValue key){
    if(key.type == JSON_STRING){
        if(json->type != JSON_OBJECT){
            fprintf(stderr, "Cannot remove string key from non object json!\n");
            return 1;
        }
        return shdel(json->object, key.string);
    }else if(key.type == JSON_NUMBER){
        if(json->type != JSON_ARRAY){
            fprintf(stderr, "Cannot remove number key from non array json!\n");
            return 1;
        }
        arrdel(json->array, (size_t)key.number);
        return 0;
    }
    fprintf(stderr, "Cannot remove key from non array or object json!\n");
    return 1;
}


bool json_remove_key(JsonValue* json, const char* key){
    if(json->type != JSON_OBJECT){
        fprintf(stderr, "Cannot remove string key from non object json!\n");
        return 1;
    }
    return shdel(json->object, key);
}

bool json_remove_at(JsonValue* json, size_t index){
    if(json->type != JSON_ARRAY){
        fprintf(stderr, "Cannot remove number key from non array json!\n");
        return 1;
    }
    arrdel(json->array, index);
    return 0;
}


JsonValue* json_new_string(Arena* arena, const char* str){
    char* copy = arena_strndup(arena, str, strlen(str));
    if(copy == NULL){
        fprintf(stderr, "Could not duplicate string\n");
        return NULL;
    }
    JsonValue* json_string = arena_alloc(arena, sizeof(JsonValue));
    json_string->type = JSON_STRING;
    json_string->string = copy;
    return json_string;
}

JsonValue* json_new_number(Arena* arena, double num){
    JsonValue* json_num = arena_alloc(arena, sizeof(JsonValue));
    json_num->type = JSON_NUMBER;
    json_num->number = num;
    return json_num;
}

JsonValue* json_new_bool(Arena* arena, bool value){
    JsonValue* json_bool = arena_alloc(arena, sizeof(JsonValue));
    json_bool->type = JSON_BOOL;
    json_bool->boolean = value;
    return json_bool;
}

JsonValue* json_new_null(Arena* arena){
    JsonValue* json_null = arena_alloc(arena, sizeof(JsonValue));
    json_null->type = JSON_NULL;
    return json_null;
}

#define STR_ARR(...) (const char*[]){__VA_ARGS__}, \
                     sizeof((const char*[]){__VA_ARGS__}) / sizeof(const char*)

#define NUM_ARR(...) (double[]){__VA_ARGS__}, \
                     sizeof((double[]){__VA_ARGS__}) / sizeof(double)




JsonValue* json_new_sarray(Arena* arena, const char** items, size_t length){
    JsonValue* json_array = arena_alloc(arena, sizeof(JsonValue));
    json_array->type = JSON_ARRAY;
    json_array->array = NULL;
    for(size_t i = 0; i < length; i++){
        const char* item = items[i];
        JsonValue* json_string = json_new_string(arena, item);
        json_add_child(json_array, NULL, json_string);
    }
    return json_array;
}

JsonValue* json_new_narray(Arena* arena, const double* items, size_t length){
    JsonValue* json_array = arena_alloc(arena, sizeof(JsonValue));
    json_array->type = JSON_ARRAY;
    json_array->array = NULL;
    for(size_t i = 0; i < length; i++){
        double item = items[i];
        JsonValue* json_number = json_new_number(arena, item);
        json_add_child(json_array, NULL, json_number);
    }
    return json_array;
}