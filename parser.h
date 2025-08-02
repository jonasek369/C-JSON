#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>

#include "ds.h"

typedef enum {
    JSON_NULL, JSON_BOOL, JSON_NUMBER, JSON_STRING,
    JSON_ARRAY, JSON_OBJECT
} JsonType;


list* file_read(const char* file_name){
    FILE* fp = fopen(file_name, "r");
    if(!fp){
        fprintf(stderr, "Could not read file");
        return NULL;
    }
    list* content = new_list(sizeof(char), 1024);
    if(!content){
        fprintf(stderr, "Could not create list");
        return NULL;
    }
    int c;
    while((c = fgetc(fp)) != EOF){
        push_char(content, (char)c);
    }
    push_char(content, '\0');
    fclose(fp);
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

bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

void parse_number(char* json, size_t* index, Token* tok) {
    size_t start = *index;

    // Handle optional leading minus
    if (json[*index] == '-') {
        if (!isdigit(json[*index + 1])) {
            tok->type = TOKEN_ERROR;
            tok->length = 1;  // just the '-'
            (*index)++;       // consume '-'
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

Token* next_token(char* json, size_t* index) {
    while (json[*index] == ' ' || json[*index] == '\n' || json[*index] == '\t' || json[*index] == '\r') {
        (*index)++;
    }

    char c = json[*index];

    Token* tok = malloc(sizeof(Token));
    tok->start = &json[*index];
    tok->length = 1;

    if (isdigit(c)) {
        parse_number(json, index, tok);
        return tok;
    }

    switch (c) {
        case '{': tok->type = TOKEN_LEFT_BRACE; (*index)++; break;
        case '}': tok->type = TOKEN_RIGHT_BRACE; (*index)++; break;
        case '[': tok->type = TOKEN_LEFT_BRACKET; (*index)++; break;
        case ']': tok->type = TOKEN_RIGHT_BRACKET; (*index)++; break;
        case ':': tok->type = TOKEN_COLON; (*index)++; break;
        case ',': tok->type = TOKEN_COMMA; (*index)++; break;
        case '"': {
            parse_string(json, index, tok);
            return tok;
        }
        case 't':
        case 'f':
        case 'n': {
            parse_keyword(json, index, tok);
            return tok;
        }
        case '\0':
            tok->type = TOKEN_EOF;
            tok->length = 0;
            return tok;
        default:
            tok->type = TOKEN_ERROR;
            return tok;
    }

    return tok;
}


void push_token(list* l, Token* tok){
    if(l == NULL){
        fprintf(stderr, "Null list passed!\n");
        return;
    }
    if(l->size+1 > l->capacity){
        if(realloc_list(l) != 0){
            return;
        }
    }
    ((Token**)l->data)[l->size] = tok;
    l->size++;
}

Token* get_token(list* l, size_t index){
    if(index >= l->size){
        fprintf(stderr, "Out of bounds error!\n");
        return NULL;    
    }
    return ((Token**)l->data)[index];
}

list* tokenize(list* file_content){
    list* tokens = new_list(sizeof(Token*), 128);
    size_t index = 0;
    while (true) {
        Token* tok = next_token((char*)file_content->data, &index);
        if (!tok || tok->type == TOKEN_EOF || tok->type == TOKEN_ERROR) {
            free(tok);
            break;
        }
        push_token(tokens, tok);
    }
    return tokens;
}

typedef struct JsonValue JsonValue;

typedef struct {
    char *key;
    JsonValue *value;
} JsonPair;

struct JsonValue {
    JsonType type;
    union {
        double number;
        char *string;
        int boolean;
        list* array;  // list of JsonValue
        list* object; // list of JsonPair
    };
};



void push_pair(list* l, JsonPair* pair){
    if(l == NULL){
        fprintf(stderr, "Null list passed!\n");
        return;
    }
    if(l->size+1 > l->capacity){
        if(realloc_list(l) != 0){
            return;
        }
    }
    ((JsonPair**)l->data)[l->size] = pair;
    l->size++;
}

JsonPair* get_pair(list* l, size_t index){
    if(index >= l->size){
        fprintf(stderr, "Out of bounds error!\n");
        return NULL;    
    }
    return ((JsonPair**)l->data)[index];
}

void push_value(list* l, JsonValue* value){
    if(l == NULL){
        fprintf(stderr, "Null list passed!\n");
        return;
    }
    if(l->size+1 > l->capacity){
        if(realloc_list(l) != 0){
            return;
        }
    }
    ((JsonValue**)l->data)[l->size] = value;
    l->size++;
}

JsonValue* get_value(list* l, size_t index){
    if(index >= l->size){
        fprintf(stderr, "Out of bounds error!\n");
        return NULL;    
    }
    return ((JsonValue**)l->data)[index];
}

char* custom_strndup(const char* str1, size_t length) {
    size_t copy_len = strnlen(str1, length);
    char* new_str = (char*)malloc(copy_len + 1);
    if (!new_str) return NULL;
    memcpy(new_str, str1, copy_len);
    new_str[copy_len] = '\0';
    return new_str;
}

typedef struct {
    list* tokens;
    size_t index;
} Parser;


Token* get_current_token(Parser* parser) {
    return get_token(parser->tokens, parser->index);
}

void advance(Parser* parser) {
    parser->index++;
}

void parse_object(Parser* parser, JsonValue* json_object);
void parse_array(Parser* parser, JsonValue* json_object);


JsonValue* parse_value(Parser* parser){
    Token* token = get_current_token(parser);
    JsonValue* value = malloc(sizeof(JsonValue));
    switch(token->type){
        case TOKEN_STRING:
            value->type = JSON_STRING;
            value->string = custom_strndup(token->start, token->length);
            advance(parser);
            break;
        case TOKEN_NUMBER:
            value->type = JSON_NUMBER;
            char* number_string = custom_strndup(token->start, token->length);
            value->number = atof(number_string);
            free(number_string);
            advance(parser);
            break;
        case TOKEN_TRUE:
            value->type = JSON_BOOL;
            value->boolean = 1;
            advance(parser);
            break;
        case TOKEN_FALSE:
            value->type = JSON_BOOL;
            value->boolean = 0;
            advance(parser);
            break;
        case TOKEN_NULL:
            value->type = JSON_NULL;
            advance(parser);
            break;
        case TOKEN_LEFT_BRACE:
            parse_object(parser, value);
            break;
        case TOKEN_LEFT_BRACKET:
            parse_array(parser, value);
            break;
        default:
            printf("Unexpected token: %.*s\n", token->length, token->start);
            exit(1);
    }
    return value;
}

void parse_object(Parser* parser, JsonValue* json_object) {
    json_object->type = JSON_OBJECT;
    json_object->object = new_list(sizeof(JsonPair*), 8);

    advance(parser); // skip "{"

    while (get_current_token(parser)->type != TOKEN_RIGHT_BRACE) {
        Token* key_token = get_current_token(parser);
        if (key_token->type != TOKEN_STRING) {
            printf("Expected string key\n");
            exit(1);
        }

        char* key = custom_strndup(key_token->start, key_token->length);
        advance(parser);

        if (get_token(parser->tokens, parser->index)->type != TOKEN_COLON) {
            printf("Expected ':'\n");
            exit(1);
        }

        advance(parser);

        JsonValue* value = parse_value(parser);

        JsonPair* pair = malloc(sizeof(JsonPair));
        pair->key = key;
        pair->value = value;

        push_pair(json_object->object, pair);

        if (get_current_token(parser)->type == TOKEN_COMMA) {
            advance(parser);
        } else if (get_current_token(parser)->type != TOKEN_RIGHT_BRACE) {
            printf("Expected ',' or '}'\n");
            exit(1);
        }
    }

    advance(parser); // Skip '}'
}

void parse_array(Parser* parser, JsonValue* json_object) {
    json_object->type = JSON_ARRAY;
    json_object->array = new_list(sizeof(JsonValue*), 8); // Assuming a list creation function

    advance(parser); // Skip '['

    while (get_current_token(parser)->type != TOKEN_RIGHT_BRACKET) {
        JsonValue* value = parse_value(parser);
        push_value(json_object->array, value);

        if (get_current_token(parser)->type == TOKEN_COMMA) {
            advance(parser);
        } else if (get_current_token(parser)->type != TOKEN_RIGHT_BRACKET) {
            printf("Expected ',' or ']'\n");
            exit(1);
        }
    }

    advance(parser); // Skip ']'
}

JsonValue* parse_json(list* tokens){
    Parser parser;
    parser.tokens = tokens;
    parser.index = 0;
    JsonValue* json_object = malloc(sizeof(JsonValue));
    if(get_token(parser.tokens, 0)->type == TOKEN_LEFT_BRACE){
        parse_object(&parser, json_object);
    }else if(get_token(parser.tokens, 0)->type == TOKEN_LEFT_BRACKET){
        parse_array(&parser, json_object);
    }else{
        printf("error");
    }

    return json_object;
}

JsonValue* load_file(const char* file_name){
    list* file_content = file_read(file_name);
    list* tokens = tokenize(file_content);
    JsonValue* json = parse_json(tokens);
    free_list(file_content);
    free_list(tokens);
    return json;
}