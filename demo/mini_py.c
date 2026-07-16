#include "mini_py.h"
#include <stdint.h>
#include <stddef.h>

static int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

static void (*output_fn)(char) = NULL;

void mini_py_set_output(void (*fn)(char)) {
    output_fn = fn;
}

static void pchar(char c) {
    if (output_fn) output_fn(c);
}

static void pstr(const char *s) {
    for (; *s; s++) pchar(*s);
}

/* ─── value ────────────────────────────────────────────────── */

typedef enum { V_NONE, V_INT, V_STR } vtype_t;

typedef struct {
    vtype_t type;
    int as_int;
    const char *as_str;
} value_t;

static value_t val_int(int n) { value_t v; v.type = V_INT; v.as_int = n; v.as_str = NULL; return v; }
static value_t val_str(const char *s) { value_t v; v.type = V_STR; v.as_int = 0; v.as_str = s; return v; }
static value_t val_none(void) { value_t v; v.type = V_NONE; v.as_int = 0; v.as_str = NULL; return v; }

/* ─── lexer ────────────────────────────────────────────────── */

typedef struct {
    const char *src;
    int pos;
} lexer_t;

static void lex_skip(lexer_t *lx) {
    while (lx->src[lx->pos] == ' ' || lx->src[lx->pos] == '\t' || lx->src[lx->pos] == '\n')
        lx->pos++;
}

static char lex_peek(lexer_t *lx) {
    lex_skip(lx);
    return lx->src[lx->pos];
}

static char lex_next(lexer_t *lx) {
    lex_skip(lx);
    char c = lx->src[lx->pos];
    if (c) lx->pos++;
    return c;
}

/* ─── parser / evaluator ──────────────────────────────────── */

static value_t eval_expr(lexer_t *lx);

static value_t eval_atom(lexer_t *lx) {
    char c = lex_next(lx);

    if (c == '"') {
        static char buf[256];
        int i = 0;
        while (lx->src[lx->pos] && lx->src[lx->pos] != '"' && i < 255) {
            if (lx->src[lx->pos] == '\\' && lx->src[lx->pos + 1] == 'n') {
                buf[i++] = '\n'; lx->pos += 2;
            } else {
                buf[i++] = lx->src[lx->pos++];
            }
        }
        buf[i] = '\0';
        if (lx->src[lx->pos] == '"') lx->pos++;
        return val_str(buf);
    }

    /* identifier: read name */
    char name[64];
    int i = 0;
    name[i++] = c;
    while ((lx->src[lx->pos] >= 'a' && lx->src[lx->pos] <= 'z') ||
           (lx->src[lx->pos] >= 'A' && lx->src[lx->pos] <= 'Z') ||
           (lx->src[lx->pos] >= '0' && lx->src[lx->pos] <= '9') ||
           lx->src[lx->pos] == '_') {
        if (i < 63) name[i++] = lx->src[lx->pos];
        lx->pos++;
    }
    name[i] = '\0';

    /* check for call */
    if (lex_peek(lx) == '(') {
        lex_next(lx); /* '(' */
        value_t arg = eval_expr(lx);
        lex_next(lx); /* ')' */

        if (strcmp(name, "print") == 0) {
            if (arg.type == V_STR) pstr(arg.as_str);
            else if (arg.type == V_INT) {
                char buf[16];
                int j = 15; buf[15] = '\0';
                int n = arg.as_int;
                if (n == 0) { buf[--j] = '0'; }
                else {
                    while (n > 0 && j > 0) { buf[--j] = '0' + (n % 10); n /= 10; }
                }
                pstr(&buf[j]);
            }
            pchar('\n');
            return val_none();
        }

        if (strcmp(name, "len") == 0) {
            if (arg.type == V_STR) {
                int len = 0;
                while (arg.as_str[len]) len++;
                return val_int(len);
            }
            return val_int(0);
        }

        pstr("unknown function: "); pstr(name); pchar('\n');
        return val_none();
    }

    return val_none();
}

static value_t eval_expr(lexer_t *lx) {
    return eval_atom(lx);
}

/* ─── public API ──────────────────────────────────────────── */

int mini_py_exec(const char *source) {
    lexer_t lx;
    lx.src = source;
    lx.pos = 0;
    eval_expr(&lx);
    return 0;
}
