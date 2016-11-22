#include <list.h>
#include <mem.h>
#include <typedef.h>
#include <errors.h>

void list_init(list_t * l) {
  l->head = NULL;
  l->count = 0;
}

int list_add(list_t *l, void *val) {
  list_node_t *n, *p;

  n = (list_node_t*)kalloc(sizeof(list_node_t));
  if (n == NULL) {
    set_errno(E_NOMEM);
    return -1;
  }

  n->val = val;
  n->next = NULL;

  if (l->head == NULL) {
    l->head = n;
    l->count = 1;
    return 0;
  }

  for (p = l->head; p->next != NULL; p = p->next);
  p->next = n;
  l->count += 1;
  return 0;
}

void * list_get(list_t *l, int pos) {
  int i;
  list_node_t *p;

  if (pos >= l->count) {
    return NULL;
  }

  for (i = 0, p = l->head; i < pos; i ++, p = p->next);

  return p->val;
}

void * list_find(list_t *l, list_cmp_t list_cmp, void *search) {
  list_node_t *p;
  for (p = l->head;
       p != NULL && !list_cmp(p, search);
       p = p->next);
  if (p == NULL)
    return NULL;
  return p->val;
}

int list_find_pos(list_t *l, list_cmp_t list_cmp, void *search) {
  list_node_t *p;
  int i;

  for (p = l->head, i = 0;
       p != NULL && !list_cmp(p, search);
       p = p->next, i ++);
  if (p == NULL) {
    return -1;
  }
  return i;
}

int list_del(list_t *l, int pos) {
  list_node_t *p, *prev;
  int i;

  if (pos >= l->count) {
    errno = E_NOKOBJ;
    return -1;
  }

  if (pos == 0) {
    p = l->head;
    l->head = p->next;
    l->count --;
    kfree(p);
    return 0;
  }

  for (i = 1, prev = l->head, p = prev->next;
       i < pos;
       i ++, prev = p, p = p->next);

  prev->next = p->next;
  l->count --;
  kfree(p);
  return 0;
}
