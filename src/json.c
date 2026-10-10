/* The JSON target. The tcc-json kit only. See json.h. */
#include <stdio.h>
#include <string.h>

#include "json.h"

#define JSON_TYPE_MAX 4096u

/* new-lang.sh replaces the placeholder with the language name. */
const char json_lang_name[] = "dil-lang";

typedef struct {
  Machine *m;
  const char *def; /* the current instance, for diagnostics */
  char *data;
  size_t len;
  size_t cap;
  unsigned depth;
} Out;

static int put(Out *o, const char *text, size_t n) {
  if (o->len + n + 1 > o->cap) {
    size_t cap = o->cap == 0 ? 4096 : o->cap;
    char *data;
    while (cap < o->len + n + 1)
      cap *= 2;
    data = arena_alloc(o->m->arena, cap);
    if (data == NULL)
      return diag_fail(o->m->diag, "OOM", o->def, "the arena limit is reached");
    if (o->len > 0)
      memcpy(data, o->data, o->len);
    o->data = data;
    o->cap = cap;
  }
  memcpy(o->data + o->len, text, n);
  o->len += n;
  o->data[o->len] = '\0';
  return 1;
}

static int put_text(Out *o, const char *text) {
  return put(o, text, strlen(text));
}

/* JSON text is UTF-8. A guest Str may hold arbitrary bytes, so reject a
   malformed sequence instead of emitting text that loses those bytes. */
static int utf8_valid(const char *text, size_t len) {
  static const unsigned long LEAST[5] = {0, 0, 0x80u, 0x800u, 0x10000u};
  const unsigned char *bytes = (const unsigned char *)text;
  size_t at = 0;
  while (at < len) {
    unsigned ch = bytes[at];
    size_t n = ch < 0x80u ? 1u : ch >= 0xc2u && ch <= 0xdfu ? 2u
      : ch >= 0xe0u && ch <= 0xefu ? 3u : ch >= 0xf0u && ch <= 0xf4u ? 4u : 0u;
    unsigned long cp;
    if (n == 0 || n > len - at)
      return 0;
    cp = n == 1 ? ch : ch & (0xffu >> (n + 1u));
    for (size_t i = 1; i < n; i++) {
      if ((bytes[at + i] & 0xc0u) != 0x80u)
        return 0;
      cp = cp << 6 | (bytes[at + i] & 0x3fu);
    }
    if (cp < LEAST[n] || cp > 0x10ffffu || (cp >= 0xd800u && cp <= 0xdfffu))
      return 0;
    at += n;
  }
  return 1;
}

/* A JSON string of the LEN bytes at TEXT. A quote, a backslash and a control
   byte become \u00XX; every other byte (UTF-8 too) is copied. */
static int put_bytes(Out *o, const char *text, size_t len) {
  char esc[8];
  size_t i;
  if (!utf8_valid(text, len))
    return diag_fail(o->m->diag, "JSON_UTF8", o->def, "a string holds bytes that are not UTF-8, which JSON text cannot hold");
  int ok = put(o, "\"", 1);
  for (i = 0; ok && i < len; i++) {
    unsigned char ch = (unsigned char)text[i];
    int plain = ch >= 0x20 && ch != '"' && ch != '\\';
    if (plain)
      ok = put(o, text + i, 1);
    else {
      snprintf(esc, sizeof esc, "\\u%04x", ch);
      ok = put(o, esc, 6);
    }
  }
  return ok && put(o, "\"", 1);
}

static int put_string(Out *o, const char *text) {
  return put_bytes(o, text, strlen(text));
}

static int refuse(Out *o, const char *what) {
  return diag_fail(o->m->diag, "JSON_VALUE", o->def, "the instance holds %s, which has no JSON form", what);
}

static int internal(Out *o, const char *what) {
  return diag_fail(o->m->diag, "INTERNAL", o->def, "the JSON printer found %s", what);
}

/* 1 when V is a number or a canonical value. A trap is a compile error. */
static int ready(Out *o, const Value *v) {
  switch (v->kind) {
  case VAL_NAT:
  case VAL_STR:
  case VAL_OP:
    return 1;
  case VAL_TRAP:
    return diag_fail(o->m->diag, "EVAL_OVERFLOW", o->def, "the instance value traps (a Nat overflow)");
  case VAL_UNIV:
  case VAL_PI:
  case VAL_SIGMA:
    return refuse(o, "a type");
  case VAL_LAM:
    return refuse(o, "a function");
  case VAL_VAR:
  case VAL_APP:
  case VAL_STUCK:
    return refuse(o, "a stuck term");
  }
  return internal(o, "an unknown value kind");
}

static int value_json(Out *o, const Value *type, const Value *v);

static int sum_json(Out *o, const Value *type, const Value *v) {
  if (val_is(v, OP_INL))
    return put_text(o, "{\"inl\":") && value_json(o, type->args[0], v->args[0]) && put_text(o, "}");
  if (val_is(v, OP_INR))
    return put_text(o, "{\"inr\":") && value_json(o, type->args[1], v->args[0]) && put_text(o, "}");
  return internal(o, "a Sum value that is not inl or inr");
}

/* none is null, so `some` wraps a value that can be null (ledger rule). */
static int option_json(Out *o, const Value *type, const Value *v) {
  const Value *elem = type->args[0];
  int wrap = val_is(elem, OP_OPTION) || val_is(elem, OP_EQ);
  if (val_is(v, OP_NONE))
    return put_text(o, "null");
  if (!val_is(v, OP_SOME))
    return internal(o, "an Option value that is not none or some");
  return (!wrap || put_text(o, "{\"some\":")) && value_json(o, elem, v->args[0]) && (!wrap || put_text(o, "}"));
}

/* Walks the spine, so a long list does not nest the C stack. */
static int list_json(Out *o, const Value *type, const Value *v) {
  const Value *cell = v;
  int first = 1;
  int ok = put_text(o, "[");
  while (ok && val_is(cell, OP_CONS)) {
    ok = (first || put_text(o, ",")) && value_json(o, type->args[0], cell->args[0]) && ready(o, cell->args[1]);
    cell = cell->args[1];
    first = 0;
  }
  if (!ok)
    return 0;
  return val_is(cell, OP_NIL) ? put_text(o, "]") : internal(o, "a List value that is not nil or cons");
}

static int is_enum(const Machine *m, const FamilyInfo *f) {
  uint32_t i;
  for (i = 0; i < f->ctor_count; i++)
    if (m->ctors[f->first_ctor + i].field_count != 0)
      return 0;
  return 1;
}

/* Only constants: the name. One constructor: an object of the fields.
   Otherwise: an object with "tag" and the fields. A field type is in the
   context of the family parameters, as in check.c. */
static int family_json(Out *o, const Value *type, const Value *v) {
  const FamilyInfo *f;
  const CtorInfo *ci;
  const Env *env = NULL;
  uint32_t i;
  int ok;
  if (type->inst >= o->m->family_count || !val_is(v, OP_CTOR) || v->inst >= o->m->ctor_count)
    return internal(o, "a family value that is not a constructor");
  f = &o->m->families[type->inst];
  ci = &o->m->ctors[v->inst];
  if (ci->family != type->inst || v->argc != f->param_count + ci->field_count || type->argc != f->param_count)
    return internal(o, "a constructor of another family");
  if (is_enum(o->m, f))
    return put_string(o, ci->name);
  for (i = 0; i < f->param_count; i++)
    env = env_push(o->m, env, type->args[i]);
  ok = put_text(o, "{");
  if (f->ctor_count != 1)
    ok = ok && put_text(o, "\"tag\":") && put_string(o, ci->name);
  for (i = 0; ok && i < ci->field_count; i++) {
    const FieldInfo *fi = &ci->fields[i];
    const Value *ft = fi->recursive ? type : eval_core(o->m, env, fi->type);
    int lead = i == 0 && f->ctor_count == 1;
    ok = ft != NULL && (lead || put_text(o, ",")) && put_string(o, fi->name) && put_text(o, ":")
      && value_json(o, ft, v->args[f->param_count + i]);
  }
  return ok && put_text(o, "}");
}

static int op_json(Out *o, const Value *type, const Value *v) {
  char num[24];
  switch (type->op) {
  case OP_NAT:
    if (v->kind != VAL_NAT)
      return internal(o, "a Nat value that is not a number");
    snprintf(num, sizeof num, "%llu", (unsigned long long)v->nat);
    return put_text(o, num);
  case OP_FLAG:
    if (val_is(v, OP_FLAG_YES))
      return put_text(o, "true");
    return val_is(v, OP_FLAG_NO) ? put_text(o, "false") : internal(o, "a Flag value that is not yes or no");
  case OP_STR:
    if (v->kind != VAL_STR)
      return internal(o, "a Str value that is not a string");
    return put_bytes(o, v->name, (size_t)v->nat);
  case OP_UNIT:
    return put_text(o, "{}");
  case OP_EQ:
    return put_text(o, "null");
  case OP_PROD:
    if (!val_is(v, OP_PAIR))
      return internal(o, "a Prod value that is not a pair");
    return put_text(o, "{\"first\":") && value_json(o, type->args[0], v->args[0]) && put_text(o, ",\"second\":")
      && value_json(o, type->args[1], v->args[1]) && put_text(o, "}");
  case OP_SUM:
    return sum_json(o, type, v);
  case OP_OPTION:
    return option_json(o, type, v);
  case OP_LIST:
    return list_json(o, type, v);
  case OP_FAMILY:
    return family_json(o, type, v);
  case OP_UNIT_VAL:
  case OP_FLAG_YES:
  case OP_FLAG_NO:
  case OP_PAIR:
  case OP_INL:
  case OP_INR:
  case OP_NONE:
  case OP_SOME:
  case OP_NIL:
  case OP_CONS:
  case OP_PACK:
  case OP_REFL:
  case OP_CTOR:
  case OP_FIRST:
  case OP_SECOND:
  case OP_EITHER:
  case OP_OPTION_ELIM:
  case OP_PURE:
  case OP_MAP:
  case OP_BIND:
  case OP_FOLD_NAT:
  case OP_FOLD_LIST:
  case OP_FOLD_FAMILY:
  case OP_UNFOLD:
  case OP_FILTER:
  case OP_WITNESS:
  case OP_PAYLOAD:
  case OP_SYMM:
  case OP_TRANS:
  case OP_TRANSPORT:
  case OP_CONG:
  case OP_NAT_ADD:
  case OP_NAT_SUB:
  case OP_NAT_MUL:
  case OP_NAT_EQ:
  case OP_NAT_LE:
  case OP_FLAG_IF:
  case OP_STR_EQ:
  case OP_SHOW_NAT:
  case OP_PROJ:
    return internal(o, "a type that is not a type former");
  }
  return internal(o, "an unknown operation");
}

/* The payload type is the Sigma body at the witness. */
static int pack_json(Out *o, const Value *type, const Value *v) {
  const Value *payload;
  if (!val_is(v, OP_PACK))
    return internal(o, "a Sigma value that is not pack");
  if (!put_text(o, "{\"witness\":") || !value_json(o, type->dom, v->args[0]))
    return 0;
  payload = closure_apply(o->m, type, v->args[0]);
  return payload != NULL && put_text(o, ",\"payload\":") && value_json(o, payload, v->args[1]) && put_text(o, "}");
}

static int typed_json(Out *o, const Value *type, const Value *v) {
  switch (type->kind) {
  case VAL_OP:
    return op_json(o, type, v);
  case VAL_SIGMA:
    return pack_json(o, type, v);
  case VAL_PI:
    return refuse(o, "a function");
  case VAL_UNIV:
    return refuse(o, "a type");
  case VAL_NAT:
  case VAL_STR:
  case VAL_TRAP:
  case VAL_LAM:
  case VAL_VAR:
  case VAL_APP:
  case VAL_STUCK:
    return refuse(o, "a value of a stuck type");
  }
  return internal(o, "an unknown type kind");
}

static int value_json(Out *o, const Value *type, const Value *v) {
  int ok;
  if (!ready(o, v))
    return 0;
  if (o->depth >= JSON_DEPTH_LIMIT)
    return diag_fail(o->m->diag, "JSON_DEPTH", o->def, "the instance nests deeper than %u levels", JSON_DEPTH_LIMIT);
  o->depth++;
  ok = typed_json(o, type, v);
  o->depth--;
  return ok;
}

/* Functions, types, families and proofs are not instances. */
int json_is_instance(const DefInfo *d) {
  if (d->origin != ORIGIN_PROGRAM)
    return 0;
  switch (d->type->kind) {
  case VAL_PI:
  case VAL_UNIV:
    return 0;
  case VAL_OP:
    return !val_is(d->type, OP_EQ);
  case VAL_SIGMA:
  case VAL_NAT:
  case VAL_STR:
  case VAL_TRAP:
  case VAL_LAM:
  case VAL_VAR:
  case VAL_APP:
  case VAL_STUCK:
    return 1;
  }
  return 1;
}

static int instance_json(Out *o, uint32_t index, char *type_text, int first) {
  Machine *m = o->m;
  const DefInfo *d = &m->defs[index];
  const Value *v;
  o->def = d->name;
  m->def = d->name;
  m->fuel = EVAL_FUEL_STEPS;
  m->depth = 0;
  v = def_value(m, index);
  if (v == NULL)
    return 0;
  if (!value_print(m, NULL, 0, d->type, type_text, JSON_TYPE_MAX))
    return diag_fail(m->diag, "JSON_TYPE", d->name, "the instance type exceeds the printer limits");
  return (first || put_text(o, ",")) && put_text(o, "{\"name\":") && put_string(o, d->name)
    && put_text(o, ",\"type\":") && put_string(o, type_text) && put_text(o, ",\"value\":")
    && value_json(o, d->type, v) && put_text(o, "}");
}

/* The app program (slice D3): the PROGRAM definitions app, init and events.
   The manifest comes before the instances. */

/* The index of the definition NAME (a PROGRAM definition when PROGRAM_ONLY). */
static int find_def(const Machine *m, const char *name, int program_only, uint32_t *out) {
  uint32_t i;
  for (i = 0; i < m->def_count; i++) {
    const DefInfo *d = &m->defs[i];
    if ((!program_only || d->origin == ORIGIN_PROGRAM) && strcmp(d->name, name) == 0) {
      *out = i;
      return 1;
    }
  }
  return 0;
}

static int domain_def(Machine *m, const char *name, uint32_t *out) {
  return find_def(m, name, 0, out) || diag_fail(m->diag, "INTERNAL", "app", "the domain has no definition %s", name);
}

static int find_ctor(Machine *m, const char *name, uint32_t *out) {
  uint32_t i;
  for (i = 0; i < m->ctor_count; i++) {
    if (strcmp(m->ctors[i].name, name) == 0) {
      *out = i;
      return 1;
    }
  }
  return diag_fail(m->diag, "INTERNAL", "app", "the domain has no constructor %s", name);
}

static int find_family(Machine *m, const char *name, uint32_t *out) {
  uint32_t i;
  for (i = 0; i < m->family_count; i++) {
    if (strcmp(m->families[i].name, name) == 0) {
      *out = i;
      return 1;
    }
  }
  return diag_fail(m->diag, "INTERNAL", "app", "the domain has no family %s", name);
}

/* 1 when definition INDEX has the type WANT (written WANT_TEXT). */
static int app_typed(Machine *m, uint32_t index, const Value *want, const char *want_text) {
  const DefInfo *d = &m->defs[index];
  char *text;
  if (want == NULL)
    return 0;
  if (conv_values(m, 0, d->type, want))
    return 1;
  text = arena_alloc(m->arena, JSON_TYPE_MAX);
  if (text == NULL)
    return diag_fail(m->diag, "OOM", d->name, "the arena limit is reached");
  if (!value_print(m, NULL, 0, d->type, text, JSON_TYPE_MAX))
    return diag_fail(m->diag, "BUILD_APP", d->name, "%s has a type that does not print; an app program needs %s : %s", d->name, d->name, want_text);
  return diag_fail(m->diag, "BUILD_APP", d->name, "%s has the type %s; an app program needs %s : %s", d->name, text, d->name, want_text);
}

/* The name of the Key value K (the constructor key over a Str), or NULL. */
static const Value *key_name(const AppInfo *a, const Value *k) {
  if (k == NULL || k->kind != VAL_OP || !val_is(k, OP_CTOR) || k->inst != a->key || k->argc < 1)
    return NULL;
  return k->args[0]->kind == VAL_STR ? k->args[0] : NULL;
}

/* The key name of a list element: the first of a pair in init, the key in events. */
static const Value *element_key(const AppInfo *a, const Value *head, int entries) {
  if (!entries)
    return key_name(a, head);
  if (head == NULL || head->kind != VAL_OP || !val_is(head, OP_PAIR) || head->argc < 2)
    return NULL;
  return key_name(a, head->args[0]);
}

static int same_str(const Value *a, const Value *b) {
  return a->nat == b->nat && memcmp(a->name, b->name, (size_t)a->nat) == 0;
}

/* Walks LIST (init when ENTRIES, else events) and refuses a key that occurs
   twice. A trap is a compile error, as for an instance. */
static int app_distinct(Machine *m, const AppInfo *a, const Value *list, int entries) {
  const char *def = entries ? "init" : "events";
  const Value *outer;
  for (outer = list; outer != NULL; outer = outer->args[1]) {
    const Value *name;
    const Value *inner;
    if (outer->kind == VAL_TRAP)
      return diag_fail(m->diag, "EVAL_OVERFLOW", def, "the instance value traps (a Nat overflow)");
    if (outer->kind != VAL_OP || !(val_is(outer, OP_NIL) || (val_is(outer, OP_CONS) && outer->argc >= 2)))
      return diag_fail(m->diag, "INTERNAL", def, "the app checker found a value that is not a list");
    if (val_is(outer, OP_NIL))
      return 1;
    name = element_key(a, outer->args[0], entries);
    if (name == NULL)
      return diag_fail(m->diag, "INTERNAL", def, "the app checker found an element without a key");
    for (inner = list; inner != outer; inner = inner->args[1]) {
      const Value *other = element_key(a, inner->args[0], entries);
      if (other != NULL && same_str(name, other))
        return entries
          ? diag_fail(m->diag, "BUILD_APP", def, "the key \"%.*s\" has two entries in init; the cells are a map", (int)name->nat, name->name)
          : diag_fail(m->diag, "BUILD_APP", def, "the key \"%.*s\" is listed twice in events", (int)name->nat, name->name);
    }
  }
  return 1;
}

/* The value of definition INDEX, evaluated as instance_json does. */
static const Value *app_value(Machine *m, uint32_t index) {
  m->def = m->defs[index].name;
  m->fuel = EVAL_FUEL_STEPS;
  m->depth = 0;
  return def_value(m, index);
}

int json_app_info(Machine *m, AppInfo *out) {
  uint32_t meaning, state, key_family;
  const Value *key_type;
  const Value *init;
  const Value *events;
  memset(out, 0, sizeof *out);
  if (!find_def(m, "app", 1, &out->app))
    return 1;
  out->present = 1;
  if (!find_def(m, "init", 1, &out->init))
    return diag_fail(m->diag, "BUILD_APP", "app", "the app program needs a definition init : State");
  if (!find_def(m, "events", 1, &out->events))
    return diag_fail(m->diag, "BUILD_APP", "app", "the app program needs a definition events : List Key");
  if (!domain_def(m, "Meaning", &meaning) || !domain_def(m, "State", &state) || !domain_def(m, "viewOf", &out->view_of)
      || !domain_def(m, "transOf", &out->trans_of) || !find_family(m, "Key", &key_family) || !find_family(m, "UITree", &out->ui_tree)
      || !find_ctor(m, "key", &out->key) || !find_ctor(m, "vNat", &out->v_nat) || !find_ctor(m, "vFlag", &out->v_flag)
      || !find_ctor(m, "vText", &out->v_text) || !find_ctor(m, "event", &out->event) || !find_ctor(m, "tEnd", &out->t_end)
      || !find_ctor(m, "text", &out->text) || !find_ctor(m, "comp", &out->comp))
    return 0;
  m->def = "app";
  m->fuel = EVAL_FUEL_STEPS;
  m->depth = 0;
  key_type = val_make(m, OP_FAMILY, key_family, 0, NULL, NULL, NULL);
  if (!app_typed(m, out->app, app_value(m, meaning), "Meaning") || !app_typed(m, out->init, app_value(m, state), "State")
      || !app_typed(m, out->events, key_type == NULL ? NULL : val_make(m, OP_LIST, 0, 1, key_type, NULL, NULL), "List Key"))
    return 0;
  init = app_value(m, out->init);
  events = app_value(m, out->events);
  return init != NULL && events != NULL && app_distinct(m, out, init, 1) && app_distinct(m, out, events, 0);
}

/* One cell of the manifest: the init entry ENTRY, a pair of a key and a value. */
static int cell_json(Out *o, const AppInfo *a, const Value *entry, int first) {
  const Value *key = entry->args[0];
  const Value *val = entry->args[1];
  const Value *field_type;
  const char *type;
  if (!ready(o, val))
    return 0;
  if (key_name(a, key) == NULL || !val_is(val, OP_CTOR) || val->argc < 1)
    return internal(o, "a cell that is not a key and a value");
  type = val->inst == a->v_nat ? "Nat" : val->inst == a->v_flag ? "Flag" : val->inst == a->v_text ? "Str" : NULL;
  if (type == NULL)
    return internal(o, "a cell value with an unknown constructor");
  field_type = eval_core(o->m, NULL, o->m->ctors[val->inst].fields[0].type);
  return field_type != NULL && (first || put_text(o, ",")) && put_text(o, "{\"key\":")
    && put_bytes(o, key->args[0]->name, (size_t)key->args[0]->nat) && put_text(o, ",\"type\":\"") && put_text(o, type)
    && put_text(o, "\",\"init\":") && value_json(o, field_type, val->args[0]) && put_text(o, "}");
}

/* The component names written so far (Str values). */
typedef struct {
  const Value **names;
  uint32_t count;
  uint32_t cap;
} Seen;

/* Adds NAME when it is new; ADDED tells. */
static int seen_add(Out *o, Seen *s, const Value *name, int *added) {
  uint32_t i;
  *added = 0;
  for (i = 0; i < s->count; i++) {
    if (same_str(s->names[i], name))
      return 1;
  }
  if (s->count == s->cap) {
    uint32_t cap = s->cap == 0 ? 16u : s->cap * 2u;
    const Value **next = arena_alloc(o->m->arena, cap * sizeof *next);
    if (next == NULL)
      return diag_fail(o->m->diag, "OOM", o->def, "the arena limit is reached");
    if (s->count > 0)
      memcpy(next, s->names, s->count * sizeof *next);
    s->names = next;
    s->cap = cap;
  }
  s->names[s->count] = name;
  s->count++;
  *added = 1;
  return 1;
}

/* Writes the new component names of the tree T in pre-order: the node, its
   kids, then its siblings. The kids recurse; the siblings loop. */
static int components_json(Out *o, const AppInfo *a, Seen *s, const Value *t) {
  if (o->depth >= JSON_DEPTH_LIMIT)
    return diag_fail(o->m->diag, "JSON_DEPTH", o->def, "the tree is deeper than %u", JSON_DEPTH_LIMIT);
  while (t != NULL) {
    const Value *name;
    int added;
    if (!ready(o, t))
      return 0;
    if (!val_is(t, OP_CTOR))
      return internal(o, "a tree node that is not a constructor");
    if (t->inst == a->t_end)
      return 1;
    if (t->inst == a->text) {
      if (t->argc < 2)
        return internal(o, "a text node without its fields");
      t = t->args[1];
      continue;
    }
    if (t->inst != a->comp || t->argc < 4)
      return internal(o, "a tree node of an unknown constructor");
    name = t->args[0];
    if (!ready(o, name))
      return 0;
    if (!val_is(name, OP_CTOR) || name->argc < 1 || name->args[0]->kind != VAL_STR)
      return internal(o, "a component name that is not a Str");
    if (!seen_add(o, s, name->args[0], &added))
      return 0;
    if (added && !((s->count == 1 || put_text(o, ",")) && put_bytes(o, name->args[0]->name, (size_t)name->args[0]->nat)))
      return 0;
    o->depth++;
    if (!components_json(o, a, s, t->args[2]))
      return 0;
    o->depth--;
    t = t->args[3];
  }
  return internal(o, "a missing tree node");
}

/* The manifest: the cells of init, the event keys, the component names and
   the tree viewOf app init. The writer ends on the comma before instances. */
static int manifest_json(Out *o, const AppInfo *a) {
  Machine *m = o->m;
  const Value *init;
  const Value *events;
  const Value *view_of;
  const Value *app;
  const Value *tree;
  const Value *tree_type;
  const Value *v;
  Seen seen;
  int first = 1;
  memset(&seen, 0, sizeof seen);
  o->def = "app";
  init = app_value(m, a->init);
  events = app_value(m, a->events);
  view_of = app_value(m, a->view_of);
  app = app_value(m, a->app);
  m->def = "app";
  m->fuel = EVAL_FUEL_STEPS;
  m->depth = 0;
  tree = apply_value(m, apply_value(m, view_of, app), init);
  tree_type = val_make(m, OP_FAMILY, a->ui_tree, 0, NULL, NULL, NULL);
  if (init == NULL || events == NULL || tree == NULL || tree_type == NULL || !put_text(o, "\"cells\":["))
    return 0;
  for (v = init; val_is(v, OP_CONS); v = v->args[1]) {
    if (!cell_json(o, a, v->args[0], first))
      return 0;
    first = 0;
  }
  first = 1;
  if (!put_text(o, "],\"events\":["))
    return 0;
  for (v = events; val_is(v, OP_CONS); v = v->args[1]) {
    const Value *name = key_name(a, v->args[0]);
    if (name == NULL)
      return internal(o, "an event key that is not a Str");
    if (!(first || put_text(o, ",")) || !put_bytes(o, name->name, (size_t)name->nat))
      return 0;
    first = 0;
  }
  return put_text(o, "],\"components\":[") && components_json(o, a, &seen, tree) && put_text(o, "],\"tree\":")
    && value_json(o, tree_type, tree) && put_text(o, ",");
}

int json_document(Machine *m, const char **text, size_t *len) {
  Out o;
  AppInfo a;
  char *type_text = arena_alloc(m->arena, JSON_TYPE_MAX);
  uint32_t i;
  int first = 1;
  int ok;
  memset(&o, 0, sizeof o);
  o.m = m;
  if (type_text == NULL)
    return diag_fail(m->diag, "OOM", NULL, "the arena limit is reached");
  if (!json_app_info(m, &a))
    return 0;
  ok = put_text(&o, "{") && put_string(&o, json_lang_name) && put_text(&o, ":1,") && (!a.present || manifest_json(&o, &a))
    && put_text(&o, "\"instances\":[");
  for (i = 0; ok && i < m->def_count; i++) {
    if (json_is_instance(&m->defs[i]) && !(a.present && i == a.app)) {
      ok = instance_json(&o, i, type_text, first);
      first = 0;
    }
  }
  if (!ok || !put_text(&o, "]}\n"))
    return 0;
  *text = o.data;
  *len = o.len;
  return 1;
}
