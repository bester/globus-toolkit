
/********************************************************************
 *
 * This file implements the list_t type
 *
 ********************************************************************/

#include "globus_common.h"

#include <assert.h>

#define GLOBUS_L_LIST_INIT_MEM_COUNT       512

#define _MEMORY_USE_INTERNAL_MEM 1

#ifdef  _MEMORY_USE_INTERNAL_MEM
    static globus_memory_t                globus_l_memory_list_info;

#   define MALLOC_LIST_T()                                            \
        ((globus_list_t *)                                            \
             globus_memory_pop_node(&globus_l_memory_list_info))
#   define FREE_LIST_T(ptr)                                           \
        (globus_memory_push_node(                                     \
                                  &globus_l_memory_list_info,         \
                                  (globus_byte_t *)ptr))
#else
#   define MALLOC_LIST_T()                                            \
        ((globus_list_t *)                                            \
            globus_malloc(sizeof(globus_list_t)))
#   define FREE_LIST_T(ptr)                                           \
        (globus_free(ptr))
#endif

static globus_bool_t                     globus_l_list_active = GLOBUS_FALSE;
/******************************************************************************
                          Function Definitions
******************************************************************************/
/*
 * needs to be called by thread
 */
int
globus_i_list_pre_activate(void)
{
    if(!globus_l_list_active)
    {
        globus_l_list_active = GLOBUS_TRUE;
#       if defined(_MEMORY_USE_INTERNAL_MEM)
        {
            globus_memory_init(
                &globus_l_memory_list_info,
                sizeof(globus_list_t),
                GLOBUS_L_LIST_INIT_MEM_COUNT);
        }
#       endif
    }
    return GLOBUS_SUCCESS;
}

int
globus_list_int_less (void * low_datum, void * high_datum,
		      void *ignored)
{
  return ((unsigned long) low_datum) < ((unsigned long) high_datum);
}

void *
globus_list_first (globus_list_t * head)
{
  assert (head != GLOBUS_NULL);
  return (void *) head->datum;
}

globus_list_t *
globus_list_rest (globus_list_t * head)
{
  assert (head != GLOBUS_NULL);
  return (globus_list_t *) head->next;
}

globus_list_t **
globus_list_rest_ref (globus_list_t * head)
{
  assert (head != GLOBUS_NULL);
  return (globus_list_t **) &(head->next);
}

int 
globus_list_empty (globus_list_t * head)
{
  return head == GLOBUS_NULL;
}

int 
globus_list_size (globus_list_t *head)
{
  if (globus_list_empty (head)) {
    return 0;
  }
  else {
    return 1 + globus_list_size (globus_list_rest (head));
  }
}

/* return the old datum value */
void *
globus_list_replace_first (globus_list_t * head, void *datum)
{
  void *old_datum;
  assert (head != GLOBUS_NULL);
  old_datum = head->datum;
  head->datum = datum;
  return old_datum;
}

globus_list_t *
globus_list_search (globus_list_t *head, void *datum)
{
  if (globus_list_empty (head)) {
    /* end of list chain */
    return GLOBUS_NULL;
  }
  else if (globus_list_first (head) == datum) {
    /* found list binding */
    return head;
  }
  else {
    /* check rest of chain */
    return globus_list_search (globus_list_rest (head), datum);
  }
}

globus_list_t *
globus_list_search_pred (globus_list_t *head, 
			 globus_list_pred_t predicate,
			 void *pred_args)
{
  if (globus_list_empty (head)) {
    /* end of list chain */
    return GLOBUS_NULL;
  }
  else if ((*predicate) (globus_list_first (head), pred_args)) {
    /* found list binding */
    return head;
  }
  else {
    /* check rest of chain */
    return globus_list_search_pred (globus_list_rest (head), predicate, pred_args);
  }
}

static globus_list_t *
s_globus_list_min_with_register (globus_list_t *current_min,
				 globus_list_t *rest_head,
				 globus_list_relation_t relation,
				 void *relation_args)
{
  if (globus_list_empty (rest_head)) {
    return current_min;
  }
  else if ( (*relation) (globus_list_first(current_min), 
			 globus_list_first(rest_head),
			 relation_args) ) {
    return s_globus_list_min_with_register (current_min,
				     globus_list_rest (rest_head),
				     relation,
				     relation_args);
  }
  else {
    return s_globus_list_min_with_register (rest_head,
				     globus_list_rest (rest_head),
				     relation,
				     relation_args);
  }
}

globus_list_t *
globus_list_min (globus_list_t *head,
		 globus_list_relation_t relation,
		 void *relation_args)
{
  if (globus_list_empty (head)) {
    return GLOBUS_NULL;
  }
  else {
    return s_globus_list_min_with_register (head,
				     globus_list_rest(head),
				     relation,
				     relation_args);
  }
}

void
globus_list_halves_destructive (globus_list_t  * head,
				globus_list_t * volatile * leftp,
				globus_list_t * volatile * rightp)
{
  int len;
  int i;

  assert (leftp!=GLOBUS_NULL);
  assert (rightp!=GLOBUS_NULL);
  
  len = globus_list_size (head);
  
  *leftp = head;

  for (i=0; i<(len/2 - 1); i++) {
    head = globus_list_rest (head);
  }

  *rightp = globus_list_rest (head);

  *(globus_list_rest_ref (head)) = GLOBUS_NULL;
}

globus_list_t *
globus_list_sort_merge_destructive (globus_list_t * left,
				    globus_list_t * right,
				    globus_list_relation_t relation,
				    void *relation_args)
{
  globus_list_t  * result = GLOBUS_NULL;
  globus_list_t ** result_tail = GLOBUS_NULL;

  while ( (! globus_list_empty (left))
	  && (! globus_list_empty (right)) ) {
    if ( relation (globus_list_first (left),
		   globus_list_first (right),
		   relation_args) ) {
      if ( result_tail ) *result_tail = left;
      else result = left;
      result_tail = globus_list_rest_ref (left);
      left = globus_list_rest (left);
      *result_tail = GLOBUS_NULL;
    }
    else {
      if ( result_tail ) *result_tail = right;
      else result = right;
      result_tail = globus_list_rest_ref (right);
      right = globus_list_rest (right);
      *result_tail = GLOBUS_NULL;
    }
  }

  if ( globus_list_empty (left) ) {
    if ( result_tail ) *result_tail = right;
    else result = right;
  }
  else {
    assert ( globus_list_empty (right) );
    if ( result_tail ) *result_tail = left;
    else result = left;
  }

  return result;
}

globus_list_t *
globus_list_sort_destructive (globus_list_t *head,
			      globus_list_relation_t relation,
			      void *relation_args)
{
  globus_list_t * left;
  globus_list_t * right;

  if ( globus_list_empty (head) 
       || globus_list_empty (globus_list_rest (head)) ) return head;

  globus_list_halves_destructive (head, &left, &right);

  return globus_list_sort_merge_destructive (
			     globus_list_sort_destructive (left,
							   relation,
							   relation_args),
			     globus_list_sort_destructive (right,
							   relation,
							   relation_args),
			     relation,
			     relation_args);
}

globus_list_t *
globus_list_sort (globus_list_t *head,
		  globus_list_relation_t relation,
		  void *relation_args)
{
  return globus_list_sort_destructive (globus_list_copy (head),
				       relation,
				       relation_args);
}

int 
globus_list_insert (
    globus_list_t * volatile *              headp, 
    void *                                  datum)
{
    globus_bool_t                        mal;
    globus_list_t *                      entry;

    mal = !globus_l_list_active;

    if(mal)
    {
        entry = globus_malloc(sizeof(globus_list_t));
        entry->malloced = mal;
    }
    else
    {
        entry = MALLOC_LIST_T();
        entry->malloced = mal;
    }
    entry->datum = datum;
    entry->next = *headp;

    *headp = entry;

    return 0;
}

globus_list_t *
globus_list_cons (void * datum, globus_list_t * list)
{
  int err;

  err = globus_list_insert (&list, datum);
  if (err) return NULL;

  return list;
}

globus_list_t *
globus_list_copy (globus_list_t *head)
{
    globus_list_t *                     entry;
    globus_bool_t                       mal;

    mal = !globus_l_list_active;
    if (head!=GLOBUS_NULL) 
    {
        if(mal)
        {
            entry = globus_malloc(sizeof(globus_list_t));
            entry->malloced = mal;
        }
        else
        {
            entry = MALLOC_LIST_T();
            entry->malloced = mal;
        }
        entry->datum = head->datum;
        entry->next = globus_list_copy (head->next);

        return entry;
    }
    else 
    {
        return GLOBUS_NULL;
    }
}

void *
globus_list_remove (globus_list_t * volatile *headp, globus_list_t *entry)
{
    assert (headp);
    assert (entry);

    if (globus_list_empty (*headp)) 
    {
        /* binding not found */
        return GLOBUS_NULL;
    }
    else if ((*headp) == entry) 
    {
        /* found list binding */
        void *datum;

        *headp = globus_list_rest (*headp);
        datum = globus_list_first (entry);

        if(entry->malloced)
        {
            globus_free(entry);
        }
        else
        {
            FREE_LIST_T(entry);
        }
        return datum;
    }
    else 
    {
        /* check rest of chain */
        return globus_list_remove (((globus_list_t **)
				&((*headp)->next)), entry);
    }
}

void
globus_list_free (globus_list_t *head)
{
  while (! globus_list_empty (head)) {
    globus_list_remove (&head, head);
  }
}

