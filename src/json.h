/* The JSON target. The tcc-json kit only (not shared with tcc-wasm).

   `langc build` checks the program, evaluates each instance and gives one
   JSON document: {"LANG":1,"instances":[{"name","type","value"}]}. An
   instance is a PROGRAM definition whose type is data: not a function type,
   not a universe (types and families) and not an equality (proofs). Values
   carry no types, so the printer walks the type and the value together. */
#ifndef LANG_JSON_H
#define LANG_JSON_H

#include <stddef.h>

#include "front/core.h"

#define JSON_DEPTH_LIMIT 2000u

/* The first key of the document: the language name. */
extern const char json_lang_name[];

/* Builds the document for the checked program M in the arena of M. Returns
   1 and sets TEXT and LEN, or 0 after a diagnostic (then no document). */
int json_document(Machine *m, const char **text, size_t *len);
/* 1 when D is an instance (the JS target writes the same document). */
int json_is_instance(const DefInfo *d);

/* The app program (slice D3): the PROGRAM definitions app, init and events,
   and the domain names the manifest and the JS exports use. */
typedef struct {
  int present; /* 1 when the program defines app */
  uint32_t app;
  uint32_t init;
  uint32_t events;
  uint32_t view_of;  /* the domain definition viewOf */
  uint32_t trans_of; /* the domain definition transOf */
  uint32_t key, v_nat, v_flag, v_text, event, t_end, text, comp; /* constructors */
  uint32_t ui_tree; /* the family UITree */
} AppInfo;
/* Finds app, init and events and checks them. Returns 1 (and PRESENT 0 or
   1), or 0 after a BUILD_APP diagnostic. */
int json_app_info(Machine *m, AppInfo *out);

#endif
