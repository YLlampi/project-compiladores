#define VARIABLE_TYPE char

typedef VARIABLE_TYPE (*peek_func)(VARIABLE_TYPE);

typedef void (*poke_func)(VARIABLE_TYPE, VARIABLE_TYPE);

void basic_init(const char *program);

void basic_run(void);

int basic_finished(void);

VARIABLE_TYPE basic_get_variable(int varnum);

void basic_set_variable(int varum, VARIABLE_TYPE value);
