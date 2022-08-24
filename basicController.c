#define DEBUG 0

#if DEBUG
#define DEBUG_PRINTF(...)  printf(__VA_ARGS__)
#else
#define DEBUG_PRINTF(...)
#endif

#include "basicController.h"
#include "tokens.h"

#include <stdio.h>
#include <stdlib.h>

static char const *programa_ptr;
#define MAX_STRINGLEN 40
static char string[MAX_STRINGLEN];

#define MAX_GOSUB_STACK_DEPTH 10
static int gosub_stack[MAX_GOSUB_STACK_DEPTH];
static int gosub_stack_ptr;

struct for_state {
    int line_after_for;
    int for_variable;
    int to;
};
#define MAX_FOR_STACK_DEPTH 4
static struct for_state for_stack[MAX_FOR_STACK_DEPTH];
static int for_stack_ptr;

struct line_index {
    int line_number;
    char const *programa_text_position;
    struct line_index *next;
};
struct line_index *line_index_head = NULL;
struct line_index *line_index_current = NULL;

#define MAX_VARNUM 26
static VARIABLE_TYPE variables[MAX_VARNUM];

static int ended;

static VARIABLE_TYPE expr(void);

static void line_statement(void);

static void statement(void);

static void index_free(void);

peek_func peek_function = NULL;
poke_func poke_function = NULL;

void basic_init(const char *programa) {
    programa_ptr = programa;
    for_stack_ptr = gosub_stack_ptr = 0;
    index_free();
    tokenizer_init(programa);
    ended = 0;
}

void basic_init_peek_poke(const char *programa, peek_func peek, poke_func poke) {
    programa_ptr = programa;
    for_stack_ptr = gosub_stack_ptr = 0;
    index_free();
    peek_function = peek;
    poke_function = poke;
    tokenizer_init(programa);
    ended = 0;
}


static void accept(int token) {
    if (token != tokenizer_token()) {
        tokenizer_error_print();
        exit(1);
    }
    DEBUG_PRINTF("Expected %d, got it\n", token);
    tokenizer_next();
}


static int varfactor(void) {
    int r;
    r = basic_get_variable(tokenizer_variable_num());
    accept(TOKENIZER_VARIABLE);
    return r;
}


static int factor(void) {
    int r;

    switch (tokenizer_token()) {
        case TOKENIZER_NUMBER:
            r = tokenizer_num();
            accept(TOKENIZER_NUMBER);
            break;
        case TOKENIZER_LEFTPAREN:
            accept(TOKENIZER_LEFTPAREN);
            r = expr();
            accept(TOKENIZER_RIGHTPAREN);
            break;
        default:
            r = varfactor();
            break;
    }
    return r;
}


static int term(void) {
    int f1, f2;
    int op;

    f1 = factor();
    op = tokenizer_token();
    while (op == TOKENIZER_ASTR ||
           op == TOKENIZER_SLASH ||
           op == TOKENIZER_MOD) {
        tokenizer_next();
        f2 = factor();
        switch (op) {
            case TOKENIZER_ASTR:
                f1 = f1 * f2;
                break;
            case TOKENIZER_SLASH:
                f1 = f1 / f2;
                break;
            case TOKENIZER_MOD:
                f1 = f1 % f2;
                break;
        }
        op = tokenizer_token();
    }
    return f1;
}


static VARIABLE_TYPE expr(void) {
    int t1, t2;
    int op;

    t1 = term();
    op = tokenizer_token();
    while (op == TOKENIZER_PLUS ||
           op == TOKENIZER_MINUS ||
           op == TOKENIZER_AND ||
           op == TOKENIZER_OR) {
        tokenizer_next();
        t2 = term();
        switch (op) {
            case TOKENIZER_PLUS:
                t1 = t1 + t2;
                break;
            case TOKENIZER_MINUS:
                t1 = t1 - t2;
                break;
            case TOKENIZER_AND:
                t1 = t1 & t2;
                break;
            case TOKENIZER_OR:
                t1 = t1 | t2;
                break;
        }
        op = tokenizer_token();
    }
    return t1;
}


static int relation(void) {
    int r1, r2;
    int op;

    r1 = expr();
    op = tokenizer_token();
    while (op == TOKENIZER_LT ||
           op == TOKENIZER_GT ||
           op == TOKENIZER_EQ) {
        tokenizer_next();
        r2 = expr();
        switch (op) {
            case TOKENIZER_LT:
                r1 = r1 < r2;
                break;
            case TOKENIZER_GT:
                r1 = r1 > r2;
                break;
            case TOKENIZER_EQ:
                r1 = r1 == r2;
                break;
        }
        op = tokenizer_token();
    }
    return r1;
}


static void index_free(void) {
    if (line_index_head != NULL) {
        line_index_current = line_index_head;
        do {
            line_index_head = line_index_current;
            line_index_current = line_index_current->next;
            free(line_index_head);
        } while (line_index_current != NULL);
        line_index_head = NULL;
    }
}


static char const *index_find(int linenum) {
    struct line_index *lidx;
    lidx = line_index_head;

#if DEBUG
    int step = 0;
#endif

    while (lidx != NULL && lidx->line_number != linenum) {
        lidx = lidx->next;

#if DEBUG
        if(lidx != NULL) {
          DEBUG_PRINTF("index_find: Step %3d. indice existente %d: %d.\n",
                       step, lidx->line_number,
                       lidx->programa_text_position);
        }
        step++;
#endif
    }
    if (lidx != NULL && lidx->line_number == linenum) {
        return lidx->programa_text_position;
    }
    return NULL;
}


static void index_add(int linenum, char const *sourcepos) {
    if (line_index_head != NULL && index_find(linenum)) {
        return;
    }

    struct line_index *new_lidx;

    new_lidx = malloc(sizeof(struct line_index));
    new_lidx->line_number = linenum;
    new_lidx->programa_text_position = sourcepos;
    new_lidx->next = NULL;

    if (line_index_head != NULL) {
        line_index_current->next = new_lidx;
        line_index_current = line_index_current->next;
    } else {
        line_index_current = new_lidx;
        line_index_head = line_index_current;
    }
}


static void jump_linenum_slow(int linenum) {
    tokenizer_init(programa_ptr);
    while (tokenizer_num() != linenum) {
        do {
            do {
                tokenizer_next();
            } while (tokenizer_token() != TOKENIZER_CR &&
                     tokenizer_token() != TOKENIZER_ENDOFINPUT);
            if (tokenizer_token() == TOKENIZER_CR) {
                tokenizer_next();
            }
        } while (tokenizer_token() != TOKENIZER_NUMBER);
    }
}


static void jump_linenum(int linenum) {
    char const *pos = index_find(linenum);
    if (pos != NULL) {
        tokenizer_goto(pos);
    } else {
        jump_linenum_slow(linenum);
    }
}


static void goto_statement(void) {
    accept(TOKENIZER_GOTO);
    jump_linenum(tokenizer_num());
}


static void print_statement(void) {
    accept(TOKENIZER_PRINT);
    do {
        DEBUG_PRINTF("Print loop\n");
        if (tokenizer_token() == TOKENIZER_STRING) {
            tokenizer_string(string, sizeof(string));
            printf("%s", string);
            tokenizer_next();
        } else if (tokenizer_token() == TOKENIZER_COMMA) {
            printf(" ");
            tokenizer_next();
        } else if (tokenizer_token() == TOKENIZER_SEMICOLON) {
            tokenizer_next();
        } else if (tokenizer_token() == TOKENIZER_VARIABLE ||
                   tokenizer_token() == TOKENIZER_NUMBER) {
            printf("%d", expr());
        } else {
            break;
        }
    } while (tokenizer_token() != TOKENIZER_CR &&
             tokenizer_token() != TOKENIZER_ENDOFINPUT);
    printf("\n");
    tokenizer_next();
}


static void if_statement(void) {
    int r;

    accept(TOKENIZER_IF);

    r = relation();
    accept(TOKENIZER_THEN);
    if (r) {
        statement();
    } else {
        do {
            tokenizer_next();
        } while (tokenizer_token() != TOKENIZER_ELSE &&
                 tokenizer_token() != TOKENIZER_CR &&
                 tokenizer_token() != TOKENIZER_ENDOFINPUT);
        if (tokenizer_token() == TOKENIZER_ELSE) {
            tokenizer_next();
            statement();
        } else if (tokenizer_token() == TOKENIZER_CR) {
            tokenizer_next();
        }
    }
}


static void let_statement(void) {
    int var;

    var = tokenizer_variable_num();

    accept(TOKENIZER_VARIABLE);
    accept(TOKENIZER_EQ);
    basic_set_variable(var, expr());
    accept(TOKENIZER_CR);

}


static void gosub_statement(void) {
    int linenum;
    accept(TOKENIZER_GOSUB);
    linenum = tokenizer_num();
    accept(TOKENIZER_NUMBER);
    accept(TOKENIZER_CR);
    if (gosub_stack_ptr < MAX_GOSUB_STACK_DEPTH) {
        gosub_stack[gosub_stack_ptr] = tokenizer_num();
        gosub_stack_ptr++;
        jump_linenum(linenum);
    }
}


static void return_statement(void) {
    accept(TOKENIZER_RETURN);
    if (gosub_stack_ptr > 0) {
        gosub_stack_ptr--;
        jump_linenum(gosub_stack[gosub_stack_ptr]);
    }
}


static void next_statement(void) {
    int var;

    accept(TOKENIZER_NEXT);
    var = tokenizer_variable_num();
    accept(TOKENIZER_VARIABLE);
    if (for_stack_ptr > 0 &&
        var == for_stack[for_stack_ptr - 1].for_variable) {
        basic_set_variable(var,
                            basic_get_variable(var) + 1);
        if (basic_get_variable(var) <= for_stack[for_stack_ptr - 1].to) {
            jump_linenum(for_stack[for_stack_ptr - 1].line_after_for);
        } else {
            for_stack_ptr--;
            accept(TOKENIZER_CR);
        }
    } else {
        accept(TOKENIZER_CR);
    }

}


static void for_statement(void) {
    int for_variable, to;

    accept(TOKENIZER_FOR);
    for_variable = tokenizer_variable_num();
    accept(TOKENIZER_VARIABLE);
    accept(TOKENIZER_EQ);
    basic_set_variable(for_variable, expr());
    accept(TOKENIZER_TO);
    to = expr();
    accept(TOKENIZER_CR);

    if (for_stack_ptr < MAX_FOR_STACK_DEPTH) {
        for_stack[for_stack_ptr].line_after_for = tokenizer_num();
        for_stack[for_stack_ptr].for_variable = for_variable;
        for_stack[for_stack_ptr].to = to;

        for_stack_ptr++;
    }
}


static void peek_statement(void) {
    VARIABLE_TYPE peek_addr;
    int var;

    accept(TOKENIZER_PEEK);
    peek_addr = expr();
    accept(TOKENIZER_COMMA);
    var = tokenizer_variable_num();
    accept(TOKENIZER_VARIABLE);
    accept(TOKENIZER_CR);

    basic_set_variable(var, peek_function(peek_addr));
}


static void poke_statement(void) {
    VARIABLE_TYPE poke_addr;
    VARIABLE_TYPE value;

    accept(TOKENIZER_POKE);
    poke_addr = expr();
    accept(TOKENIZER_COMMA);
    value = expr();
    accept(TOKENIZER_CR);

    poke_function(poke_addr, value);
}


static void end_statement(void) {
    accept(TOKENIZER_END);
    ended = 1;
}


static void statement(void) {
    int token;

    token = tokenizer_token();

    switch (token) {
        case TOKENIZER_PRINT:
            print_statement();
            break;
        case TOKENIZER_IF:
            if_statement();
            break;
        case TOKENIZER_GOTO:
            goto_statement();
            break;
        case TOKENIZER_GOSUB:
            gosub_statement();
            break;
        case TOKENIZER_RETURN:
            return_statement();
            break;
        case TOKENIZER_FOR:
            for_statement();
            break;
        case TOKENIZER_PEEK:
            peek_statement();
            break;
        case TOKENIZER_POKE:
            poke_statement();
            break;
        case TOKENIZER_NEXT:
            next_statement();
            break;
        case TOKENIZER_END:
            end_statement();
            break;
        case TOKENIZER_LET:
            accept(TOKENIZER_LET);
            /* Fall through. */
        case TOKENIZER_VARIABLE:
            let_statement();
            break;
        default:
            DEBUG_PRINTF("basic.c: statement(): no implementado %d\n", token);
            exit(1);
    }
}


static void line_statement(void) {
    index_add(tokenizer_num(), tokenizer_pos());
    accept(TOKENIZER_NUMBER);
    statement();
    return;
}


void basic_run(void) {
    if (tokenizer_finished()) {
        return;
    }

    line_statement();
}


int basic_finished(void) {
    return ended || tokenizer_finished();
}


void basic_set_variable(int varnum, VARIABLE_TYPE value) {
    if (varnum >= 0 && varnum <= MAX_VARNUM) {
        variables[varnum] = value;
    }
}

VARIABLE_TYPE basic_get_variable(int varnum) {
    if (varnum >= 0 && varnum <= MAX_VARNUM) {
        return variables[varnum];
    }
    return 0;
}

