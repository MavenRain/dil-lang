/* The front end entry point. SHARED by the tcc kits. */
#ifndef LANG_FRONT_FRONT_H
#define LANG_FRONT_FRONT_H

#include "front/parser.h"

#define SOURCE_MAX_BYTES ((size_t)1 << 20)

/* build/domain.c holds the bytes of domain/domain.lang (gen/embed.c). */
extern const unsigned char domain_source[];
extern const size_t domain_source_len;

/* Parses the embedded domain file, then the program, into one list. */
int front_load(Arena *arena, const char *source, const char *text, size_t len, DeclList *out, Diag *diag);

/* front_load for --partial: the domain file as in front_load, the program
   with lex_source_partial and parse_source_partial. *CANDIDATE is 1 when the
   last declaration is a cut item with its open '(' closed. *TAIL identifies
   the final application that can still take arguments at EOF, or is NULL. */
int front_load_partial(Arena *arena, const char *source, const char *text, size_t len, DeclList *out, int *candidate, const Term **tail, Diag *diag);

#endif
