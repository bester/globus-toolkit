/********************************************************************
 *
 ********************************************************************/
#include "globus_common_include.h"
#include "globus_range_list.h"
#include "globus_libc.h"

typedef struct globus_l_range_ent_s
{
    globus_off_t                        offset;
    globus_off_t                        length;
    struct globus_l_range_ent_s *       next;
} globus_l_range_ent_t;

typedef struct globus_l_range_list_s
{
    int                                 size;
    globus_l_range_ent_t *              head;
} globus_l_range_list_t;

int
globus_range_list_init(
    globus_range_list_t *               range_list)
{
    globus_l_range_list_t *             rl;

    rl = (globus_l_range_list_t *) globus_calloc(
        sizeof(globus_l_range_list_t), 1);
    if(rl == NULL)
    {
        return GLOBUS_RANGE_LIST_ERROR_MEMORY;
    }

    *range_list = rl;

    return GLOBUS_SUCCESS;
}

void
globus_range_list_destroy(
    globus_range_list_t                 range_list)
{
    globus_l_range_ent_t *              i;
    globus_l_range_ent_t *              j;

    if(range_list == NULL)
    {
        return;
    }

    i = range_list->head;
    while(i != NULL)
    {
        j = i;
        i = i->next;
        globus_free(j);
    }
    globus_free(range_list);
}

int
globus_range_list_insert(
    globus_range_list_t                 range_list,
    globus_off_t                        offset,
    globus_off_t                        length)
{
    globus_l_range_ent_t *              prev;
    globus_l_range_ent_t *              ent;
    globus_l_range_ent_t *              next;
    globus_l_range_ent_t *              new_ent;
    globus_size_t                       end_offset;
    globus_size_t                       ent_end;
    globus_bool_t                       done = GLOBUS_FALSE;

    if(range_list->head == NULL)
    {
        new_ent = (globus_l_range_ent_t *) globus_malloc(
            sizeof(globus_l_range_ent_t));
        if(new_ent == NULL)
        {
            globus_assert(0);
        }
        new_ent->offset = offset;
        new_ent->length = length;
        new_ent->next = NULL;
        range_list->head = new_ent;
        range_list->size = 1;
        
        return GLOBUS_SUCCESS;
    }

    end_offset = offset + length;

    prev = NULL;
    ent = range_list->head;
    while(ent != NULL && !done)
    {
        ent_end = ent->offset + ent->length;
        next = ent->next;
        /* if it is discontigous and in front of this one */
        if(end_offset < ent->offset)
        {
            new_ent = (globus_l_range_ent_t *) globus_malloc(
                sizeof(globus_l_range_ent_t));
            if(new_ent == NULL)
            {
                globus_assert(0);
            }
            new_ent->offset = offset;
            new_ent->length = length;
            new_ent->next = ent;
            if(prev == NULL)
            {
                range_list->head = new_ent;
            }
            else
            {
                prev->next = new_ent;
            }
            range_list->size++;
            done = GLOBUS_TRUE;
        }
        /* if it is merging */
        else if(end_offset >= ent->offset && offset <= ent_end)
        {
            if(offset < ent->offset)
            {
                ent->offset = offset;
            }
            if(end_offset > ent_end)
            {
                ent->length = end_offset - ent->offset;
            }
            if(next != NULL && end_offset >= next->offset)
            {
                ent->length = next->offset + next->length - ent->offset;
                range_list->size--;
                ent->next = next->next;
                globus_free(next);
            }
            done = GLOBUS_TRUE;
        }
        else
        {
            prev = ent;
            ent = ent->next;
        }
    }
    /* must be last entry */
    if(!done)
    {
        new_ent = (globus_l_range_ent_t *) globus_malloc(
            sizeof(globus_l_range_ent_t));
        if(new_ent == NULL)
        {
            globus_assert(0);
        }
        new_ent->offset = offset;
        new_ent->length = length;
        new_ent->next = ent;

        globus_assert(prev != NULL);
        prev->next = new_ent;
        range_list->size++;
    }

    return GLOBUS_SUCCESS;
}

int
globus_range_list_size(
    globus_range_list_t                 range_list)
{
    if(range_list == NULL)
    {
        return 0;
    }

    return range_list->size;
}

int
globus_range_list_at(
    globus_range_list_t                 range_list,
    int                                 ndx,
    globus_off_t *                      offset,
    globus_off_t *                      length)
{
    int                                 ctr;
    globus_l_range_ent_t *              i;

    if(range_list == NULL)
    {
        return GLOBUS_RANGE_LIST_ERROR_PARAMETER;
    }
    if(offset == NULL)
    {
        return GLOBUS_RANGE_LIST_ERROR_PARAMETER;
    }
    if(length == NULL)
    {
        return GLOBUS_RANGE_LIST_ERROR_PARAMETER;
    }

    i = range_list->head;
    for(ctr = 0; ctr < ndx; ctr++)
    {
        if(i == NULL)
        {
            return GLOBUS_RANGE_LIST_ERROR_PARAMETER;
        }
        i = i->next;
    }

    *offset = i->offset;
    *length = i->length;

    return GLOBUS_SUCCESS;
}

int
globus_range_list_remove_at(
    globus_range_list_t                 range_list,
    int                                 ndx,
    globus_off_t *                      offset,
    globus_off_t *                      length)
{
    int                                 ctr;
    globus_l_range_ent_t *              i;
    globus_l_range_ent_t *              prev;

    if(range_list == NULL)
    {
        return GLOBUS_RANGE_LIST_ERROR_PARAMETER;
    }
    if(offset == NULL)
    {
        return GLOBUS_RANGE_LIST_ERROR_PARAMETER;
    }
    if(length == NULL)
    {
        return GLOBUS_RANGE_LIST_ERROR_PARAMETER;
    }

    prev = NULL;
    i = range_list->head;
    for(ctr = 0; ctr < ndx; ctr++)
    {
        if(i == NULL)
        {
            return GLOBUS_RANGE_LIST_ERROR_PARAMETER;
        }
        prev = i;
        i = i->next;
    }

    if(i == NULL)
    {
        return GLOBUS_RANGE_LIST_ERROR_PARAMETER;
    }

    if(prev == NULL)
    {
        range_list->head = i->next;
    }
    else
    {
        prev->next = i->next;
    }

    range_list->size--;
    
    *offset = i->offset;
    *length = i->length;
    globus_free(i);

    return GLOBUS_SUCCESS;
}
