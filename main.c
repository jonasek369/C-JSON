#include "parser.h"


int main(int argc, char const *argv[])
{
    Arena arena = {0};
    JsonValue json = jsonFileLoad("A:/cjson/new/tests/basic.json", &arena);
    json_print(&json, 4, 0);

	return 0;
}