#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE 1
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>

static const char *fail_install;
int test_rename(const char *from, const char *to) {
  if (fail_install != NULL && strcmp(to, fail_install) == 0) {
    fail_install = NULL;
    errno = EIO;
    return -1;
  }
  return rename(from, to);
}

#define rename test_rename
#define main langc_main
#include "../src/main.c"
#undef main
#undef rename
#include "../src/json.c"

#include <unistd.h>

static unsigned checked;
static unsigned failed;

static void expect(int condition, const char *message) {
  checked++;
  if (!condition) {
    failed++;
    fprintf(stderr, "FAIL %s\n", message);
  }
}

static void output_options(char *left, char *right, int accepted) {
  Options opt;
  Diag diag;
  char *args[] = {"langc", "build", "unused.lang", "--js", left, "--json", right};
  int result;
  diag_init(&diag);
  result = parse_options(7, args, &opt, &diag);
  expect(result == accepted && (accepted || strcmp(diag.code, "USAGE") == 0),
    accepted ? "distinct outputs were refused" : "overlapping outputs were accepted");
}

static int invoke(int argc, char **argv, const char *code) {
  FILE *out = tmpfile();
  FILE *err = tmpfile();
  int saved_out = dup(STDOUT_FILENO);
  int saved_err = dup(STDERR_FILENO);
  int status;
  char message[512] = {0};
  if (out == NULL || err == NULL || saved_out < 0 || saved_err < 0)
    return -1;
  fflush(stdout);
  fflush(stderr);
  if (dup2(fileno(out), STDOUT_FILENO) < 0 || dup2(fileno(err), STDERR_FILENO) < 0)
    return -1;
  status = langc_main(argc, argv);
  fflush(stdout);
  fflush(stderr);
  expect(ftell(out) == 0, "file build or refusal wrote to stdout");
  rewind(err);
  (void)fread(message, 1, sizeof message - 1, err);
  (void)dup2(saved_out, STDOUT_FILENO);
  (void)dup2(saved_err, STDERR_FILENO);
  close(saved_out);
  close(saved_err);
  fclose(out);
  fclose(err);
  expect(code == NULL ? message[0] == '\0' : strstr(message, code) != NULL,
    "build returned the wrong diagnostic");
  return status;
}

static int write_file(const char *path, const char *text) {
  FILE *file = fopen(path, "wb");
  size_t len = strlen(text);
  int written;
  int closed;
  if (file == NULL)
    return 0;
  written = fwrite(text, 1, len, file) == len;
  closed = fclose(file) == 0;
  return written && closed;
}

static int file_equals(const char *path, const char *text) {
  char contents[64] = {0};
  FILE *file = fopen(path, "rb");
  size_t len;
  if (file == NULL) return 0;
  len = fread(contents, 1, sizeof contents, file);
  fclose(file);
  return len == strlen(text) && memcmp(contents, text, len) == 0;
}

static void output_failure_tests(char *source, char *target, char *other, char *symbolic) {
  char *args[] = {"langc", "build", source, "--js", target, "--json", other};
  struct stat info;
  expect(mkdir(other, 0700) == 0, "cannot prepare failed JSON destination");
  expect(write_file(target, "previous JS"), "cannot prepare previous JS artifact");
  expect(invoke(7, args, "IO") == 2, "failed JSON destination did not exit 2");
  expect(file_equals(target, "previous JS"), "failed JSON write destroyed the previous JS artifact");
  unlink(target);
  expect(invoke(7, args, "IO") == 2, "failed new-output build did not exit 2");
  expect(stat(target, &info) != 0, "failed JSON write left a new JS artifact");
  rmdir(other);
  expect(write_file(target, "previous JS") && write_file(other, "previous JSON"),
    "cannot prepare previous output pair");
  fail_install = other;
  expect(invoke(7, args, "IO") == 2, "failed second install did not exit 2");
  expect(file_equals(target, "previous JS") && file_equals(other, "previous JSON"),
    "failed second install did not restore the previous output pair");
  unlink(target);
  fail_install = other;
  expect(invoke(7, args, "IO") == 2, "failed new-file install did not exit 2");
  expect(stat(target, &info) != 0 && file_equals(other, "previous JSON"),
    "failed second install did not restore absent and existing destinations");
  unlink(other);
  expect(symlink("out", symbolic) == 0, "cannot prepare symbolic output");
  args[4] = symbolic;
  expect(invoke(7, args, NULL) == 0, "build through dangling symbolic output failed");
  expect(lstat(symbolic, &info) == 0 && S_ISLNK(info.st_mode) && stat(target, &info) == 0,
    "successful output replacement discarded its symbolic link");
  unlink(symbolic);
  unlink(target);
  unlink(other);
}

static int checked_program(Arena *arena, Diag *diag, Machine *m, const char *domain, const char *text) {
  DeclList decls = {0};
  TokenList tokens;
  diag_init(diag);
  return lex_source(arena, "domain", domain, strlen(domain), &tokens, diag)
    && parse_source(arena, "domain", ORIGIN_DOMAIN, &tokens, &decls, diag)
    && lex_source(arena, "program", text, strlen(text), &tokens, diag)
    && parse_source(arena, "program", ORIGIN_PROGRAM, &tokens, &decls, diag)
    && check_program(arena, &decls, m, diag);
}

static void group_test(const char *domain, const char *text, const char *message) {
  Arena arena;
  Machine m;
  Diag diag;
  FILE *out = tmpfile();
  FILE *err = tmpfile();
  char result[16] = {0};
  int ok;
  arena_init(&arena, (size_t)1 << 24);
  ok = out != NULL && err != NULL && checked_program(&arena, &diag, &m, domain, text);
  if (ok) {
    ok = eval_command(&m, "result", NULL, 0, out, err) == 0;
    rewind(out);
    (void)fread(result, 1, sizeof result - 1u, out);
    ok = ok && strcmp(result, "7\n") == 0;
  }
  expect(ok, message);
  if (out != NULL) fclose(out);
  if (err != NULL) fclose(err);
  arena_release(&arena);
}

static void grouped_binder_tests(void) {
  group_test("", "def grouped : (A : Type 0) -> (A x : A) -> Nat := fun (A : Type 0) (A x : A) => 7\n"
    "def result : Nat := grouped Nat 1 2\n", "typed binder groups captured an annotation name");
  group_test("", "def result : Nat := (fun (A : Type 0) (A x : A) => 7) Nat 1 2\n",
    "inferred binder groups captured an annotation name");
  group_test("", "def packed : Sigma (A : Type 0) ((A x : A) * Nat) := pack Nat (pack 1 (pack 2 7))\n"
    "def result : Nat := payload (payload (payload packed))\n", "Sigma binder groups captured an annotation name");
  group_test("family Box (A : Type 0) (A x : A) := makeBox (amount : Nat)\n",
    "def box : Box Nat 1 2 := makeBox Nat 1 2 7\ndef result : Nat := amount box\n",
    "family parameter groups captured an annotation name");
  {
    Arena arena;
    Machine m;
    Diag diag;
    arena_init(&arena, (size_t)1 << 24);
    expect(!checked_program(&arena, &diag, &m, "",
      "def bad : (A : Type 0) -> (A x : A) -> Nat := fun A (A x : Nat) => 7\n")
      && diag.code != NULL && strcmp(diag.code, "TYPE_MISMATCH") == 0,
      "group scope repair accepted an incompatible annotation");
    arena_release(&arena);
  }
}

static void doc_error_test(void) {
  FILE *out = tmpfile();
  pid_t child = out == NULL ? -1 : fork();
  int status = -1;
  if (child == 0) {
    if (dup2(fileno(out), STDOUT_FILENO) < 0 || dup2(fileno(out), STDERR_FILENO) < 0) _exit(127);
    execlp("make", "make", "-s", "doc-check", "RG=rg --invalid-review-option", (char *)NULL);
    _exit(127);
  }
  if (child > 0 && waitpid(child, &status, 0) != child) status = -1;
  expect(status != -1 && WIFEXITED(status) && WEXITSTATUS(status) != 0 && WEXITSTATUS(status) != 127,
    "doc-check reports success when its search command fails");
  if (out != NULL) fclose(out);
}

static void family_selftest(char *directory) {
  Arena arena;
  Machine m;
  Diag diag;
  const char *module = NULL;
  size_t len = 0;
  char path[FILENAME_MAX];
  FILE *file;
  int status = -1;
  pid_t child;
  arena_init(&arena, (size_t)1 << 24);
  if (!checked_program(&arena, &diag, &m, "family Phantom (fn : Nat -> Nat) := makePhantom\n",
      "def identity : Nat -> Nat := fun n => n\n"
      "def value : Phantom identity := makePhantom identity\n"
      "def law : Eq (Phantom identity) value (makePhantom (fun (n : Nat) => n)) := refl\n")
      || !js_module(&m, 1, &module, &len)) {
    expect(0, "cannot compile function-indexed family selftest");
    arena_release(&arena);
    return;
  }
  snprintf(path, sizeof path, "%s/selftest.mjs", directory);
  file = fopen(path, "wb");
  if (file != NULL) {
    int ok = fwrite(module, 1, len, file) == len;
    ok = fclose(file) == 0 && ok;
    if (ok) {
      child = fork();
      if (child == 0) {
        execlp("node", "node", path, (char *)NULL);
        _exit(127);
      }
      if (child > 0 && waitpid(child, &status, 0) != child) status = -1;
    }
  }
  expect(status != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 0,
    "generated selftest falsely rejects equal function-indexed family values");
  unlink(path);
  arena_release(&arena);
}

static void writer_tests(void) {
  Diag diag;
  Arena arena;
  Machine machine;
  const char *bad[] = {"\200", "\301\201", "\342\202", "\355\240\200", "\364\220\200\200"};
  const size_t lengths[] = {1, 2, 2, 3, 4};
  const char valid[] = "\000\177\302\200\342\202\254\360\237\230\200";
  arena_init(&arena, (size_t)1 << 20);
  memset(&machine, 0, sizeof machine);
  machine.arena = &arena;
  machine.diag = &diag;
  for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
    Out out;
    memset(&out, 0, sizeof out);
    out.m = &machine;
    out.def = "bad";
    diag_init(&diag);
    expect(!put_bytes(&out, bad[i], lengths[i]) && strcmp(diag.code, "JSON_UTF8") == 0,
      "invalid UTF-8 was accepted by the JSON writer");
  }
  {
    Out out;
    memset(&out, 0, sizeof out);
    out.m = &machine;
    diag_init(&diag);
    expect(put_bytes(&out, valid, sizeof valid - 1) && out.len == sizeof valid + 6
      && memcmp(out.data, "\"\\u0000", 7) == 0
      && memcmp(out.data + 7, valid + 1, sizeof valid - 2) == 0
      && out.data[out.len - 1] == '"', "valid UTF-8 lost its JSON bytes");
  }
  arena_release(&arena);
}

int main(void) {
  char directory[] = "/tmp/langc-backend-XXXXXX";
  char target[FILENAME_MAX];
  char alias[FILENAME_MAX];
  char hard[FILENAME_MAX];
  char symbolic[FILENAME_MAX];
  char other[FILENAME_MAX];
  char source[FILENAME_MAX];
  struct stat info;
  FILE *file;
  char contents[16] = {0};
  if (mkdtemp(directory) == NULL)
    return 2;
  snprintf(target, sizeof target, "%s/out", directory);
  snprintf(alias, sizeof alias, "%s/./out", directory);
  snprintf(hard, sizeof hard, "%s/hard", directory);
  snprintf(symbolic, sizeof symbolic, "%s/symbolic", directory);
  snprintf(other, sizeof other, "%s/other", directory);
  snprintf(source, sizeof source, "%s/source.lang", directory);
  output_options(target, target, 0);
  output_options(target, alias, 0);
  output_options(target, other, 1);
  expect(symlink(target, symbolic) == 0, "cannot prepare dangling symbolic-link alias");
  output_options(target, symbolic, 0);
  unlink(symbolic);
  expect(symlink("out", symbolic) == 0, "cannot prepare relative dangling alias");
  output_options(target, symbolic, 0);
  unlink(symbolic);
  expect(symlink("out", hard) == 0 && symlink("hard", symbolic) == 0,
    "cannot prepare chained dangling alias");
  output_options(target, symbolic, 0);
  unlink(hard);
  unlink(symbolic);
  expect(write_file(target, "preserve"), "cannot prepare existing output");
  expect(link(target, hard) == 0, "cannot prepare hard-link alias");
  expect(symlink(target, symbolic) == 0, "cannot prepare symbolic-link alias");
  output_options(target, hard, 0);
  output_options(target, symbolic, 0);
  expect(write_file(source, "def valid : Nat := 7\n"), "cannot prepare valid program");
  {
    char *args[] = {"langc", "build", source, "--js", target, "--json", hard};
    expect(invoke(7, args, "USAGE") == 2, "aliased output build did not exit 2");
  }
  file = fopen(target, "rb");
  if (file != NULL) {
    (void)fread(contents, 1, sizeof contents - 1, file);
    fclose(file);
  }
  expect(file != NULL && strcmp(contents, "preserve") == 0, "output collision changed an existing file");
  unlink(target);
  unlink(hard);
  unlink(symbolic);
  {
    char *args[] = {"langc", "build", source, "--js", target, "--json", other};
    expect(invoke(7, args, NULL) == 0, "distinct output build failed");
    expect(stat(target, &info) == 0 && stat(other, &info) == 0, "distinct build omitted an output");
  }
  unlink(target);
  unlink(other);
  expect(symlink("out", symbolic) == 0, "cannot prepare dangling build output");
  {
    char *args[] = {"langc", "build", source, "--js", symbolic, "--json", target};
    expect(invoke(7, args, "USAGE") == 2, "dangling output collision did not exit 2");
    expect(stat(target, &info) != 0, "dangling output collision created its target");
  }
  unlink(symbolic);
  output_failure_tests(source, target, other, symbolic);
  expect(write_file(source, "def firstByte : Str := \"\200\"\ndef secondByte : Str := \"\201\"\n"),
    "cannot prepare invalid UTF-8 program");
  {
    char *args[] = {"langc", "build", source};
    expect(invoke(3, args, "JSON_UTF8") == 1, "JSON stdout build did not refuse invalid UTF-8");
  }
  {
    char *args[] = {"langc", "build", source, "-o", other};
    expect(invoke(5, args, "JSON_UTF8") == 1, "JSON-only build did not refuse invalid UTF-8");
    expect(stat(other, &info) != 0, "JSON UTF-8 refusal created an output");
  }
  {
    char *args[] = {"langc", "build", source, "--js", target, "--json", other};
    expect(invoke(7, args, "JS_UTF8") == 1, "JS build changed its UTF-8 diagnostic");
    expect(stat(target, &info) != 0 && stat(other, &info) != 0, "JS UTF-8 refusal created an output");
  }
  writer_tests();
  grouped_binder_tests();
  family_selftest(directory);
  doc_error_test();
  unlink(source);
  expect(rmdir(directory) == 0, "backend tests left temporary outputs behind");
  printf("backend regressions: %u checked, %u failed\n", checked, failed);
  return failed != 0;
}
