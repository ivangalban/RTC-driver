#ifndef __LIST_H__
#define __LIST_H__

/* Most of the system will use lists to hold things. Well, let's provide a
 * general implementation of a list. It's probably not the most efficient
 * implementation of a linked list but let optimizations for later.
 * It's is important to remeber this list implementation doesn't release
 * any value pointer when an item is removed. It's up to the user to do it. */

typedef int list_key_t;

typedef struct __list_node {
  list_key_t key;
  struct __list_node *next;
  void *val;
} list_node_t;

typedef list_key_t (*list_key_func_t)(void *);

typedef struct __list {
  list_node_t *head;
  int count;
} list_t;

void list_init(list_t *);
int list_add(list_t *, void *, list_key_t);
void * list_get(list_t *, int);
int list_del(list_t *, int);
void * list_find(list_t *, list_key_t);
int list_find_pos(list_t *, list_key_t);

#endif
