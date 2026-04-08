#ifndef _H_CJSON
#define _H_CJSON

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdalign.h>
#include <stdint.h>
#include <ctype.h>
#include <math.h>
#include <stdarg.h>


#ifndef CJSON_NO_STB_DS
    #define STB_DS_IMPLEMENTATION
    // #define STBDS_STRING_KEY
    #include "stb_ds.h"
#endif

typedef enum {
    JSON_NULL, JSON_BOOL, JSON_NUMBER, JSON_STRING,
    JSON_ARRAY, JSON_OBJECT
} JsonTypeConstants;

typedef uint8_t JsonType;


static const bool cjson_is_whitespace[256] = {
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
    TOKEN_CJSON_STRING, TOKEN_NUMBER,
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
        case TOKEN_CJSON_STRING: return "TOKEN_STRING";
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
} CjsonToken;

static int utf8_char_len(uint8_t c) {
    if (c < 0x80) return 1;              // ASCII
    else if ((c & 0xE0) == 0xC0) return 2; // 110xxxxx
    else if ((c & 0xF0) == 0xE0) return 3; // 1110xxxx
    else if ((c & 0xF8) == 0xF0) return 4; // 11110xxx
    else return 1;
}


void cjson_parse_string(char* json, size_t* index, CjsonToken* tok) {
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
    tok->type = TOKEN_CJSON_STRING;

    if (json[*index] == '"') {
        (*index)++;
    } else {
        tok->type = TOKEN_ERROR;
    }
}


void cjson_parse_number(char* json, size_t* index, CjsonToken* tok) {
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

void parse_keyword(char* json, size_t* index, CjsonToken* tok) {
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


CjsonToken cjson_next_token(char* json, size_t* index) {
    while (cjson_is_whitespace[(uint8_t)json[*index]]) {
        (*index)++;
    }

    char c = json[*index];

    CjsonToken tok;
    tok.type = TOKEN_NOT_INIT;
    tok.start = &json[*index];
    tok.length = 1;

    if (isdigit(c) || c == '-') {
        cjson_parse_number(json, index, &tok);
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
            cjson_parse_string(json, index, &tok);
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


CjsonToken* tokenize(char* file_content){
    CjsonToken* tokens = NULL;
    size_t index = 0;

    while (true) {
        CjsonToken tok = cjson_next_token(file_content, &index);
        if (tok.type == TOKEN_ERROR) {
            printf("Hit error at token %li. At file_content index %li\n",
                   (long int)arrlenu(tokens), (long int)index);
            printf("%s", file_content);
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

typedef struct {
    CjsonToken* tokens;
    size_t index;
} Parser;


void advance(Parser* parser) {
    if (parser->tokens[parser->index].type != TOKEN_EOF) {
        parser->index++;
    }
}

void parse_object(Parser* parser, JsonValue* json_object);
void parse_array(Parser* parser, JsonValue* json_object);


JsonValue parse_value(Parser* parser){
    CjsonToken token = parser->tokens[parser->index];

    JsonValue value;

    switch(token.type){
        case TOKEN_CJSON_STRING:
            value.type = JSON_STRING;
            value.string = malloc((token.length+1)*sizeof(char));
            memcpy(value.string, token.start, token.length);
            value.string[token.length] = '\0';
            advance(parser);
            break;
        case TOKEN_NUMBER:
            value.type = JSON_NUMBER;
            char* number_string = malloc((token.length+1)*sizeof(char));
            memcpy(number_string, token.start, token.length);
            number_string[token.length] = '\0';
            value.number = atof(number_string);
            free(number_string);
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
            parse_object(parser, &value);
            break;
        case TOKEN_LEFT_BRACKET:
            parse_array(parser, &value);
            break;
        default:
            printf("Unexpected token: %.*s type=%u\n", (int)token.length, token.start, token.type);
            exit(1);
    }
    return value;
}

CjsonToken get_current_token(Parser* parser) {
    size_t len = arrlenu(parser->tokens);
    if (parser->index >= len) {
        printf("Parser index %lu out of bounds (len=%lu)\n", (long int)parser->index, (long int)len);
        CjsonToken t = { .type = TOKEN_EOF, .start = NULL, .length = 0 };
        return t;
    }
    return parser->tokens[parser->index];
}

void parse_object(Parser* parser, JsonValue* json_object) {
    json_object->type = JSON_OBJECT;
    json_object->object = NULL;

    advance(parser); // skip "{"

    while (get_current_token(parser).type != TOKEN_RIGHT_BRACE) {
        CjsonToken key_token = get_current_token(parser);

        if (key_token.type != TOKEN_CJSON_STRING) {
            printf("Expected string key but %s found. at %lu\n", token_type_to_string(key_token.type), (long int)parser->index);
            exit(1);
        }

        char* key = malloc((key_token.length+1)*sizeof(char));
        memcpy(key, key_token.start, key_token.length);
        key[key_token.length] = '\0';
        advance(parser);

        if (get_current_token(parser).type != TOKEN_COLON) {
            printf("Expected ':'\n");
            exit(1);
        }

        advance(parser);

        JsonValue value = parse_value(parser);
        JsonValue* heap_value = malloc(sizeof(JsonValue));
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

void parse_array(Parser* parser, JsonValue* json_object) {
    json_object->type = JSON_ARRAY;
    json_object->array = NULL;

    advance(parser); // Skip '['

    while (get_current_token(parser).type != TOKEN_RIGHT_BRACKET) {
        JsonValue value = parse_value(parser);
        JsonValue* heap_value = malloc(sizeof(JsonValue));
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

void parse_json(CjsonToken* tokens, JsonValue* output) {
    Parser parser = { tokens, 0 };

    if (tokens[0].type == TOKEN_LEFT_BRACE) {
        parse_object(&parser, output);
    } else if (tokens[0].type == TOKEN_LEFT_BRACKET) {
        parse_array(&parser, output);
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


static void sb_append(char **buf, const char *fmt, ...) {
    char temp[1024];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(temp, sizeof(temp), fmt, args);
    va_end(args);

    if (n < 0) return;

    if ((size_t)n >= sizeof(temp)) {
        char *heapbuf = malloc(n + 1);
        va_start(args, fmt);
        vsnprintf(heapbuf, n + 1, fmt, args);
        va_end(args);
        for (int i = 0; i < n; i++)
            arrpush(*buf, heapbuf[i]);
        free(heapbuf);
    } else {
        for (int i = 0; i < n; i++)
            arrpush(*buf, temp[i]);
    }
}


void json_dump(JsonValue *json, char **out) {
    switch (json->type) {
        case JSON_OBJECT: {
            sb_append(out, "{");
            JsonPair *pairs = json->object;
            size_t first = 1;
            for (size_t slot = 0; slot < hmlenu(pairs); slot++) {
                if (pairs[slot].key == NULL) continue;
                if (!first) sb_append(out, ",");
                first = 0;
                sb_append(out, "\"%s\":", pairs[slot].key);
                json_dump(pairs[slot].value, out);
            }
            sb_append(out, "}");
            break;
        }

        case JSON_ARRAY: {
            sb_append(out, "[");
            JsonValue **values = json->array;
            size_t count = arrlen(values);
            for (size_t i = 0; i < count; i++) {
                if (i > 0) sb_append(out, ",");
                json_dump(values[i], out);
            }
            sb_append(out, "]");
            break;
        }

        case JSON_STRING:
            sb_append(out, "\"%s\"", json->string);
            break;
        case JSON_NUMBER:
            sb_append(out, "%g", json->number);
            break;
        case JSON_BOOL:
            sb_append(out, "%s", json->boolean ? "true" : "false");
            break;
        case JSON_NULL:
            sb_append(out, "null");
            break;
    }
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

        case JSON_OBJECT:
            if(value->object){
                for (size_t i = 0; i < shlenu(value->object); i++) {
                    free(value->object[i].key);
                    json_free(value->object[i].value);
                }
                shfree(value->object);
            }
            break;

        case JSON_STRING:
            free(value->string);
            break;

        case JSON_NUMBER:
        case JSON_BOOL:
        case JSON_NULL:
            break;
    }

    free(value);
}

char *json_escape(const char *input) {
    if (!input) return NULL;

    size_t len = strlen(input);
    char *escaped = malloc(len * 6 + 1);
    if (!escaped) return NULL;

    char *p = escaped;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = input[i];
        switch (c) {
            case '\"': *p++ = '\\'; *p++ = '\"'; break;
            case '\\': *p++ = '\\'; *p++ = '\\'; break;
            case '\b': *p++ = '\\'; *p++ = 'b';  break;
            case '\f': *p++ = '\\'; *p++ = 'f';  break;
            case '\n': *p++ = '\\'; *p++ = 'n';  break;
            case '\r': *p++ = '\\'; *p++ = 'r';  break;
            case '\t': *p++ = '\\'; *p++ = 't';  break;
            default:
                if (c < 0x20) {
                    sprintf(p, "\\u%04x", c);
                    p += 6;
                } else {
                    *p++ = c;
                }
                break;
        }
    }
    *p = '\0';
    return escaped;
}

static char hex_to_char(const char *hex) {
    int value = 0;
    for (int i = 0; i < 4; i++) {
        value <<= 4;
        if (hex[i] >= '0' && hex[i] <= '9') value += hex[i] - '0';
        else if (hex[i] >= 'a' && hex[i] <= 'f') value += hex[i] - 'a' + 10;
        else if (hex[i] >= 'A' && hex[i] <= 'F') value += hex[i] - 'A' + 10;
        else return '?'; // invalid hex
    }
    return (char)value;
}

char *json_unescape(const char *input) {
    if (!input) return NULL;

    size_t len = strlen(input);
    char *unescaped = malloc(len + 1);
    if (!unescaped) return NULL;

    char *p = unescaped;
    for (size_t i = 0; i < len; i++) {
        if (input[i] == '\\') {
            i++;
            if (i >= len) break;
            switch (input[i]) {
                case '\"': *p++ = '\"'; break;
                case '\\': *p++ = '\\'; break;
                case '/':  *p++ = '/';  break;
                case 'b':  *p++ = '\b'; break;
                case 'f':  *p++ = '\f'; break;
                case 'n':  *p++ = '\n'; break;
                case 'r':  *p++ = '\r'; break;
                case 't':  *p++ = '\t'; break;
                case 'u':
                    if (i + 4 < len) {
                        *p++ = hex_to_char(&input[i + 1]);
                        i += 4;
                    }
                    break;
                default:
                    *p++ = input[i];
                    break;
            }
        } else {
            *p++ = input[i];
        }
    }
    *p = '\0';
    return unescaped;
}

void jsonFileLoad(const char* file_name, JsonValue* output){
    char* file_content = file_read(file_name);
    if(!file_content){
        return;
    }
    CjsonToken* tokens = tokenize(file_content);
    parse_json(tokens, output);
    arrfree(file_content);
    arrfree(tokens);
}

void jsonStringLoad(char* json_string, JsonValue* output){
    CjsonToken* tokens = tokenize(json_string);
    parse_json(tokens, output);
    arrfree(tokens);
}

void json_init_object(JsonValue* json){
    json->type = JSON_OBJECT;
    json->object = NULL;
}

void json_init_array(JsonValue* json){
    json->type = JSON_ARRAY;
    json->array = NULL;
}

void json_add_child(JsonValue* json, const char* key, JsonValue* child) {
    if (!child) {
        fprintf(stderr, "Trying to add child that is NULL\n");
        return;
    }
    if (json->type == JSON_OBJECT) {
        if (key == NULL) {
            fprintf(stderr, "Cannot put NULL key into object\n");
            return;
        }
        char* key_copy = malloc((strlen(key) + 1) * sizeof(char));
        memcpy(key_copy, key, strlen(key) + 1);
        shput(json->object, key_copy, child);
    } else if (json->type == JSON_ARRAY) {
        if (key != NULL) {
            fprintf(stderr, "Warn: Trying to add key to array\n");
        }
        arrput(json->array, child);
    } else {
        fprintf(stderr, "Cannot add child to non-object/array\n");
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


JsonValue* json_new_string(const char* str){
    size_t len = strlen(str);
    char* copy = malloc((len+1)*sizeof(char));

    if(copy == NULL){
        fprintf(stderr, "Could not duplicate string\n");
        return NULL;
    }
    memcpy(copy, str, len);
    copy[len] = '\0';

    JsonValue* json_string = malloc(sizeof(JsonValue));
    json_string->type = JSON_STRING;
    json_string->string = copy;
    return json_string;
}

JsonValue* json_new_nstring(const char* str, size_t n) {
    char* copy = malloc((n+1)*sizeof(char));

    if (copy == NULL) {
        fprintf(stderr, "Could not allocate memory for string\n");
        return NULL;
    }

    memcpy(copy, str, n);
    copy[n] = '\0';

    JsonValue* json_string = malloc(sizeof(JsonValue));
    if (json_string == NULL) {
        fprintf(stderr, "Could not allocate memory for JsonValue\n");
        return NULL;
    }

    json_string->type = JSON_STRING;
    json_string->string = copy;
    return json_string;
}

// TODO: Remove arena

JsonValue* json_new_number(double num){
    JsonValue* json_num = malloc(sizeof(JsonValue));
    json_num->type = JSON_NUMBER;
    json_num->number = num;
    return json_num;
}

JsonValue* json_new_bool(bool value){
    JsonValue* json_bool = malloc(sizeof(JsonValue));
    json_bool->type = JSON_BOOL;
    json_bool->boolean = value;
    return json_bool;
}

JsonValue* json_new_null(){
    JsonValue* json_null = malloc(sizeof(JsonValue));
    json_null->type = JSON_NULL;
    return json_null;
}

#define STR_ARR(...) (const char*[]){__VA_ARGS__}, \
                     sizeof((const char*[]){__VA_ARGS__}) / sizeof(const char*)

#define NUM_ARR(...) (double[]){__VA_ARGS__}, \
                     sizeof((double[]){__VA_ARGS__}) / sizeof(double)




JsonValue* json_new_sarray(const char** items, size_t length){
    JsonValue* json_array = malloc(sizeof(JsonValue));
    json_array->type = JSON_ARRAY;
    json_array->array = NULL;
    for(size_t i = 0; i < length; i++){
        const char* item = items[i];
        JsonValue* json_string = json_new_string(item);
        json_add_child(json_array, NULL, json_string);
    }
    return json_array;
}

JsonValue* json_new_narray(const double* items, size_t length){
    JsonValue* json_array = malloc(sizeof(JsonValue));
    json_array->type = JSON_ARRAY;
    json_array->array = NULL;
    for(size_t i = 0; i < length; i++){
        double item = items[i];
        JsonValue* json_number = json_new_number(item);
        json_add_child(json_array, NULL, json_number);
    }
    return json_array;
}

JsonValue* json_new_object(){
    JsonValue* json_object = malloc(sizeof(JsonValue));
    json_object->type = JSON_OBJECT;
    json_object->object = NULL;
    return json_object;
}

JsonValue* json_new_array(){
    JsonValue* json_array = malloc(sizeof(JsonValue));
    json_array->type = JSON_ARRAY;
    json_array->array = NULL;
    return json_array;
}

#endif