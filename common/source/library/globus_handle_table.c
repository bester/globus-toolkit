/******************************************************************************
globus_handle_table.c

Description:
    This module implements a reference-counting handle table structure.

CVS Information:

    $Source$
    $Date$
    $Revision$
    $Author$
******************************************************************************/
#include "config.h"
#include "globus_handle_table.h"

#define GLOBUS_L_HANDLE_TABLE_BLOCK_SIZE 100

/******************************************************************************
                           local data structures
******************************************************************************/
typedef struct globus_l_handle_entry_s
{
    int                                 index;
    int                                 ref;
    void *                              value;
    struct globus_l_handle_entry_s *    pnext;
} globus_l_handle_entry_t;

/*
 * Function: globus_handle_table_init()
 *
 * Description: Initialize a handle table
 *
 * Parameters:
 *
 * Returns:
 */
int
globus_handle_table_init(
    globus_handle_table_t *             handle_table,
    globus_handle_destructor_t          destructor)
{
    if(!handle_table)
    {
        return GLOBUS_FAILURE;
    }

    handle_table->table = (globus_l_handle_entry_t **)
        globus_libc_malloc(GLOBUS_L_HANDLE_TABLE_BLOCK_SIZE * 
            sizeof(globus_l_handle_entry_t *));
    if(!handle_table->table)
    {
        return GLOBUS_FAILURE;
    }

    handle_table->next_slot = GLOBUS_NULL_HANDLE + 1;
    handle_table->table_size = GLOBUS_L_HANDLE_TABLE_BLOCK_SIZE;
    handle_table->inactive = GLOBUS_NULL;
    handle_table->destructor = destructor;
    
    return GLOBUS_SUCCESS;
}
/* globus_handle_table_init() */

/*
 * Function: globus_handle_table_destroy()
 *
 * Description: Destroy a handle table
 *
 * Parameters:
 *
 * Returns:
 */
int
globus_handle_table_destroy(
    globus_handle_table_t *             handle_table)
{
    int                                 i;
    globus_l_handle_entry_t **          table;
    globus_l_handle_entry_t *           inactive;
    globus_l_handle_entry_t *           save;
    globus_handle_destructor_t          destructor;

    if(!handle_table)
    {
        return GLOBUS_FAILURE;
    }

    /* first free all active handles */
    table = handle_table->table;
    destructor = handle_table->destructor;
    i = handle_table->next_slot;
    while(--i > GLOBUS_NULL_HANDLE)
    {
        if(table[i])
        {
            if(destructor)
            {
                destructor(table[i]->value);
            }
            
            globus_libc_free(table[i]);
        }
    }

    /* then free inactive handles */
    inactive = handle_table->inactive;
    while(inactive)
    {
        save = inactive->pnext;
        globus_libc_free(inactive);
        inactive = save;
    }

    /* finally, free table */
    globus_libc_free(table);

    return GLOBUS_SUCCESS;
}
/* globus_l_callback_handle_destroy() */

/*
 * Function: globus_handle_table_insert()
 *
 * Description: Insert a value into the handle table, and
 *              return a unique handle.
 *
 * Parameters:  handle_table - the table of unique handles
 *                  we want to use
 *              value - the value to insert into the table
 *              initial_refs - the initial reference count
 *                  of this value in the table.
 *
 * Returns:  A unique handle.
 */
globus_handle_t
globus_handle_table_insert(
    globus_handle_table_t *             handle_table,
    void *                              value,
    int                                 initial_refs)
{
    globus_l_handle_entry_t *           entry;

    if(!handle_table)
    {
        return GLOBUS_NULL_HANDLE;
    }

    /* see if we have an inactive handle, if so, take it */
    if(handle_table->inactive)
    {
        entry = handle_table->inactive;
        handle_table->inactive = entry->pnext;
    }
    /* otherwise allocate a new entry */
    else
    {
        /* if table is full, make bigger */
        if(handle_table->next_slot == handle_table->table_size)
        {
            globus_l_handle_entry_t ** new_table;

            new_table = (globus_l_handle_entry_t **)
                globus_libc_realloc(
                    handle_table->table,
                    (handle_table->table_size + 
                        GLOBUS_L_HANDLE_TABLE_BLOCK_SIZE) *
                        sizeof(globus_l_handle_entry_t *));

            if(!new_table)
            {
                return GLOBUS_NULL_HANDLE;
            }

            handle_table->table = new_table;
            handle_table->table_size += GLOBUS_L_HANDLE_TABLE_BLOCK_SIZE;
        }

        entry = (globus_l_handle_entry_t *)
            globus_libc_malloc(sizeof(globus_l_handle_entry_t));
        if(!entry)
        {
            return GLOBUS_NULL_HANDLE;
        }

        entry->index = handle_table->next_slot++;
    }

    /* now bind this entry to table */
    handle_table->table[entry->index] = entry;

    entry->value = value;
    entry->ref = initial_refs;

    return entry->index;
}
/* globus_handle_table_insert() */

globus_bool_t
globus_handle_table_increment_reference_by(
    globus_handle_table_t *             handle_table,
    globus_handle_t                     handle,
    unsigned int                        inc)
{
    globus_l_handle_entry_t *           entry;

    if(!handle_table)
    {
        return GLOBUS_FALSE;
    }

    if(handle > GLOBUS_NULL_HANDLE && handle < handle_table->next_slot)
    {
        entry = handle_table->table[handle];
    }
    else
    {
        entry = GLOBUS_NULL;
    }

    if(entry)
    {
        entry->ref += inc;
        return GLOBUS_TRUE;
    }
    else
    {
        return GLOBUS_FALSE;
    }
}

/*
 * Function: globus_handle_table_decrement_reference()
 *
 * Description: Remove a reference to a handle table entry,
 *              deleting the entry if no more references
 *              exist.
 *
 * Parameters:  handle_table - the table of unique handles
 *                  we want to use
 *              handle - the handle that we want to remove
 *
 * Returns:  GLOBUS_TRUE if the handle is still referenced.
 *
 */
globus_bool_t
globus_handle_table_decrement_reference(
    globus_handle_table_t *             handle_table,
    globus_handle_t                     handle)
{
    globus_l_handle_entry_t *           entry;

    if(!handle_table)
    {
        return GLOBUS_FALSE;
    }

    if(handle > GLOBUS_NULL_HANDLE && handle < handle_table->next_slot)
    {
        entry = handle_table->table[handle];
    }
    else
    {
        entry = GLOBUS_NULL;
    }

    if(entry)
    {
        entry->ref--;
        if(entry->ref == 0)
        {
            if(handle_table->destructor)
            {
                handle_table->destructor(entry->value);
            }
            
            /* NULL out slot and push this on the inactive list */
            handle_table->table[handle] = GLOBUS_NULL;
            entry->pnext = handle_table->inactive;
            handle_table->inactive = entry;
        }
        else
        {
            return GLOBUS_TRUE;
        }
    }

    return GLOBUS_FALSE;
}
/* globus_handle_table_decrement_reference() */

/*
 * Function: globus_handle_table_increment_reference()
 *
 * Description: Add a reference to a handle table entry.
 *
 * Parameters:  handle_table - the table of unique handles
 *                  we want to use
 *              handle - the handle that we want to remove
 *
 * Returns:  GLOBUS_TRUE if the handle is still referenced.
 *
 */
globus_bool_t
globus_handle_table_increment_reference(
    globus_handle_table_t *             handle_table,
    globus_handle_t                     handle)
{
    globus_l_handle_entry_t *           entry;

    if(!handle_table)
    {
        return GLOBUS_FALSE;
    }

    if(handle > GLOBUS_NULL_HANDLE && handle < handle_table->next_slot)
    {
        entry = handle_table->table[handle];
    }
    else
    {
        entry = GLOBUS_NULL;
    }

    if(entry)
    {
        entry->ref++;
        return GLOBUS_TRUE;
    }
    else
    {
        return GLOBUS_FALSE;
    }
}
/* globus_handle_table_increment_reference() */

/*
 * Function: globus_handle_table_lookup()
 *
 * Description: Find the void * corresponding to a unique
 *              handle. Does not update the reference count.
 *
 * Parameters:  handle_table - the table of unique handles
 *                  we want to use
 *              handle - the handle that we want to look up
 *
 * Returns:  the data value associated with the handle
 *
 */
void *
globus_handle_table_lookup(
    globus_handle_table_t *             handle_table,
    globus_handle_t                     handle)
{
    globus_l_handle_entry_t *           entry;

    if(!handle_table)
    {
        return GLOBUS_NULL;
    }

    if(handle > GLOBUS_NULL_HANDLE && handle < handle_table->next_slot)
    {
        entry = handle_table->table[handle];
    }
    else
    {
        entry = GLOBUS_NULL;
    }

    if(entry)
    {
        return entry->value;
    }
    else
    {
        return GLOBUS_NULL;
    }
}
/* globus_handle_table_lookup() */

