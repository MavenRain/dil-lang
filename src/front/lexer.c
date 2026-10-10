#include <string.h>

#include "front/lexer.h"

typedef struct {
  const char *text;
  size_t len;
  size_t pos;
  int line;
  int col;
  Token *out;
  size_t count;
  Diag *diag;
  const char *source;
  Arena *arena; /* the decoded bytes of string literals */
  int partial;  /* lex_source_partial: a cut at the end of input is dropped */
} Lexer;

static const struct {
  const char *word;
  TokKind kind;
} KEYWORDS[] = {
  {"def", TOK_KW_DEF},
  {"rec", TOK_KW_REC},
  {"fun", TOK_KW_FUN},
  {"family", TOK_KW_FAMILY},
  {"axiom", TOK_KW_AXIOM},
  {"Type", TOK_KW_TYPE},
  {"Sigma", TOK_KW_SIGMA},
};

const char *tok_kind_name(TokKind kind) {
  switch (kind) {
    case TOK_EOF: return "end of input";
    case TOK_IDENT: return "a name";
    case TOK_NAT: return "a Nat literal";
    case TOK_STR: return "a string literal";
    case TOK_LPAREN: return "'('";
    case TOK_RPAREN: return "')'";
    case TOK_COLON: return "':'";
    case TOK_DEFINE: return "':='";
    case TOK_ARROW: return "'->'";
    case TOK_FATARROW: return "'=>'";
    case TOK_STAR: return "'*'";
    case TOK_BAR: return "'|'";
    case TOK_KW_DEF: return "'def'";
    case TOK_KW_REC: return "'rec'";
    case TOK_KW_FUN: return "'fun'";
    case TOK_KW_FAMILY: return "'family'";
    case TOK_KW_AXIOM: return "'axiom'";
    case TOK_KW_TYPE: return "'Type'";
    case TOK_KW_SIGMA: return "'Sigma'";
  }
  return "a token";
}

static int is_digit(char c) {
  return c >= '0' && c <= '9';
}

static int is_ident_start(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static int is_ident_char(char c) {
  return is_ident_start(c) || is_digit(c) || c == '\'';
}

static char peek(const Lexer *lx, size_t ahead) {
  return lx->pos + ahead < lx->len ? lx->text[lx->pos + ahead] : '\0';
}

static void advance(Lexer *lx, size_t width) {
  for (size_t i = 0; i < width && lx->pos < lx->len; i++) {
    int newline = lx->text[lx->pos] == '\n';
    lx->line += newline;
    lx->col = newline ? 1 : lx->col + 1;
    lx->pos++;
  }
}

static Token *push(Lexer *lx, TokKind kind, size_t start, int line, int col) {
  Token *tok = &lx->out[lx->count];
  lx->count++;
  tok->kind = kind;
  tok->text = lx->text + start;
  tok->len = lx->pos - start;
  tok->nat = 0;
  tok->str = NULL;
  tok->str_len = 0;
  tok->line = line;
  tok->col = col;
  return tok;
}

/* Skips white space and `--` comments to the end of the line. */
static void skip_trivia(Lexer *lx) {
  for (;;) {
    char c = peek(lx, 0);
    int space = lx->pos < lx->len && (c == ' ' || c == '\t' || c == '\n' || c == '\r');
    int comment = c == '-' && peek(lx, 1) == '-';
    if (!space && !comment) return;
    if (space) {
      advance(lx, 1);
      continue;
    }
    while (lx->pos < lx->len && lx->text[lx->pos] != '\n') advance(lx, 1);
  }
}

static TokKind keyword_kind(const char *text, size_t len) {
  for (size_t i = 0; i < sizeof KEYWORDS / sizeof KEYWORDS[0]; i++) {
    if (strlen(KEYWORDS[i].word) == len && memcmp(KEYWORDS[i].word, text, len) == 0) return KEYWORDS[i].kind;
  }
  return TOK_IDENT;
}

/* A token that the next byte can still extend: a name or a keyword, a Nat
   literal, or ':' (to ':='). */
static int can_grow(TokKind kind) {
  switch (kind) {
    case TOK_IDENT:
    case TOK_NAT:
    case TOK_COLON:
    case TOK_KW_DEF:
    case TOK_KW_REC:
    case TOK_KW_FUN:
    case TOK_KW_FAMILY:
    case TOK_KW_AXIOM:
    case TOK_KW_TYPE:
    case TOK_KW_SIGMA:
      return 1;
    case TOK_EOF:
    case TOK_STR:
    case TOK_LPAREN:
    case TOK_RPAREN:
    case TOK_DEFINE:
    case TOK_ARROW:
    case TOK_FATARROW:
    case TOK_STAR:
    case TOK_BAR:
      return 0;
  }
  return 0;
}

static int lex_ident(Lexer *lx) {
  size_t start = lx->pos;
  int line = lx->line;
  int col = lx->col;
  while (is_ident_char(peek(lx, 0))) advance(lx, 1);
  push(lx, keyword_kind(lx->text + start, lx->pos - start), start, line, col);
  return 1;
}

static int lex_nat(Lexer *lx) {
  size_t start = lx->pos;
  int line = lx->line;
  int col = lx->col;
  uint64_t value = 0;
  int overflow = 0;
  while (is_digit(peek(lx, 0))) {
    uint64_t digit = (uint64_t)(peek(lx, 0) - '0');
    overflow |= value > (UINT64_MAX - digit) / 10u;
    value = value * 10u + digit;
    advance(lx, 1);
  }
  if (overflow) {
    return diag_fail(lx->diag, "LEX_NAT_RANGE", NULL, "%s:%d:%d: a Nat literal is larger than 2^64 - 1", lx->source, line, col);
  }
  if (is_ident_start(peek(lx, 0))) {
    return diag_fail(lx->diag, "LEX_CHAR", NULL, "%s:%d:%d: a letter follows a Nat literal", lx->source, lx->line, lx->col);
  }
  push(lx, TOK_NAT, start, line, col)->nat = value;
  return 1;
}

/* The byte that the escape `\E` stands for, or -1 for an unknown escape. */
static int escape_byte(char e) {
  switch (e) {
    case '"': return '"';
    case '\\': return '\\';
    case 'n': return '\n';
    default: return -1;
  }
}

/* A string literal. The escapes are \" \\ and \n; every other byte (UTF-8
   too) stays as it is. A raw newline or other control byte is refused. The
   first pass checks the literal and counts its bytes; the second copies them
   into the arena. */
static int lex_str(Lexer *lx) {
  size_t start = lx->pos;
  int line = lx->line;
  int col = lx->col;
  size_t count = 0;
  advance(lx, 1);
  while (lx->pos < lx->len && peek(lx, 0) != '"') {
    unsigned char c = (unsigned char)peek(lx, 0);
    if (c < 0x20u || c == 0x7fu) {
      return diag_fail(lx->diag, "LEX_STR_CONTROL", NULL, "%s:%d:%d: a string literal holds the control byte 0x%02x (write \\n for a newline)",
                       lx->source, lx->line, lx->col, (unsigned)c);
    }
    if (c == '\\' && lx->pos + 1 < lx->len && escape_byte(peek(lx, 1)) < 0) {
      return diag_fail(lx->diag, "LEX_STR_ESCAPE", NULL, "%s:%d:%d: an unknown escape in a string literal (the escapes are \\\" \\\\ \\n)",
                       lx->source, lx->line, lx->col);
    }
    advance(lx, c == '\\' ? 2 : 1);
    count++;
  }
  /* L1 of --partial: a literal that the end of input cuts is dropped, also
     with a cut escape or a cut UTF-8 sequence at its end. */
  if (lx->pos >= lx->len && lx->partial) return 1;
  if (lx->pos >= lx->len) {
    return diag_fail(lx->diag, "LEX_STR_OPEN", NULL, "%s:%d:%d: a string literal has no closing quote", lx->source, line, col);
  }
  advance(lx, 1);
  char *bytes = arena_alloc(lx->arena, count + 1);
  if (bytes == NULL) return diag_fail(lx->diag, "OOM", NULL, "%s: no memory for a string literal", lx->source);
  size_t at = start + 1;
  for (size_t i = 0; i < count; i++) {
    int escaped = lx->text[at] == '\\';
    bytes[i] = escaped ? (char)escape_byte(lx->text[at + 1]) : lx->text[at];
    at += escaped ? 2u : 1u;
  }
  bytes[count] = '\0';
  Token *tok = push(lx, TOK_STR, start, line, col);
  tok->str = bytes;
  tok->str_len = count;
  return 1;
}

static int lex_symbol(Lexer *lx) {
  size_t start = lx->pos;
  int line = lx->line;
  int col = lx->col;
  char c = peek(lx, 0);
  char next = peek(lx, 1);
  TokKind kind = TOK_EOF;
  size_t width = 1;
  switch (c) {
    case '(': kind = TOK_LPAREN; break;
    case ')': kind = TOK_RPAREN; break;
    case '*': kind = TOK_STAR; break;
    case '|': kind = TOK_BAR; break;
    case ':':
      kind = next == '=' ? TOK_DEFINE : TOK_COLON;
      width = next == '=' ? 2 : 1;
      break;
    case '-':
      kind = next == '>' ? TOK_ARROW : TOK_EOF;
      width = 2;
      break;
    case '=':
      kind = next == '>' ? TOK_FATARROW : TOK_EOF;
      width = 2;
      break;
    default: break;
  }
  /* L2 of --partial: a '-' or '=' at the end of input can still become '->'
     or '=>', so it is dropped. */
  if (kind == TOK_EOF && lx->partial && lx->pos + 1 == lx->len && (c == '-' || c == '=')) {
    advance(lx, 1);
    return 1;
  }
  if (kind == TOK_EOF) {
    return diag_fail(lx->diag, "LEX_CHAR", NULL, "%s:%d:%d: unexpected byte 0x%02x", lx->source, line, col, (unsigned)(unsigned char)c);
  }
  advance(lx, width);
  push(lx, kind, start, line, col);
  return 1;
}

/* PARTIAL selects the reading of lex_source_partial. */
static int lex_mode(Arena *arena, const char *source, const char *text, size_t len, int partial, TokenList *out, Diag *diag) {
  /* Each token takes at least one byte, and one EOF token follows. */
  Token *tokens = arena_alloc(arena, (len + 1) * sizeof(Token));
  if (tokens == NULL) return diag_fail(diag, "OOM", NULL, "%s: no memory for the tokens", source);
  Lexer lx = {
    .text = text, .len = len, .pos = 0, .line = 1, .col = 1,
    .out = tokens, .count = 0, .diag = diag, .source = source, .arena = arena, .partial = partial,
  };
  for (;;) {
    skip_trivia(&lx);
    if (lx.pos >= lx.len) break;
    char c = peek(&lx, 0);
    int ok = is_ident_start(c) ? lex_ident(&lx) : is_digit(c) ? lex_nat(&lx) : c == '"' ? lex_str(&lx) : lex_symbol(&lx);
    if (!ok) return 0;
  }
  /* L2 of --partial: a last token that touches the end of input can still
     grow (`1` to `12`, `re` to `refl`, `:` to `:=`), so it is dropped. */
  const Token *last = lx.count > 0 ? &lx.out[lx.count - 1] : NULL;
  lx.count -= partial && last != NULL && last->text + last->len == text + len && can_grow(last->kind) ? 1u : 0u;
  push(&lx, TOK_EOF, lx.pos, lx.line, lx.col);
  out->items = tokens;
  out->count = lx.count;
  return 1;
}

int lex_source(Arena *arena, const char *source, const char *text, size_t len, TokenList *out, Diag *diag) {
  return lex_mode(arena, source, text, len, 0, out, diag);
}

int lex_source_partial(Arena *arena, const char *source, const char *text, size_t len, TokenList *out, Diag *diag) {
  return lex_mode(arena, source, text, len, 1, out, diag);
}
