def post_process(src_content, src_rel):
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
