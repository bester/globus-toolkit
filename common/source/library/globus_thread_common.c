/******************************************************************************
globus_common.c

Description:

  Routines common to all of Globus

CVS Information:

******************************************************************************/

/******************************************************************************
			     Include header files
******************************************************************************/
#include "config.h"
#include "globus_common.h"
#include "globus_thread_common.h"
#include "globus_i_thread.h"

#if HAVE_STRING_H
#include <string.h>
#endif

#if HAVE_SIGNAL_H
#include <signal.h>
#endif

#define THREAD_STACK_INIT_SIZE 32


typedef struct globus_l_thread_stack_node_s
{
    globus_thread_blocking_func_t  func;
    void *                         user_args;
    globus_bool_t                  enabled;

} globus_l_thread_stack_node_t;

typedef struct globus_l_thread_stack_manager
{
    globus_l_thread_stack_node_t *        stack;
    int                                   max;
    int                                   top;

} globus_l_thread_stack_manager_t;


static int
globus_l_thread_common_activate(void);

static int
globus_l_thread_common_deactivate(void);

static void 
globus_l_thread_blocking_callback_destroy(void* p);

static globus_thread_key_t              l_thread_stack_key  = GLOBUS_NULL;
static globus_bool_t                    globus_l_mod_active = GLOBUS_FALSE;

globus_module_descriptor_t              globus_i_thread_common_module =
{
   "globus_thread_common",
    globus_l_thread_common_activate,
    globus_l_thread_common_deactivate,
    GLOBUS_NULL,
    GLOBUS_NULL
};       

static globus_l_thread_stack_manager_t *
globus_l_thread_blocking_callback_init()
{
   globus_l_thread_stack_manager_t *         manager; 

   manager = (globus_l_thread_stack_manager_t *)
		globus_malloc(sizeof(globus_l_thread_stack_manager_t));

   manager->top = -1;
   manager->max = THREAD_STACK_INIT_SIZE;
   manager->stack = (globus_l_thread_stack_node_t *)
		      globus_malloc(sizeof(globus_l_thread_stack_node_t) *
		      THREAD_STACK_INIT_SIZE);

   return manager;
}



void
globus_i_thread_report_bad_rc(int rc,
			      char *message )
{
    char achMessHead[] = "[Thread System]";
    char achDesc[GLOBUS_L_LIBC_MAX_ERR_SIZE];
    
    if(rc != GLOBUS_SUCCESS)
    {
	switch( rc )
	{
	case EAGAIN:
	    strcpy(achDesc, "system out of resources (EAGAIN)");
	    break;
	case ENOMEM:
	    strcpy(achDesc, "insufficient memory (ENOMEM)");
	    break;
	case EINVAL:
	    strcpy(achDesc, "invalid value passed to thread interface (EINVAL)");
	    break;
	case EPERM:
	    strcpy(achDesc, "user does not have adequate permission (EPERM)");
	    break;
	case ERANGE:
	    strcpy(achDesc, "a parameter has an invalid value (ERANGE)");
	    break;
	case EBUSY:
	    strcpy(achDesc, "mutex is locked (EBUSY)");
	    break;
	case EDEADLK:
	    strcpy(achDesc, "deadlock detected (EDEADLK)");
	    break;
	case ESRCH:
	    strcpy(achDesc, "could not find specified thread (ESRCH)");
	    break;
	default:
	    globus_fatal("%s %s\n%s unknown error number: %d\n",
				  achMessHead,
				  message,
				  achMessHead,
				  rc);
	    break;
	}
	globus_fatal("%s %s\n%s %s",
			      achMessHead,
			      message,
			      achMessHead,
			      achDesc);
    }
} /* globus_i_thread_report_bad_rc() */


/*
 *
 */
globus_thread_result_t
globus_thread_blocking_callback_push(
    globus_thread_blocking_func_t        func,
    void * user_args,
    globus_thread_callback_index_t * i)
{
   globus_l_thread_stack_node_t *            n;
   globus_l_thread_stack_manager_t *         manager; 

   if(!globus_l_mod_active)
   {
       return GLOBUS_FAILURE;
   }

   /*
    *  If module not yet activated return -1
    */

   manager = (globus_l_thread_stack_manager_t *)
		       globus_thread_getspecific(l_thread_stack_key);
   /*
    *  If first time push is called on this thread create a new 
    *  manager structure.
    */
   if(manager == NULL)
   {
       manager = globus_l_thread_blocking_callback_init();
   }

   manager->top = manager->top + 1;
   n = &(manager->stack[manager->top]);

   n->func = func;
   n->user_args = user_args;
   n->enabled = GLOBUS_TRUE;

   if(i != NULL)
   {
       *i = manager->top;
   }
   if(manager->top >= manager->max - 1)
   {
       manager->max += THREAD_STACK_INIT_SIZE;
       manager->stack = (globus_l_thread_stack_node_t *)
       realloc((void*)manager->stack, 
                    sizeof(globus_l_thread_stack_node_t) * manager->max);
	   
   }
   globus_thread_setspecific(l_thread_stack_key,
			         (void *)manager);

   return GLOBUS_SUCCESS;
}

/*
 *
 */
globus_thread_result_t
globus_thread_blocking_callback_pop(
    globus_thread_callback_index_t * i)
{
   globus_l_thread_stack_manager_t *       manager; 
   
   /*
    *  If module not yet activated return -1
    */
   if(!globus_l_mod_active)
   {
       return GLOBUS_FAILURE;
   }
   
       manager = (globus_l_thread_stack_manager_t *)
  		          globus_thread_getspecific(l_thread_stack_key);

       if(manager == NULL ||
          manager->top < 0)
       {
           return GLOBUS_FAILURE;
       }

       if(i != NULL)
       {
           *i = manager->top;
       }
       manager->top--;

   return GLOBUS_SUCCESS;
}

/*
 *
 */
globus_thread_result_t
globus_thread_blocking_callback_enable(
    globus_thread_callback_index_t * i)
{
    globus_l_thread_stack_manager_t *       manager; 
   
    /*
     *  If module not yet activated return -1
     */
    if(!globus_l_mod_active)
    {
        return GLOBUS_FAILURE;
    }
   
    manager = (globus_l_thread_stack_manager_t *)
		       globus_thread_getspecific(l_thread_stack_key);

    if(manager == NULL)
    {
	return GLOBUS_FAILURE;
    }

    if(*i <= manager->top)
    {
        manager->stack[*i].enabled = GLOBUS_TRUE;
    }

    return GLOBUS_SUCCESS;
}

void
globus_thread_blocking_reset()
{
    globus_l_thread_stack_manager_t *       manager; 

    manager = (globus_l_thread_stack_manager_t *)
		       globus_thread_getspecific(l_thread_stack_key);
    globus_l_thread_blocking_callback_destroy(manager);
}

/*
 *
 */
globus_thread_result_t
globus_thread_blocking_callback_disable(
					globus_thread_callback_index_t * i)
{
    globus_l_thread_stack_manager_t *  manager; 
   
    /*
     *  If module not yet activated return -1
     */
    if(!globus_l_mod_active)
    {
        return GLOBUS_FAILURE;
    }
   
    manager = (globus_l_thread_stack_manager_t *)
		       globus_thread_getspecific(l_thread_stack_key);

    if(manager == NULL)
    {
	return GLOBUS_FAILURE;
    }

    if(*i <= manager->top)
    {
        manager->stack[*i].enabled = GLOBUS_FALSE;
    }

    return GLOBUS_TRUE;
}

/*
 *
 */
globus_thread_result_t
globus_thread_blocking_will_block()
{
    int                                       ctr;
    globus_thread_blocking_func_t             func;
    globus_l_thread_stack_manager_t *         manager; 

    
    /*
     *  If module not yet activated return -1
     */
    if(!globus_l_mod_active)
    {
        return GLOBUS_FAILURE;
    }

    manager = (globus_l_thread_stack_manager_t *)
		       globus_thread_getspecific(l_thread_stack_key);

    if(manager == NULL)
    {
        return GLOBUS_FAILURE;
    }

    for(ctr = manager->top;  ctr >= 0; ctr--)
    {
       if(manager->stack[ctr].enabled)
       {
           func =  (manager->stack[ctr].func);
	   func(ctr, manager->stack[ctr].user_args);
       }
    }

    return GLOBUS_SUCCESS;
}

int
globus_l_thread_common_activate(void)
{
    int rc;

    rc = globus_thread_key_create(&l_thread_stack_key,
			          globus_l_thread_blocking_callback_destroy);

    if(rc == 0)
    {
        globus_l_mod_active  = GLOBUS_TRUE;
    }
    return rc;
}

int
globus_l_thread_common_deactivate(void)
{
    return GLOBUS_SUCCESS;
}

static void 
globus_l_thread_blocking_callback_destroy(void* p)
{
     globus_l_thread_stack_manager_t * manager = 
             (globus_l_thread_stack_manager_t*)p;
 
     if(!manager)
       return;
     free(manager->stack);
     free(manager);
    globus_thread_setspecific(l_thread_stack_key,
                                 (void *)GLOBUS_NULL);
}

void thread_print(char * s, ...)
{
    char tmp[1023];
    int x;
    va_list ap;
    pid_t   pid = getpid();
    
#ifdef HAVE_STDARG_H
        va_start(ap, s);
#else
	va_start(ap);
#endif

#if !defined(BUILD_LITE)
    sprintf(tmp, "p#%dt#%ld::", pid, (long)globus_thread_self());
    x = strlen(tmp);
    vsprintf(&tmp[x], s, ap);

    globus_libc_printf(tmp);
    globus_thread_yield();
#else
    sprintf(tmp, "p#%dt#main::", pid);
    x = strlen(tmp);
    vsprintf(&tmp[x], s, ap);
    printf(tmp);
#endif
   
    fflush(stdin);
}

int
globus_i_thread_ignore_sigpipe(void)
{
    struct sigaction act;
    struct sigaction oldact;
    int rc;
    int save_errno;

    memset(&oldact, '\0', sizeof(struct sigaction));

    do
    {
        rc = sigaction(SIGPIPE, GLOBUS_NULL, &oldact);
	save_errno = errno;
    } while(rc < 0 && save_errno == EINTR);

    if(rc != GLOBUS_SUCCESS)
    {
	return rc;
    }
    else if(oldact.sa_handler != SIG_DFL)
    {
	return GLOBUS_SUCCESS;
    }
    else
    {
        memset(&act, '\0', sizeof(struct sigaction));
        sigemptyset(&(act.sa_mask));
        act.sa_handler = SIG_IGN;
        act.sa_flags = 0;

        return sigaction(SIGPIPE, &act, GLOBUS_NULL);
    }
}
/* globus_i_thread_ignore_sigpipe() */
