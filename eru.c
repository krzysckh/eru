#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <err.h>
#include <wchar.h>

#include <locale.h>

static char *argv0 = NULL;

typedef enum {
  LAMBDA,
  APP,
  VAL
} exp_t;

typedef struct Exp {
  exp_t t;
  void *arg; /* Exp* | char* */
  struct Exp *body;
} Exp;

typedef struct Env {
  wchar_t *key;
  Exp  *val;
  struct Env *next;
} Env;

#if 0
static void*
eru_malloc(size_t n)
{
  static size_t all;
  all += n;
  void* p = malloc(n);
  warnx("malloc: %zu (now=%p)", all, p);
  return p;
}
#endif

#define eru_malloc malloc

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
parse_exp(wchar_t *s)
{
  Exp *e = eru_malloc(sizeof(Exp));
  wchar_t *rest = 0;

  switch (*s) {
  case L'λ':
    e->t = LAMBDA;
    e->arg = upto(s+1, L'.', &rest);
    e->body = parse_exp(rest);
    break;
  case L'(':
    e->t = APP;
    e->arg = parse_exp(upto(s+1, L' ', &rest));
    e->body = parse_exp(upto(rest, L')', &rest));
    break;
  default:
    e->t = VAL;
    e->arg = s;
    e->body = NULL;
    break;
  }

  return e;
}

static void print_exp(Exp*);

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
  /* printf("will dup: "); */
  /* print_exp(e); */
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
    printf("λ");
    printf("%ls", (wchar_t*)e->arg);
    printf(".");
    _print_exp(e->body, depth+1);
    break;
  case APP:
    printf("(");
    _print_exp(e->arg, depth+1);
    printf(" ");
    _print_exp(e->body, depth+1);
    printf(")");
    break;
  case VAL:
    printf("%ls", (wchar_t*)e->arg);
    break;
  }
}

static void
print_exp(Exp *e)
{
  _print_exp(e, 0);
  putchar('\n');
}

static Exp *
replace(Exp *e, wchar_t *arg, Exp *val)
{
  /* if (wcscmp(arg, L"g") == 0) */
  /*   print_exp(e); */
  switch (e->t) {
  case LAMBDA:
    if (wcscmp(e->arg, arg) == 0) { /* shadowing */
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
    if (wcscmp(e->arg, arg) == 0) {
      free_exp(e);
      memcpy(e, dupexp(val), sizeof(Exp));
      /* if (e->t != VAL) */
      /*   return replace(e, arg, val); */
      return e;
    }
    return e;
  default:
    errx(1, "wtf %d", e->t);
    return e;
  }
}

static Exp *
apply(Exp *e, Exp *val)
{
  assert(e->t == LAMBDA);
  /* printf("apply "); */
  /* print_exp(val); */
  /* printf(" to "); */
  /* print_exp(e); */
  Exp *e2 = replace(e->body, e->arg, val);
  /* printf(" returning "); */
  /* print_exp(e2); */
  /* puts(""); */
  return e2;
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

static Exp *apply_env(Env *, Exp *);

static Exp *
reduce(Env *env, Exp *e)
{
  int subs = 0;
  e = apply_env(env, e);
  /* print_exp(e); */
  int i = 0;
  do {
    /* warnx("REDUCE %d", i++); */
    subs = 0;
    walk_apply(e, &subs);
    /* TODO: verbose flag */
    /* print_exp(e); */
  } while (subs);

  return e;
}

static Exp *
apply_env(Env *env, Exp *e)
{
  while (env) {
    Exp *tmp = eru_malloc(sizeof(Exp)), *app = malloc(sizeof(Exp));
    tmp->t = LAMBDA;
    tmp->arg = env->key;
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
parse(wchar_t *s)
{
  wchar_t *rest = s, *line, *name, *exp, *tmp;
  Env *env = NULL, *cell;

  do {
    if (*rest == L';') { /* allow very simple comments */
      while (*rest && *rest++ != '\n')
        ;
      continue;
    }
    while (*rest == L'\n') rest++; /* skip empty lines */
    line = upto(rest, L'\n', &rest);
    if (wcslen(line) > 0) {
      name = upto(line, L' ', &tmp);
      upto(tmp, L' ', &tmp);
      exp = tmp;
      cell = eru_malloc(sizeof(Env));
      cell->key = name;
      /* cell->val = reduce(env, parse_exp(exp)); */
      cell->val =  parse_exp(exp);
      cell->next = env;
      env = cell;
    }
  } while (*rest);
  return env;
}


static wchar_t *
read_file(char *filename)
{
  FILE *fp = fopen(filename, "rb+");
  size_t bump = 512, read = 0;
  wchar_t *buf = eru_malloc(bump * sizeof(wchar_t));
  while (!feof(fp)) {
    buf[read++] = fgetwc(fp);
    if (read % bump == 0)
      buf = realloc(buf, (read+bump)*sizeof(wchar_t));
  }
  buf[read-1] = 0;
  return buf;
}

static void
print_env(Env *e)
{
  while (e) {
    printf("[%ls: ", e->key);
    _print_exp(e->val, -1);
    printf("]\n");
    e = e->next;
  }
}

static Exp *
get(Env *e, wchar_t *name)
{
  while (e) {
    if (wcscmp(name, e->key) == 0)
      return e->val;
    e = e->next;
  }

  return NULL;
}

static void
usage(void)
{
  fprintf(stderr, "Usage: %s filename\n", argv0);
  exit(1);
}

int
main(int argc, char **argv)
{
  argv0 = *argv;
  setlocale(LC_ALL, "C.UTF-8"); /* wtf */

  if (argc != 2)
    usage();
  // char *exp = "\\x.\\y.(x y)";
  // char *exp = "((\\x.x \\x.x) (\\x.x \\x.x))";
  // char *exp = "(\\x.x \\x.\\y.((\\x.x x) y))";
  // Exp *e = parse_exp(exp);
  // print_exp(e);
  // puts("");
  // e = reduce(e);
  /* e = apply(e, ); */
  /* puts(""); */
  // print_exp(e);

  wchar_t *buf = read_file(argv[1]);
  Env *env = parse(buf);

  /* print_env(env); */

  Exp *output = get(env, L"output");
  print_exp(reduce(env, output));

  return 0;
}
