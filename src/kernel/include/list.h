#ifndef __LIST_H__
#define __LIST_H__

/* Most of the system will use lists to hold things. Well, let's provide a
 * general implementation of a list. It's probably not the most efficient
 * implementation of a linked list but let optimizations for later.
 * It's is important to remeber this list implementation doesn't release
 * any value pointer when an item is removed. It's up to the user to do it. */

typedef struct __list_node {
  struct __list_node *next;
  void *val;
} list_node_t;

/* This is a generic function type used to compare two arbitrary items. When
 * using list_find you'll need to provide it. The first argument will always
 * be an item in the list, the second will be the third parameter passed to
 * list_find. Exempli gratia:
 *  int foo_cmp(void *item, void *str) {
 *    return !strcmp((foo_item *)item->name, (char *)str);
 *  }
 * We expect 0 to mean the item is NOT the one looked for.
 */
typedef int (*list_cmp_t) (void *, void *);

typedef struct __list {
  list_node_t *head;
  int count;
} list_t;

void list_init(list_t *);
int list_add(list_t *, void *);
void * list_get(list_t *, int);
void * list_del(list_t *, int);
void * list_find(list_t *, list_cmp_t, void *);
void * list_find_del(list_t *, list_cmp_t, void *);
int list_find_pos(list_t *, list_cmp_t, void *);

#endif
