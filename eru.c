#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <wchar.h>
#include <unistd.h>
#include <locale.h>
#include <wctype.h>
#include <limits.h>
#include <inttypes.h>

typedef enum {
  LAMBDA,
  APP,
  VAL
} exp_t;

typedef struct Exp {
  exp_t t;
  void *arg; /* Exp* | size_t (interned name) */
  struct Exp *body;
} Exp;

typedef struct Env {
  size_t key; /* interned */
  Exp *val;
  struct Env *next;
} Env;

static wchar_t *read_file(void *, uint8_t);
static void     print_exp(Exp *);
static Exp     *apply_env(Env *, Exp *);
static Env     *parse(wchar_t *, Env *);
static void     print_env(Env *);
static Exp     *reduce(Env *, Exp *);

/* TODO: make this reallocatable but *not* a linked list */
#define SYMBOLS_MAX 1<<13

static char *argv0 = NULL;
static size_t vflag = 0;

static size_t mem_allocated = 0;
static wchar_t *intern_pool[SYMBOLS_MAX] = {0};

static inline void *
eru_malloc(size_t n)
{
  mem_allocated += n;
  return malloc(n);
}

static inline wchar_t *
symbol_name(size_t n)
{
  return intern_pool[n];
}

static size_t
intern(wchar_t *sym)
{
  size_t n = 0;
  while (1) {
    if (symbol_name(n)) {
      if (wcscmp(sym, symbol_name(n)) == 0)
        return n;
      else
        n++;
    } else {
      intern_pool[n] = sym;
      return n;
    }
  }
}

static wchar_t*
upto(wchar_t* s, wchar_t delim, wchar_t **rest)
{
  wchar_t *res = wcsdup(s), *v = res;
  size_t i = 0, paren = 0;
 further:
  do {
    if (*v == L'(') paren++;
    if (*v == L')') paren--;
    ++i, ++v;
  } while (*v != delim && *v);
  if (paren > 0)
    goto further;

  if (*v) *v = 0;
  *rest = s+i+1;
  return res;
}

static Exp*
parse_exp(wchar_t *s, Env *env)
{
  Exp *e = eru_malloc(sizeof(Exp));
  wchar_t *rest = 0;

  switch (*s) {
  case L'#':
    return reduce(env, parse_exp(s+1, env));
  case L'λ':
  case L'\\':
    e->t = LAMBDA;
    e->arg = (void*)intern(upto(s+1, L'.', &rest));
    e->body = parse_exp(rest, env);
    break;
  case L'(':
    e->t = APP;
    e->arg = parse_exp(upto(s+1, L' ', &rest), env);
    e->body = parse_exp(upto(rest, L')', &rest), env);
    break;
  default:
    e->t = VAL;
    e->arg = (void*)intern(s);
    e->body = NULL;
    break;
  }

  return e;
}

/* TODO: find more memory leaks */
static void
free_exp(Exp *e)
{
  if (!e)
    return;
  if (e->t == APP) {
    free_exp(e->arg);
    free(e->arg);
  } else {/* We don't deallocate strings */}

  if (e->t != VAL) {
    free_exp(e->body);
    free(e->body);
  }
}

static Exp *
dupexp(Exp *e)
{
  Exp *r = eru_malloc(sizeof(Exp));
  r->t = e->t;
  if (r->t == APP)
    r->arg = dupexp(e->arg);
  else
    r->arg = e->arg; /* i don't think i have to strdup here */

  if (r->t != VAL)
    r->body = dupexp(e->body);
  return r;
}


static void
_print_exp(Exp *e, size_t depth)
{
  if (e == NULL)
    return;
  switch (e->t) {
  case LAMBDA:
    fprintf(stderr, "λ");
    fprintf(stderr, "%ls", symbol_name((size_t)e->arg));
    fprintf(stderr, ".");
    _print_exp(e->body, depth+1);
    break;
  case APP:
    fprintf(stderr, "(");
    _print_exp(e->arg, depth+1);
    fprintf(stderr, " ");
    _print_exp(e->body, depth+1);
    fprintf(stderr, ")");
    break;
  case VAL:
    fprintf(stderr, "%ls", symbol_name((size_t)e->arg));
    break;
  }
}

static void
print_exp(Exp *e)
{
  _print_exp(e, 0);
  putc('\n', stderr);
}

static Exp *
replace(Exp *e, wchar_t *arg, Exp *val)
{
  switch (e->t) {
  case LAMBDA:
    if (e->arg == arg) { /* shadowing */
      return e;
    } else {
      e->body = replace(e->body, arg, val);
      return e;
    }
  case APP:
    e->arg  = replace(e->arg, arg, val);
    e->body = replace(e->body, arg, val);
    return e;
  case VAL:
    if (e->arg == arg) {
      free_exp(e);
      memcpy(e, dupexp(val), sizeof(Exp));
      /* if (e->t != VAL) */
      /*   return replace(e, arg, val); */
      return e;
    }
    return e;
  default:
    abort();
    return e;
  }
}

static Exp *
apply(Exp *e, Exp *val)
{
  assert(e->t == LAMBDA);
  return replace(e->body, e->arg, val);
}

static Exp *
walk_apply(Exp *e, int *subs)
{
  if (e == NULL)
    return e;
  switch (e->t) {
  case LAMBDA:
    e->body = walk_apply(e->body, subs);
    return e;
  case APP:
    if (((Exp*)e->arg)->t == LAMBDA) {
      Exp *new = apply(e->arg, e->body);
      memcpy(e, new, sizeof(Exp));
      /* memcpy(e, dupexp(new), sizeof(Exp)); */ // is this needed here?
      (*subs)++;
      return e;
    }
    else {
      walk_apply(e->arg, subs);
      walk_apply(e->body, subs);
      /* fallthrough */
    }
  case VAL:
    /* fallthrough */
  default:
    return e;
  }
}

static size_t
n_symbols(void)
{
  size_t i = 0;
  while (symbol_name(i++))
    ;
  return i-1;
}

static Exp *
reduce(Env *env, Exp *e)
{
  int subs = 0;
  size_t n_reduce = 0, n_subst = 0, mem_was = mem_allocated;
  e = apply_env(env, e);
  do {
    if (vflag >= 2) {
      fprintf(stderr, "Step %zu: ", n_reduce+1);
      print_exp(e);
    }

    subs = 0;
    walk_apply(e, &subs);
    n_reduce++, n_subst += subs;
  } while (subs);

  if (vflag)
    fprintf(stderr, "Reduced in %zu passes, %zu substitutions. %zu bytes allocated. %zu symbols used.\n", n_reduce, n_subst, mem_allocated - mem_was, n_symbols());

  return e;
}

static Exp *
apply_env(Env *env, Exp *e)
{
  while (env) {
    Exp *tmp = eru_malloc(sizeof(Exp)), *app = malloc(sizeof(Exp));
    tmp->t = LAMBDA;
    tmp->arg = (void*)env->key;
    tmp->body = e;
    app->t = APP;
    app->arg = tmp;
    /* app->body = dupexp(env->val); */
    app->body = env->val;
    e = app;
    env = env->next;
  }

  return e;
}

static Env *
maybe_parse_macro(size_t name, wchar_t *arg, Env *env)
{
  if (name == intern(L".include")) {
    if (vflag >= 2)
      fprintf(stderr, "Including %ls\n", arg);
    return parse(read_file(arg, 1), env);
  }

  return env;
}

static Env *
parse(wchar_t *s, Env *env_begin)
{
  wchar_t *rest = s, *line, *exp, *tmp;
  size_t name;
  Env *env = env_begin, *cell;

  do {
    if (iswspace(*rest)) { /* allow whitespace before declarations */
      while (*rest && iswspace(*rest))
        rest++;
      continue;
    }
    if (*rest == L';') { /* allow very simple comments */
      while (*rest && *rest != L'\n')
        rest++;
      continue;
    }
    if (*rest == L'\n') {
      while (*rest == L'\n') rest++; /* skip empty lines */
      continue;
    }

    line = upto(rest, L'\n', &rest);
    if (wcslen(line) > 0) {
      name = intern(upto(line, L' ', &tmp));
      if ((cell = maybe_parse_macro(name, tmp, env)) != env) {
        env = cell;
      } else {
        upto(tmp, L' ', &tmp);
        exp = tmp;
        cell = eru_malloc(sizeof(Env));
        cell->key = name;
        cell->val = parse_exp(exp, env);
        cell->next = env;
        env = cell;
      }
    }
  } while (*rest);
  return env;
}


static FILE *
fwideopen(wchar_t *_filename, char *mode)
{
  char buf[PATH_MAX] = {0};
  mbstate_t bs = {0};
  FILE *f;
  const wchar_t *filename = wcsdup(_filename), *view = filename;

  wcsrtombs(buf, &view, wcslen(filename), &bs);

  f = fopen(buf, mode);

  free((void*)filename);
  return f;
}

static wchar_t *
read_file(void *filename, uint8_t wide_name_p)
{
  FILE *fp;
  size_t bump = 512, read = 0;
  wchar_t *buf = eru_malloc(bump * sizeof(wchar_t));
  if (wide_name_p)
    fp = fwideopen(filename, "rb+");
  else
    fp = fopen(filename, "rb+");
  while (!feof(fp)) {
    buf[read++] = fgetwc(fp);
    if (read % bump == 0)
      buf = realloc(buf, (read+bump)*sizeof(wchar_t));
  }
  buf[read-1] = 0;
  fclose(fp);
  return buf;
}

static __attribute__((unused)) void
print_env(Env *e)
{
  while (e) {
    fprintf(stderr, "[%ls: ", symbol_name(e->key));
    _print_exp(e->val, -1);
    fprintf(stderr, "]\n");
    e = e->next;
  }
}

static Exp *
get(Env *e, wchar_t *name_str)
{
  size_t name = intern(name_str);
  while (e) {
    if (name == e->key)
      return e->val;
    e = e->next;
  }

  return NULL;
}

static __attribute__((noreturn)) void
usage(void)
{
  fprintf(stderr, "Usage: %s [-hv] filename\n", argv0);
  exit(1);
}

int
main(int argc, char **argv)
{
  int opt;

  argv0 = *argv;
  setlocale(LC_ALL, "C.UTF-8"); /* wtf */

  while ((opt = getopt(argc, argv, "hv")) != -1) {
    switch (opt) {
    case 'h': usage(); /* noreturn */
    case 'v': vflag++; break;
    default:
      usage(); /* noreturn */
    }
  }

  if (optind >= argc)
    usage();

  wchar_t *buf = read_file(argv[optind], 0);
  Env *env = parse(buf, NULL);

  Exp *output = get(env, L"output");
  if (output) {
    print_exp(reduce(env, output));
  } else {
    fprintf(stderr, "No output declared in %s. Nothing to do.\n", argv[optind]);
  }

  return 0;
}
