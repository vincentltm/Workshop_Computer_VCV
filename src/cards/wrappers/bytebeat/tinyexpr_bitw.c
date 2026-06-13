/*
 * TINYEXPR - Tiny recursive descent parser and evaluation engine in C
 *
 * Copyright (c) 2015, 2016 Lewis Van Winkle
 *
 * http://CodePlea.com
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 * claim that you wrote the original software. If you use this software
 * in a product, an acknowledgement in the product documentation would be
 * appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

/* This has been altered from the original software to be used for bitwise operations
 * Most notably all of the double types are now int, and most of the floating point
 * math funcitons are commented out.
 * supported operators << >> | ~ ^ & . Also, I added a couple of custom bit functions
 * low and high
 *
 * VCV port: renamed and/or/xor/not to te_and/te_or/te_xor/te_not to avoid
 * C++ reserved keyword conflicts.
 */

#include "tinyexpr_bitw.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stdint.h>

#define NOTNUM 0

#ifndef INFINITY
#define INFINITY (1/0)
#endif

/* Suppress printf in embedded context */
#define printf(fmt, ...) ({0;})

typedef int (*te_fun2)(int, int);

enum {
    TOK_NULL = TE_CLOSURE7+1, TOK_ERROR, TOK_END, TOK_SEP,
    TOK_OPEN, TOK_CLOSE, TOK_NUMBER, TOK_VARIABLE, TOK_INFIX
};

enum {TE_CONSTANT = 1};

typedef struct state {
    const char *start;
    const char *next;
    int type;
    union {int value; const int *bound; const void *function;};
    void *context;

    const te_variable *lookup;
    int lookup_len;
} state;

#define TYPE_MASK(TYPE) ((TYPE)&0x0000001F)

#define IS_PURE(TYPE) (((TYPE) & TE_FLAG_PURE) != 0)
#define IS_FUNCTION(TYPE) (((TYPE) & TE_FUNCTION0) != 0)
#define IS_CLOSURE(TYPE) (((TYPE) & TE_CLOSURE0) != 0)
#define ARITY(TYPE) ( ((TYPE) & (TE_FUNCTION0 | TE_CLOSURE0)) ? ((TYPE) & 0x00000007) : 0 )
#define NEW_EXPR(type, ...) new_expr((type), (const te_expr*[]){__VA_ARGS__})

static te_expr *new_expr(const int type, const te_expr *parameters[]) {
    const int arity = ARITY(type);
    const int psize = sizeof(void*) * arity;
    const int size = (sizeof(te_expr) - sizeof(void*)) + psize + (IS_CLOSURE(type) ? sizeof(void*) : 0);
    te_expr *ret = (te_expr*)malloc(size);
    memset(ret, 0, size);
    if (arity && parameters) {
        memcpy(ret->parameters, parameters, psize);
    }
    ret->type = type;
    ret->bound = 0;
    return ret;
}

void te_free_parameters(te_expr *n) {
    if (!n) return;
    switch (TYPE_MASK(n->type)) {
        case TE_FUNCTION7: case TE_CLOSURE7: te_free((te_expr*)n->parameters[6]);
        /* fall through */
        case TE_FUNCTION6: case TE_CLOSURE6: te_free((te_expr*)n->parameters[5]);
        /* fall through */
        case TE_FUNCTION5: case TE_CLOSURE5: te_free((te_expr*)n->parameters[4]);
        /* fall through */
        case TE_FUNCTION4: case TE_CLOSURE4: te_free((te_expr*)n->parameters[3]);
        /* fall through */
        case TE_FUNCTION3: case TE_CLOSURE3: te_free((te_expr*)n->parameters[2]);
        /* fall through */
        case TE_FUNCTION2: case TE_CLOSURE2: te_free((te_expr*)n->parameters[1]);
        /* fall through */
        case TE_FUNCTION1: case TE_CLOSURE1: te_free((te_expr*)n->parameters[0]);
        /* fall through */
        default: break;
    }
}

void te_free(te_expr *n) {
    if (!n) return;
    te_free_parameters(n);
    free(n);
}

static int low(int a) {
    uint16_t i = (uint16_t)a;
    return (int)(i & 0x00ff);
}
static int high(int a) {
    uint16_t i = (uint16_t)a;
    return (int)((i & 0xff00) >> 8);
}

static const te_variable functions[] = {
    /* must be in alphabetical order */
    {(char*)"high", (const void*)high, TE_FUNCTION1, 0},
    {(char*)"low",  (const void*)low,  TE_FUNCTION1, 0}
};

static const te_variable *find_builtin(const char *name, int len) {
    int imin = 0;
    int imax = 1;

    while (imax >= imin) {
        const int i = (imin + ((imax-imin)/2));
        int c = strncmp(name, functions[i].name, len);
        if (!c) c = '\0' - functions[i].name[len];
        if (c == 0) {
            return functions + i;
        } else if (c > 0) {
            imin = i + 1;
        } else {
            imax = i - 1;
        }
    }

    return 0;
}

static const te_variable *find_lookup(const state *s, const char *name, int len) {
    int iters;
    const te_variable *var;
    if (!s->lookup) return 0;

    for (var = s->lookup, iters = s->lookup_len; iters; ++var, --iters) {
        if (strncmp(name, var->name, len) == 0 && var->name[len] == '\0') {
            return var;
        }
    }
    return 0;
}

/* Renamed from and/or/xor/not to avoid C++ reserved keyword conflicts */
static int te_and(int a, int b) {return (int)((uint32_t)a  & (uint32_t)b);}
static int te_or(int a, int b)  {return (int)((uint32_t)a  | (uint32_t)b);}
static int te_xor(int a, int b) {return (int)((uint32_t)a  ^ (uint32_t)b);}
static int te_not(int a)        {return ~a;}

static int less_than(int a, int b)    {return ((a < b) ? 1 : 0);}
static int greater_than(int a, int b) {return ((a > b) ? 1 : 0);}
static int shiftl(int a, int b) {return (int)((uint32_t)a << (uint32_t)b);}
static int shiftr(int a, int b) {return (int)((uint32_t)a >> (uint32_t)b);}
static int add(int a, int b)    {return a + b;}
static int sub(int a, int b)    {return a - b;}
static int mul(int a, int b)    {return a * b;}
static int divide(int a, int b) {return a / b;}
static int negate(int a)        {return -a;}
static int comma(int a, int b)  {(void)a; return b;}
static int intmod(int a, int b) {return a % b;}

void next_token(state *s) {
    s->type = TOK_NULL;

    do {
        if (!*s->next) {
            s->type = TOK_END;
            return;
        }

        if ((s->next[0] >= '0' && s->next[0] <= '9') || s->next[0] == '.') {
            s->value = (int)strtod(s->next, (char**)&s->next);
            s->type = TOK_NUMBER;
        } else if (s->next[0] == '\'') {
            /* char literal */
            s->value = s->next[1];
            s->next += 3;
            s->type = TOK_NUMBER;
        } else {
            if (s->next[0] >= 'a' && s->next[0] <= 'z') {
                const char *start;
                start = s->next;
                while ((s->next[0] >= 'a' && s->next[0] <= 'z') ||
                       (s->next[0] >= '0' && s->next[0] <= '9') ||
                       (s->next[0] == '_')) s->next++;

                const te_variable *var = find_lookup(s, start, s->next - start);
                if (!var) var = find_builtin(start, s->next - start);

                if (!var) {
                    s->type = TOK_ERROR;
                } else {
                    switch(TYPE_MASK(var->type)) {
                        case TE_VARIABLE:
                            s->type = TOK_VARIABLE;
                            s->bound = (const int*)var->address;
                            break;
                        case TE_CLOSURE0: case TE_CLOSURE1: case TE_CLOSURE2: case TE_CLOSURE3:
                        case TE_CLOSURE4: case TE_CLOSURE5: case TE_CLOSURE6: case TE_CLOSURE7:
                            s->context = var->context;
                            /* fall through */
                        case TE_FUNCTION0: case TE_FUNCTION1: case TE_FUNCTION2: case TE_FUNCTION3:
                        case TE_FUNCTION4: case TE_FUNCTION5: case TE_FUNCTION6: case TE_FUNCTION7:
                            s->type = var->type;
                            s->function = var->address;
                            break;
                    }
                }
            } else {
                switch (s->next[0]) {
                    case '+': s->type = TOK_INFIX; s->function = (const void*)add; break;
                    case '-': s->type = TOK_INFIX; s->function = (const void*)sub; break;
                    case '*': s->type = TOK_INFIX; s->function = (const void*)mul; break;
                    case '/': s->type = TOK_INFIX; s->function = (const void*)divide; break;
                    case '^': s->type = TOK_INFIX; s->function = (const void*)te_xor; break;
                    case '~': s->type = TOK_INFIX; s->function = (const void*)te_not; break;
                    case '|': s->type = TOK_INFIX; s->function = (const void*)te_or;  break;
                    case '&': s->type = TOK_INFIX; s->function = (const void*)te_and; break;
                    case '%': s->type = TOK_INFIX; s->function = (const void*)intmod; break;
                    case '>':
                        s->type = TOK_INFIX;
                        if (s->next[1] == '>') {
                            s->function = (const void*)shiftr;
                            s->next++;
                        } else {
                            s->function = (const void*)greater_than;
                        }
                        break;
                    case '<':
                        s->type = TOK_INFIX;
                        if (s->next[1] == '<') {
                            s->function = (const void*)shiftl;
                            s->next++;
                        } else {
                            s->function = (const void*)less_than;
                        }
                        break;
                    case '(': s->type = TOK_OPEN; break;
                    case ')': s->type = TOK_CLOSE; break;
                    case ',': s->type = TOK_SEP; break;
                    case ' ': case '\t': case '\n': case '\r': break;
                    default: s->type = TOK_ERROR; break;
                }
                s->next++;
            }
        }
    } while (s->type == TOK_NULL);
}


static te_expr *list(state *s);
static te_expr *expr(state *s);
static te_expr *power(state *s);

static te_expr *base(state *s) {
    te_expr *ret;
    int arity;

    switch (TYPE_MASK(s->type)) {
        case TOK_NUMBER:
            ret = new_expr(TE_CONSTANT, 0);
            ret->value = s->value;
            next_token(s);
            break;

        case TOK_VARIABLE:
            ret = new_expr(TE_VARIABLE, 0);
            ret->bound = s->bound;
            next_token(s);
            break;

        case TE_FUNCTION0:
        case TE_CLOSURE0:
            ret = new_expr(s->type, 0);
            ret->function = s->function;
            if (IS_CLOSURE(s->type)) ret->parameters[0] = s->context;
            next_token(s);
            if (s->type == TOK_OPEN) {
                next_token(s);
                if (s->type != TOK_CLOSE) {
                    s->type = TOK_ERROR;
                } else {
                    next_token(s);
                }
            }
            break;

        case TE_FUNCTION1:
        case TE_CLOSURE1:
            ret = new_expr(s->type, 0);
            ret->function = s->function;
            if (IS_CLOSURE(s->type)) ret->parameters[1] = s->context;
            next_token(s);
            ret->parameters[0] = power(s);
            break;

        case TE_FUNCTION2: case TE_FUNCTION3: case TE_FUNCTION4:
        case TE_FUNCTION5: case TE_FUNCTION6: case TE_FUNCTION7:
        case TE_CLOSURE2:  case TE_CLOSURE3:  case TE_CLOSURE4:
        case TE_CLOSURE5:  case TE_CLOSURE6:  case TE_CLOSURE7:
            arity = ARITY(s->type);
            ret = new_expr(s->type, 0);
            ret->function = s->function;
            if (IS_CLOSURE(s->type)) ret->parameters[arity] = s->context;
            next_token(s);
            if (s->type != TOK_OPEN) {
                s->type = TOK_ERROR;
            } else {
                int i;
                for (i = 0; i < arity; i++) {
                    next_token(s);
                    ret->parameters[i] = expr(s);
                    if (s->type != TOK_SEP) {
                        break;
                    }
                }
                if (s->type != TOK_CLOSE || i != arity - 1) {
                    s->type = TOK_ERROR;
                } else {
                    next_token(s);
                }
            }
            break;

        case TOK_OPEN:
            next_token(s);
            ret = list(s);
            if (s->type != TOK_CLOSE) {
                s->type = TOK_ERROR;
            } else {
                next_token(s);
            }
            break;

        default:
            ret = new_expr(0, 0);
            s->type = TOK_ERROR;
            ret->value = NOTNUM;
            break;
    }

    return ret;
}

static te_expr *power(state *s) {
    int sign = 1, bnot = 0;
    while (s->type == TOK_INFIX && (s->function == (const void*)add ||
                                    s->function == (const void*)sub ||
                                    s->function == (const void*)te_not)) {
        if (s->function == (const void*)sub) sign = -sign;
        if (s->function == (const void*)te_not) bnot = 1;
        next_token(s);
    }

    te_expr *ret;
    if (sign == 1) {
        if (bnot == 1) {
            ret = NEW_EXPR(TE_FUNCTION1 | TE_FLAG_PURE, base(s));
            ret->function = (const void*)te_not;
        } else {
            ret = base(s);
        }
    } else {
        ret = NEW_EXPR(TE_FUNCTION1 | TE_FLAG_PURE, base(s));
        ret->function = (const void*)negate;
    }
    return ret;
}

static te_expr *factor(state *s) {
    te_expr *ret = power(s);
    return ret;
}

static te_expr *term(state *s) {
    te_expr *ret = factor(s);
    while (s->type == TOK_INFIX && (s->function == (const void*)mul ||
                                    s->function == (const void*)divide ||
                                    s->function == (const void*)intmod)) {
        te_fun2 t = (te_fun2)s->function;
        next_token(s);
        ret = NEW_EXPR(TE_FUNCTION2 | TE_FLAG_PURE, ret, factor(s));
        ret->function = (const void*)t;
    }
    return ret;
}

static te_expr *parse_additive(state *s) {
    te_expr *ret = term(s);
    while (s->type == TOK_INFIX && (s->function == (const void*)add ||
                                    s->function == (const void*)sub)) {
        te_fun2 t = (te_fun2)s->function;
        next_token(s);
        ret = NEW_EXPR(TE_FUNCTION2 | TE_FLAG_PURE, ret, term(s));
        ret->function = (const void*)t;
    }
    return ret;
}

static te_expr *parse_shift(state *s) {
    te_expr *ret = parse_additive(s);
    while (s->type == TOK_INFIX && (s->function == (const void*)shiftl ||
                                    s->function == (const void*)shiftr)) {
        te_fun2 t = (te_fun2)s->function;
        next_token(s);
        ret = NEW_EXPR(TE_FUNCTION2 | TE_FLAG_PURE, ret, parse_additive(s));
        ret->function = (const void*)t;
    }
    return ret;
}

static te_expr *parse_relational(state *s) {
    te_expr *ret = parse_shift(s);
    while (s->type == TOK_INFIX && (s->function == (const void*)less_than ||
                                    s->function == (const void*)greater_than)) {
        te_fun2 t = (te_fun2)s->function;
        next_token(s);
        ret = NEW_EXPR(TE_FUNCTION2 | TE_FLAG_PURE, ret, parse_shift(s));
        ret->function = (const void*)t;
    }
    return ret;
}

static te_expr *parse_bitwise_and(state *s) {
    te_expr *ret = parse_relational(s);
    while (s->type == TOK_INFIX && s->function == (const void*)te_and) {
        te_fun2 t = (te_fun2)s->function;
        next_token(s);
        ret = NEW_EXPR(TE_FUNCTION2 | TE_FLAG_PURE, ret, parse_relational(s));
        ret->function = (const void*)t;
    }
    return ret;
}

static te_expr *parse_bitwise_xor(state *s) {
    te_expr *ret = parse_bitwise_and(s);
    while (s->type == TOK_INFIX && s->function == (const void*)te_xor) {
        te_fun2 t = (te_fun2)s->function;
        next_token(s);
        ret = NEW_EXPR(TE_FUNCTION2 | TE_FLAG_PURE, ret, parse_bitwise_and(s));
        ret->function = (const void*)t;
    }
    return ret;
}

static te_expr *parse_bitwise_or(state *s) {
    te_expr *ret = parse_bitwise_xor(s);
    while (s->type == TOK_INFIX && s->function == (const void*)te_or) {
        te_fun2 t = (te_fun2)s->function;
        next_token(s);
        ret = NEW_EXPR(TE_FUNCTION2 | TE_FLAG_PURE, ret, parse_bitwise_xor(s));
        ret->function = (const void*)t;
    }
    return ret;
}

static te_expr *expr(state *s) {
    return parse_bitwise_or(s);
}

static te_expr *list(state *s) {
    te_expr *ret = expr(s);
    while (s->type == TOK_SEP) {
        next_token(s);
        ret = NEW_EXPR(TE_FUNCTION2 | TE_FLAG_PURE, ret, expr(s));
        ret->function = (const void*)comma;
    }
    return ret;
}

#define TE_FUN(...) ((int(*)(__VA_ARGS__))n->function)
#define M(e) te_eval((const te_expr*)n->parameters[e])

int te_eval(const te_expr *n) {
    if (!n) return NOTNUM;

    switch(TYPE_MASK(n->type)) {
        case TE_CONSTANT: return n->value;
        case TE_VARIABLE: return *n->bound;

        case TE_FUNCTION0: case TE_FUNCTION1: case TE_FUNCTION2: case TE_FUNCTION3:
        case TE_FUNCTION4: case TE_FUNCTION5: case TE_FUNCTION6: case TE_FUNCTION7:
            switch(ARITY(n->type)) {
                case 0: return TE_FUN(void)();
                case 1: return TE_FUN(int)(M(0));
                case 2: return TE_FUN(int, int)(M(0), M(1));
                case 3: return TE_FUN(int, int, int)(M(0), M(1), M(2));
                case 4: return TE_FUN(int, int, int, int)(M(0), M(1), M(2), M(3));
                case 5: return TE_FUN(int, int, int, int, int)(M(0), M(1), M(2), M(3), M(4));
                case 6: return TE_FUN(int, int, int, int, int, int)(M(0), M(1), M(2), M(3), M(4), M(5));
                case 7: return TE_FUN(int, int, int, int, int, int, int)(M(0), M(1), M(2), M(3), M(4), M(5), M(6));
                default: return NOTNUM;
            }

        case TE_CLOSURE0: case TE_CLOSURE1: case TE_CLOSURE2: case TE_CLOSURE3:
        case TE_CLOSURE4: case TE_CLOSURE5: case TE_CLOSURE6: case TE_CLOSURE7:
            switch(ARITY(n->type)) {
                case 0: return TE_FUN(void*)(n->parameters[0]);
                case 1: return TE_FUN(void*, int)(n->parameters[1], M(0));
                case 2: return TE_FUN(void*, int, int)(n->parameters[2], M(0), M(1));
                case 3: return TE_FUN(void*, int, int, int)(n->parameters[3], M(0), M(1), M(2));
                case 4: return TE_FUN(void*, int, int, int, int)(n->parameters[4], M(0), M(1), M(2), M(3));
                case 5: return TE_FUN(void*, int, int, int, int, int)(n->parameters[5], M(0), M(1), M(2), M(3), M(4));
                case 6: return TE_FUN(void*, int, int, int, int, int, int)(n->parameters[6], M(0), M(1), M(2), M(3), M(4), M(5));
                case 7: return TE_FUN(void*, int, int, int, int, int, int, int)(n->parameters[7], M(0), M(1), M(2), M(3), M(4), M(5), M(6));
                default: return NOTNUM;
            }

        default: return NOTNUM;
    }
}

#undef TE_FUN
#undef M

static void optimize(te_expr *n) {
    if (n->type == TE_CONSTANT) return;
    if (n->type == TE_VARIABLE) return;

    if (IS_PURE(n->type)) {
        const int arity = ARITY(n->type);
        int known = 1;
        int i;
        for (i = 0; i < arity; ++i) {
            optimize((te_expr*)n->parameters[i]);
            if (((te_expr*)(n->parameters[i]))->type != TE_CONSTANT) {
                known = 0;
            }
        }
        if (known) {
            const int value = te_eval(n);
            te_free_parameters(n);
            n->type = TE_CONSTANT;
            n->value = value;
        }
    }
}

te_expr *te_compile(const char *expression, const te_variable *variables, int var_count, int *error) {
    state s;
    s.start = s.next = expression;
    s.lookup = variables;
    s.lookup_len = var_count;

    next_token(&s);
    te_expr *root = list(&s);

    if (s.type != TOK_END) {
        te_free(root);
        if (error) {
            *error = (s.next - s.start);
            if (*error == 0) *error = 1;
        }
        return 0;
    } else {
        optimize(root);
        if (error) *error = 0;
        return root;
    }
}

int te_interp(const char *expression, int *error) {
    te_expr *n = te_compile(expression, 0, 0, error);
    int ret;
    if (n) {
        ret = te_eval(n);
        te_free(n);
    } else {
        ret = NOTNUM;
    }
    return ret;
}

void te_print(const te_expr *n) {
    (void)n;
}
