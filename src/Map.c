#include "Cello/Map.h"

#include "Cello/List.h"
#include "Cello/Bool.h"
#include "Cello/None.h"
#include "Cello/Exception.h"

#include <string.h>

#if defined(__APPLE__)
#include <sys/rbtree.h>
#else
#define AVL_COUNT 
#include <avl.h>
#endif

#include <assert.h>


data {
  var type;
#if defined(__APPLE__)
  rb_tree_t tree;
  rb_tree_ops_t ops;
#else 
  avl_tree_t tree;
#endif
} MapData;

var Map = type_data {
  type_begin(Map),
  type_entry(Map, New),
  type_entry(Map, Assign),
  type_entry(Map, Copy),
  type_entry(Map, Eq),
  type_entry(Map, Collection),
  type_entry(Map, Dict),
  type_entry(Map, Iter),
  type_entry(Map, Show),
  type_end(Map),
};

#if defined(__APPLE__)
typedef struct {
    rb_node_t node;
    var key;
    var value;
} tree_node;
int compare_nodes(void *context, const void *node1, const void *node2)
{
  if_eq( ((tree_node *)node1)->key, ((tree_node *)node2)->key )
    return 0;
  if_lt( ((tree_node *)node1)->key, ((tree_node *)node2)->key )
    return -1;
  else 
    return 1;
}
int compare_key(void *context, const void *node, const void *key)
{
  if_eq( ((tree_node *)node)->key, (var)key )
    return 0;
  if_lt( ((tree_node *)node)->key, (var)key )
    return -1;
  else
    return 1;
}
#else 
typedef struct {
    var key;
    var value;
} avl_payload;
int compare_avl_nodes(const void *p1, const void *p2)
{
    if_eq(((avl_payload *)p1)->key, ((avl_payload *)p2)->key)
        return 0;
    if_lt(((avl_payload *)p1)->key, ((avl_payload *)p2)->key)
        return -1;
    else 
        return 1;
}
void free_avl_nodeitem(void *p)
{
    free(p);
}
#endif
var Map_New(var self, var_list vl) {
  MapData* md = cast(self, Map);
#if defined(__APPLE__)
  md->ops.rbto_compare_nodes = compare_nodes;
  md->ops.rbto_compare_key = compare_key;
  md->ops.rbto_node_offset = 0;
  rb_tree_init(&md->tree, &md->ops);
#else
  avl_init_tree(&md->tree, compare_avl_nodes, free_avl_nodeitem);
#endif
  return self;
}

var Map_Delete(var self) {
  clear(self);
  return self;
}

size_t Map_Size(void) {
  return sizeof(MapData);
}

void Map_Assign(var self, var obj) {
  MapData* other = cast(obj, Map);
  clear(self);
  
  foreach(key in other) {
    var val = get(other, key);
    put(self, key, val);
  }
}

var Map_Copy(var self) {
  var newmap = new(Map);
  foreach(key in self) {
    var val = get(self, key);
    put(newmap, key, val);
  }
  return newmap;
}

var Map_Eq(var self, var obj) {
  if_neq(type_of(obj), Map) { return False; }
  
  foreach(key in obj) {
    if (not contains(self, key)) { return False; }
    if_neq(get(obj, key), get(self, key)) { return False; }
  }
  
  foreach(key in self) {
    if (not contains(obj, key)) { return False; }
    if_neq(get(self, key), get(obj, key)) { return False; }
  }  
  return True;
}

int Map_Len(var self) {
  MapData* md = cast(self, Map);
#if defined(__APPLE__)
  return (int)rb_tree_count(&md->tree);
#else
  return avl_count(&md->tree);
#endif
}

void Map_Clear(var self) {
  MapData* md = cast(self, Map);
  
#if defined(__APPLE__)
  while(not is_empty(self)) {
    discard(self, ((tree_node *)RB_TREE_MIN(&md->tree))->key); 
  }
#else 
  avl_clear_tree(&md->tree);
#endif
}

var Map_Contains(var self, var key) {
  MapData* md = cast(self, Map);
#if defined(__APPLE__)
  return rb_tree_find_node(&md->tree, key) != NULL ? True : False;
#else 
  avl_payload p = {key, NULL};
  return avl_search(&md->tree, &p);
#endif 
}



void Map_Discard(var self, var key) {
  MapData* md = cast(self, Map);
#if defined(__APPLE__)
  tree_node *n = rb_tree_find_node(&md->tree, key);
  if (n != NULL)
  {
    rb_tree_remove_node(&md->tree, n);
    free(n);
  }
#else
  avl_payload p = {key, NULL};
  avl_delete(&md->tree, &p);
#endif
}

var Map_Get(var self, var key) {
  MapData* md = cast(self, Map);
  
#if defined(__APPLE__)
  tree_node *n = rb_tree_find_node(&md->tree, key);
  
  if (!n) {
    return throw(KeyError, "Key '%$' not in Map!", key);
  }
  return n->value;
#else 
  avl_payload target = {key, NULL};
  avl_node_t * n = avl_search(&md->tree, &target);

  if (!n) {
    return throw(KeyError, "Key '%$' not in Map!", key);
  }
  return ((avl_payload *)n->item)->value;
#endif
}

void Map_Put(var self, var key, var val) {
  MapData* md = cast(self, Map);
  
#if defined(__APPLE__)
  tree_node *n = malloc(sizeof(tree_node));
  n->key = key;
  n->value = val;
  tree_node *m = rb_tree_insert_node(&md->tree, n);
  if (m != n) {
    free(n);
    m->value = val;
    m->key = key;
  }
  assert(rb_tree_find_node(&md->tree, key));
#else
  avl_payload *p = malloc(sizeof(avl_payload));
  p->key = key;
  p->value = val;
  
  avl_node_t *e = avl_search(&md->tree, p);
  if (e) {
    ((avl_payload *)e->item)->value = val;
    free(p);
  } else {
    avl_insert(&md->tree, p);
  }
#endif
}

var Map_Iter_Start(var self) {
  MapData* md = cast(self, Map);

#if defined(__APPLE__)
  tree_node *n = rb_tree_iterate(&md->tree, NULL, RB_DIR_RIGHT);
  if (!n) {
    return Iter_End;
  }
  else {
    return n->key;
  }    
#else 
  avl_node_t *n = avl_at(&md->tree, 0);
  if (!n) {
    return Iter_End;
  }
  else {
    return ((avl_payload *)n->item)->key;
  }   
#endif
}

var Map_Iter_End(var self) {
  return Iter_End;
}

var Map_Iter_Next(var self, var curr) {
  MapData* md = cast(self, Map);

#if defined(__APPLE__)
  tree_node *n = rb_tree_find_node(&md->tree, curr);
  if (!n) {
    return Iter_End;
  } else {
    n = rb_tree_iterate(&md->tree, n, RB_DIR_RIGHT);
    if (!n)
      return Iter_End;
    return n->key;
  } 
#else 
  avl_payload p = {curr, NULL};
  avl_node_t *n = avl_search(&md->tree, &p);
  if (!n) {
    return Iter_End;
  } else {
    n = n->next;
    if (!n)
      return Iter_End;
    return ((avl_payload *)n->item)->key;
  }
#endif
}

int Map_Show(var self, var output, int pos) {
  pos = print_to(output, pos, "<'Map' At 0x%p {", self);
  int i = 0;
  int last_i = len(self)-1;
  foreach(key in self) {
    pos = print_to(output, pos, "%$:%$", key, get(self, key));
    if (i < last_i) { pos = print_to(output, pos, ", "); }
    i++;
  }
  
  pos = print_to(output, pos, "}>");
  
  return pos;
}

