/* The parser. SHARED by the tcc kits. */
#ifndef LANG_FRONT_PARSER_H
#define LANG_FRONT_PARSER_H

#include "front/ast.h"
#include "front/lexer.h"

/* Appends the declarations of one source to LIST. Returns 1, or 0 after a
   diagnostic. */
int parse_source(Arena *arena, const char *source, Origin origin, const TokenList *toks, DeclList *list, Diag *diag);

/* parse_source for --partial. P1: a source that parses is kept. P2: a
   failure before the end of input is refused as in parse_source. P3: after a
   failure at the end of input, the whole items are kept, and the cut item too
   when it parses with its open '(' closed (then *CANDIDATE is 1 and it is the
   last declaration of LIST). On P1 success, *TAIL is the final body's
   application that can still take arguments at EOF, or NULL. Returns 1,
   or 0 after a diagnostic. */
int parse_source_partial(Arena *arena, const char *source, Origin origin, const TokenList *toks, DeclList *list, int *candidate, const Term **tail, Diag *diag);

#endif
