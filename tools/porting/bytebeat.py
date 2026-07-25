def post_process(src_content, src_rel):
    # Rename C++ reserved operator keywords and, or, not, xor
    src_content = src_content.replace("static int or(int a, int b)", "static int op_or(int a, int b)")
    src_content = src_content.replace("static int and(int a, int b)", "static int op_and(int a, int b)")
    src_content = src_content.replace("static int not(int a)", "static int op_not(int a)")
    src_content = src_content.replace("static int xor(int a, int b)", "static int op_xor(int a, int b)")
    src_content = src_content.replace("te_expr *ret = malloc(", "te_expr *ret = (te_expr*)malloc(")
    src_content = src_content.replace("s->function = add;", "s->function = (const void*)add;")
    src_content = src_content.replace("s->function = sub;", "s->function = (const void*)sub;")
    src_content = src_content.replace("s->function = mul;", "s->function = (const void*)mul;")
    src_content = src_content.replace("s->function = divide;", "s->function = (const void*)divide;")
    src_content = src_content.replace("s->function = modulus;", "s->function = (const void*)modulus;")
    src_content = src_content.replace("s->function = and;", "s->function = (const void*)op_and;")
    src_content = src_content.replace("s->function = or;", "s->function = (const void*)op_or;")
    src_content = src_content.replace("s->function = not;", "s->function = (const void*)op_not;")
    src_content = src_content.replace("s->function = xor;", "s->function = (const void*)op_xor;")
    src_content = src_content.replace("s->function = intmod;", "s->function = (const void*)intmod;")
    src_content = src_content.replace("s->function = shiftr;", "s->function = (const void*)shiftr;")
    src_content = src_content.replace("s->function = shiftl;", "s->function = (const void*)shiftl;")
    src_content = src_content.replace("s->function = greater_than;", "s->function = (const void*)greater_than;")
    src_content = src_content.replace("s->function = less_than;", "s->function = (const void*)less_than;")
    src_content = src_content.replace("s->function = negate;", "s->function = (const void*)negate;")
    src_content = src_content.replace("s->bound = var->address;", "s->bound = (const int*)var->address;")
    src_content = src_content.replace('{"high", high, TE_FUNCTION1, 0}', '{"high", (const void*)high, TE_FUNCTION1, 0}')
    src_content = src_content.replace('{"low", low, TE_FUNCTION1, 0}', '{"low", (const void*)low, TE_FUNCTION1, 0}')
    
    src_content = src_content.replace("s->function == not", "s->function == (const void*)op_not")
    src_content = src_content.replace("s->function == pow", "s->function == (const void*)powf")
    src_content = src_content.replace("ret->function = not;", "ret->function = (const void*)op_not;")
    src_content = src_content.replace("te_fun2 t = s->function;", "te_fun2 t = (te_fun2)s->function;")
    src_content = src_content.replace("te_free(n->parameters[", "te_free((te_expr*)n->parameters[")

    if src_rel == "tinyexpr_bitw.c":
        target = '#define NEW_EXPR(type, ...) new_expr((type), (const te_expr*[]){__VA_ARGS__})'
        replacement = """static te_expr *new_expr(const int type, const te_expr *parameters[]);

static te_expr *new_expr_args(const int type, const te_expr* p1) {
    const te_expr *params[1] = { p1 };
    return new_expr(type, params);
}

static te_expr *new_expr_args(const int type, const te_expr* p1, const te_expr* p2) {
    const te_expr *params[2] = { p1, p2 };
    return new_expr(type, params);
}

#define NEW_EXPR(type, ...) new_expr_args((type), __VA_ARGS__)"""
        src_content = src_content.replace(target, replacement)
    return src_content
