/******************************************************************************
Description:

  XXX - fill this in

CVS Information:

  $Source$
  $Date$
  $Revision$
  $State$
  $Author$
******************************************************************************/

/******************************************************************************
			     Include header files
******************************************************************************/
#include "config.h"
#include "globus_common.h"

/******************************************************************************
			       Type definitions
******************************************************************************/

#define UNNECESSARY 0

/*
 * data structure needed to implement a recursive mutex
 */
typedef struct
{
    globus_mutex_t			mutex;
    globus_cond_t			cond;
    globus_thread_t			thread_id;
    int					level;
} globus_l_module_mutex_t;

/*
 * data structures for a hash table entry and the associated key
 */
typedef globus_module_activation_func_t globus_l_module_key_t;
typedef struct
{
    globus_module_descriptor_t *	descriptor;
    globus_list_t *			clients;
    int					reference_count;
} globus_l_module_entry_t;

/******************************************************************************
		       Define module specific variables
******************************************************************************/

globus_bool_t
globus_i_module_initialized = GLOBUS_FALSE;

static globus_bool_t
globus_l_environ_initialized = GLOBUS_FALSE;
static globus_bool_t
globus_l_environ_mutex_initialized = GLOBUS_FALSE;

/* Recursive mutex to protect internal data structures */
static globus_l_module_mutex_t		globus_l_module_mutex;

/* Hash table and list to maintain a table of registered modules */
const int GLOBUS_L_MODULE_TABLE_SIZE = 13;
static globus_hashtable_t		globus_l_module_table;
static globus_list_t *			globus_l_module_list;

/* Hash table for globus_environ*/
const int GLOBUS_L_ENVIRON_TABLE_SIZE = 13;
static globus_mutex_t		globus_l_environ_hashtable_mutex;
static globus_hashtable_t		globus_l_environ_table;

#if defined(HAVE_ONEXIT)
#    define atexit(a) on_exit(a,GLOBUS_NULL)
#endif

#if defined(HAVE_ATEXIT) || defined(HAVE_ONEXIT)
globus_list_t *globus_l_module_atexit_funcs = GLOBUS_NULL;
#endif

/******************************************************************************
		      Module specific function prototypes
******************************************************************************/
static void
globus_l_module_initialize();

static globus_bool_t
globus_l_module_increment(
    globus_module_descriptor_t *	module_descriptor,
    globus_l_module_key_t		parent_key);

static globus_bool_t
globus_l_module_decrement(
    globus_module_descriptor_t *	module_descriptor,
    globus_l_module_key_t		parent_key);

static
int
globus_l_module_reference_count(
    globus_module_descriptor_t *	module_descriptor);
/******************************************************************************
		      Recursive mutex function prototypes
******************************************************************************/
static void
globus_l_module_mutex_init(
    globus_l_module_mutex_t *		mutex);

static void
globus_l_module_mutex_lock(
    globus_l_module_mutex_t *		mutex);

#if UNNECESSARY
static int
globus_l_module_mutex_get_level(
    globus_l_module_mutex_t *		mutex);
#endif

static void
globus_l_module_mutex_unlock(
    globus_l_module_mutex_t *		mutex);
    
#if UNNECESSARY
static void
globus_l_module_mutex_destroy(
    globus_l_module_mutex_t *		mutex);
#endif

/******************************************************************************
			   API function definitions
******************************************************************************/

/*
 * globus_module_activate()
 */
int
globus_module_activate(
    globus_module_descriptor_t *	module_descriptor)
{
    static globus_l_module_key_t	parent_key = GLOBUS_NULL;

    int					ret_val;
    globus_l_module_key_t		parent_key_save;
    
    /*
     * If this is the first time this routine has been called, then we need to
     * initialize the internal data structures and activate the threads
     * packages if the system has been configured to use threads.
     */
    if (globus_i_module_initialized == GLOBUS_FALSE)
    {
	globus_i_module_initialized = GLOBUS_TRUE;
	globus_l_module_initialize();
/*	globus_i_module_initialized = GLOBUS_TRUE;*/
    }

    /*
     * Once the recursive mutex has been acquired, increment the reference
     * counter for this module, and call it's activation function if it is not
     * currently active.
     */
    globus_l_module_mutex_lock(&globus_l_module_mutex);
    {
	ret_val = GLOBUS_SUCCESS;

	if (module_descriptor->activation_func != GLOBUS_NULL)
	{
	    if (globus_l_module_increment(module_descriptor,
					  parent_key) == GLOBUS_TRUE)
	    {
		parent_key_save = parent_key;
		parent_key = module_descriptor->activation_func;
		
		ret_val = module_descriptor->activation_func();

		/*
		 * Set up the exit handler
		 */
#               if defined(HAVE_ATEXIT) || defined(HAVE_ONEXIT)
		{
		    if(module_descriptor->atexit_func != GLOBUS_NULL)
		    {
			/* only call the atexit function once */
			if(!globus_list_search(
			    globus_l_module_atexit_funcs,
			    (void *) module_descriptor->atexit_func))
			{
			    globus_list_insert(
				&globus_l_module_atexit_funcs,
				(void *) module_descriptor->atexit_func);

			    atexit(module_descriptor->atexit_func);
			}
		    }
		}
#               endif

		parent_key = parent_key_save;
	    }
	}
    }
    globus_l_module_mutex_unlock(&globus_l_module_mutex);

    return ret_val;
}
/* globus_module_activate() */

/*
 * globus_module_deactivate()
 */
int
globus_module_deactivate(
    globus_module_descriptor_t *	module_descriptor)
{
    static globus_l_module_key_t	parent_key = GLOBUS_NULL;

    int					ret_val;
    globus_l_module_key_t		parent_key_save;


    /*
     * If module activation hasn't been initialized then return an error
     */
    if (!globus_i_module_initialized)
    {
	return GLOBUS_FAILURE;
    }
    
    /*
     * Once the recursive mutex has been acquired, decrement the reference
     * counter for this module, and call it's deactivation function if it is
     * no longer being used.
     */
    globus_l_module_mutex_lock(&globus_l_module_mutex);
    {
	ret_val = GLOBUS_SUCCESS;

	if (module_descriptor->activation_func != GLOBUS_NULL)
	{
	    if (globus_l_module_decrement(module_descriptor,
					  parent_key) == GLOBUS_TRUE)
	    {
		parent_key_save = parent_key;
		parent_key = module_descriptor->activation_func;
		
		if(module_descriptor->deactivation_func != NULL)
		{
		    ret_val = module_descriptor->deactivation_func();
		}

		parent_key = parent_key_save;
	    }
	    else if(globus_l_module_reference_count(module_descriptor) == 0)
            {
                ret_val = GLOBUS_FAILURE;
            }
	}
    }
    globus_l_module_mutex_unlock(&globus_l_module_mutex);

    return ret_val;
}
/* globus_module_deactivate() */

/*
 * globus_module_deactivate_all()
 */
int
globus_module_deactivate_all(void)
{
    /*
     * If module activation hasn't been initialized then return an error
     */
    if (!globus_i_module_initialized)
    {
	return GLOBUS_FAILURE;
    }
    
    globus_l_module_mutex_lock(&globus_l_module_mutex);
    {
	globus_bool_t			 deactivated_one;

	deactivated_one = GLOBUS_TRUE;

	while(deactivated_one)
	{
	    globus_list_t *		module_list;

	    module_list = globus_l_module_list;
	    deactivated_one = GLOBUS_FALSE;

	    while(!globus_list_empty(module_list))
	    {
		globus_l_module_entry_t *module_entry;

		module_entry = globus_list_first(module_list);
		module_list = globus_list_rest(module_list);
	    
		if(globus_list_empty(module_entry->clients) &&
		   module_entry->reference_count > 0)
		{
		    globus_module_deactivate(module_entry->descriptor);
		    deactivated_one = GLOBUS_TRUE;
		}
	    }
	}
    }
    globus_l_module_mutex_unlock(&globus_l_module_mutex);

    return GLOBUS_SUCCESS;
}
/* globus_module_deactivate_all() */

/*
 * globus_module_get_module_pointer()
 *
 */

void *
globus_module_get_module_pointer(
    globus_module_descriptor_t *	structptr)
{
    void * retptr;
    void * (*module_func)();

    module_func=structptr->get_pointer_func;

    if (module_func!=NULL)
    {	
        retptr=(*module_func)();
    }
    else
    {
        retptr=GLOBUS_NULL;
    }

    return(retptr);
} 
/*globus_module_get_module_pointer();*/


/*
 * globus_module_setenv();
 */ 

void
globus_module_setenv(
       char * name,
       char * value)
{
    int				rc;

    /*
     *  First, check to see if the environment mutex has been initialized
     */

    if(globus_l_environ_mutex_initialized == GLOBUS_FALSE)
    {
	if(globus_i_module_initialized == GLOBUS_TRUE)
	{
	    rc = globus_mutex_init(&globus_l_environ_hashtable_mutex,
                           (globus_mutexattr_t *) GLOBUS_NULL);
            globus_assert (rc == 0);
	    globus_l_environ_mutex_initialized = GLOBUS_TRUE;
	}
    }
   
    /*
     *  then, check to see if the environment hash table has been initialized
     */
 

    if((globus_l_environ_initialized == GLOBUS_FALSE))
    {
	if(globus_i_module_initialized==GLOBUS_TRUE)
	{
	    globus_mutex_lock(&globus_l_environ_hashtable_mutex);
	}

        globus_hashtable_init(&globus_l_environ_table,
                          GLOBUS_L_ENVIRON_TABLE_SIZE,
                          globus_hashtable_string_hash,
                          globus_hashtable_string_keyeq);

	globus_l_environ_initialized = GLOBUS_TRUE;

	if(globus_i_module_initialized == GLOBUS_TRUE)
	{
	    globus_mutex_unlock(&globus_l_environ_hashtable_mutex);
	}
    }

    /*
     *  Then actually put the name and value into the hash table
     */

    if(globus_i_module_initialized == GLOBUS_TRUE)
    {
	globus_mutex_lock(&globus_l_environ_hashtable_mutex);
    }

    globus_hashtable_remove(
	&globus_l_environ_table,
	name);
    globus_hashtable_insert(
         &globus_l_environ_table,
         name,
         value);

    if(globus_i_module_initialized == GLOBUS_TRUE)
    {
	globus_mutex_unlock(&globus_l_environ_hashtable_mutex);
    }

}
/*globus_module_setenv();*/

/*
 * globus_module_getenv();
 */

char * 
globus_module_getenv(
       char * name)
{
    char * 			entry;

    if((globus_l_environ_initialized == GLOBUS_TRUE))
    {
	if((globus_i_module_initialized == GLOBUS_TRUE)
	    &&(globus_l_environ_mutex_initialized == GLOBUS_TRUE))
	{
	    globus_mutex_lock(&globus_l_environ_hashtable_mutex);
	}

        entry =
           globus_hashtable_lookup(
               &globus_l_environ_table,
               name); 


	if((globus_i_module_initialized == GLOBUS_TRUE)
	    &&(globus_l_environ_mutex_initialized == GLOBUS_TRUE))
	{
	    globus_mutex_unlock(&globus_l_environ_hashtable_mutex);
	}
    }
    else
    {
        entry=GLOBUS_NULL;
    }

    /*
     *  If we have found an entry, return it
     */

    if (entry!=GLOBUS_NULL)
    {
	return(entry);
    }

    /*
     *  otherwise check system environment
     */

    entry=getenv(name);

    if (entry!=NULL)
    {
	return(entry);
    }

    return(GLOBUS_NULL);
}
/*globus_module_getenv();*/


/**
 * get version associated with module
 *
 * This function copies the version structure associated with the module
 * into 'version'.
 *
 * @param module_descriptor
 *        pointer to a module descriptor (module does NOT need to be activated)
 *
 * @param version
 *        pointer to storage for a globus_version_t.  The version will be
 *        copied into this
 *
 * @return
 *        - GLOBUS_SUCCESS
 *        - GLOBUS_FAILURE if there is no version associated with this module
 *          (module->version == null)
 */

int
globus_module_get_version(
    globus_module_descriptor_t *	module_descriptor,
    globus_version_t *                  version)
{
    globus_version_t *                  module_version;
    
    module_version = module_descriptor->version;
    
    if(!module_version)
    {
        return GLOBUS_FAILURE;
    }
    
    version->major      = module_version->major;       
    version->minor      = module_version->minor;       
    version->timestamp  = module_version->timestamp;   
    version->branch_id  = module_version->branch_id;   

    return GLOBUS_SUCCESS;
}


/**
 * print module's version
 *
 * This function prints a modules version info using the standard form 
 * provided by globus_version_print
 *
 * @param module_descriptor
 *        pointer to a module descriptor (module does NOT need to be activated)
 *
 * @param stream
 *        stream to print on (stdout, stderr, etc)
 *
 * @param verbose
 *        If GLOBUS_TRUE, then all available version info is printed 
 *        (ex: globus_module: 1.1 (1013708618-5))
 *        else, only the major.minor is printed (ex: globus_module: 1.1)
 *
 * @return
 *        - void
 */

void
globus_module_print_version(
    globus_module_descriptor_t *	module_descriptor,
    FILE *                              stream,
    globus_bool_t                       verbose)
{
    globus_version_print(
        module_descriptor->module_name,
        module_descriptor->version,
        stream,
        verbose);
}

/**
 * print all activated modules' versions
 *
 * This function prints all activated modules' version info using the standard 
 * form provided by globus_version_print
 *
 * @param stream
 *        stream to print on (stdout, stderr, etc)
 *
 * @param verbose
 *        If GLOBUS_TRUE, then all available version info is printed 
 *        (ex: globus_module: 1.1 (1013708618-5))
 *        else, only the major.minor is printed (ex: globus_module: 1.1)
 *
 * @return
 *        - void
 */

void
globus_module_print_activated_versions(
    FILE *                              stream,
    globus_bool_t                       verbose)
{
    /*
     * If module activation hasn't been initialized then there are no
     * activated modules
     */
    if(!globus_i_module_initialized)
    {
        return;
    }
    
    globus_l_module_mutex_lock(&globus_l_module_mutex);
    {
        globus_list_t *		        module_list;
        
        module_list = globus_l_module_list;
        while(!globus_list_empty(module_list))
        {
            globus_l_module_entry_t *       module_entry;
    
            module_entry = globus_list_first(module_list);
            module_list = globus_list_rest(module_list);
            
            if(module_entry->reference_count > 0)
            {
                globus_version_print(
                    module_entry->descriptor->module_name,
                    module_entry->descriptor->version,
                    stream,
                    verbose);
            }
        }
    }
    globus_l_module_mutex_unlock(&globus_l_module_mutex);

    return;
}


/**
 * print version structure
 *
 * This function provides a stand way of printing version information
 * The version is printed to stream with the following form:
 * name: major.minor                        if verbose = false
 * name: major.minor.timestamp-branch_id    if verbose = true
 *
 * In either case, if name is NULL, then only the numerical version will be 
 * printed.
 *
 * @param name
 *        A string to prefix the version.  May be NULL
 *
 * @param version
 *        The version structure containing the info to print.
 *        (May be NULL, although pointless to do so)
 *
 * @param stream
 *        stream to print on (stdout, stderr, etc)
 *
 * @param verbose
 *        If GLOBUS_TRUE, then all available version info is printed 
 *        (ex: globus_module: 1.1 (1013708618-5))
 *        else, only the major.minor is printed (ex: globus_module: 1.1)
 *
 * @return
 *        - void
 */

void
globus_version_print(
    const char *                        name,
    const globus_version_t *            version,
    FILE *                              stream,
    globus_bool_t                       verbose)
{
    if(name)
    {
        globus_libc_fprintf(stream, "%s: ", name);
    }
    
    if(version)
    {
        if(verbose)
        {
            globus_libc_fprintf(
                stream, 
                "%d.%d (%lu-%d)\n", 
                version->major,
                version->minor,
                version->timestamp,
                version->branch_id);
        }
        else
        {
            globus_libc_fprintf(
                stream, 
                "%d.%d\n", 
                version->major,
                version->minor);
        }
    }
    else
    {
        globus_libc_fprintf(stream, "<no version>\n");
    }
}


/******************************************************************************
		     Module specific function definitions
******************************************************************************/

/*
 * globus_l_module_initialize()
 */
static void
globus_l_module_initialize()
{
    /*
     * Initialize the threads package (can't use the standard interface since
     * it depends on threads)
     */
    globus_i_thread_pre_activate();
    globus_i_memory_pre_activate();
    /*
     * Initialize the registered module table and list
     */
    globus_hashtable_init(&globus_l_module_table,
			  GLOBUS_L_MODULE_TABLE_SIZE,
			  globus_hashtable_voidp_hash,
			  globus_hashtable_voidp_keyeq);

    globus_l_module_list = GLOBUS_NULL;
    
    /*
     * Initialize the recursive mutex
     */
    globus_l_module_mutex_init(&globus_l_module_mutex);

    /*
     * Now finish initializing the threads package
     */
    globus_module_activate(GLOBUS_THREAD_MODULE);

}
/* globus_l_module_initialize() */


/*
 * globus_l_module_increment()
 */
static globus_bool_t
globus_l_module_increment(
    globus_module_descriptor_t *	module_descriptor,
    globus_l_module_key_t		parent_key)
{
    globus_l_module_entry_t *		entry;
    
    entry =
	globus_hashtable_lookup(
	    &globus_l_module_table,
	    (void *) module_descriptor->activation_func);

    if (entry != GLOBUS_NULL)
    {
	/*
	 * The module has already been registered.  Increment its reference
	 * counter and add any new clients to the dependency list
	 */
	entry->reference_count++;
	if (parent_key != GLOBUS_NULL
	    && globus_list_search(entry->clients,
				  (void *) parent_key) == GLOBUS_NULL)
	{
	    globus_list_insert(&entry->clients, (void *) parent_key);
	}

	if(entry->reference_count == 1)
	{
	    return GLOBUS_TRUE;
	}
	else
	{
    	    return GLOBUS_FALSE;
	}
    }
    else
    {
	/*
	 * This is the first time this module has been registered.  Create a
	 * new entry in the modules table.
	 */
	entry = (globus_l_module_entry_t *)
	    globus_malloc(sizeof(globus_l_module_entry_t));
	globus_assert(entry != GLOBUS_NULL);

	entry->descriptor = module_descriptor;
	entry->reference_count = 1;
	entry->clients = GLOBUS_NULL;
	if (parent_key != GLOBUS_NULL)
	{
	    globus_list_insert(&entry->clients, (void *) parent_key);
	}
	
	globus_hashtable_insert(
	    &globus_l_module_table,
	    (void *) module_descriptor->activation_func,
	    entry);

	globus_list_insert(&globus_l_module_list, entry);
	
	return GLOBUS_TRUE;
    }
}
/* globus_l_module_increment() */

static
int
globus_l_module_reference_count(
    globus_module_descriptor_t *	module_descriptor)
{
    globus_l_module_entry_t *		entry;
    
    entry =
	globus_hashtable_lookup(
	    &globus_l_module_table,
	    (void *) module_descriptor->activation_func);
    if (entry == GLOBUS_NULL || entry->reference_count <= 0)
    {
	return 0;
    }
    else
    {
        return entry->reference_count;
    }
}

/*
 * globus_l_module_decrement()
 */
static globus_bool_t
globus_l_module_decrement(
    globus_module_descriptor_t *	module_descriptor,
    globus_l_module_key_t		parent_key)
{
    globus_l_module_entry_t *		entry;
    
    entry =
	globus_hashtable_lookup(
	    &globus_l_module_table,
	    (void *) module_descriptor->activation_func);
    if (entry == GLOBUS_NULL || entry->reference_count <= 0)
    {
	return GLOBUS_FALSE;
    }

    entry->reference_count--;
    
    if (parent_key != GLOBUS_NULL)
    {
	globus_list_t *			client_entry;

	
	client_entry = globus_list_search(entry->clients,
					  (void *) parent_key);
	globus_assert(client_entry != GLOBUS_NULL);

	globus_list_remove(&entry->clients, client_entry);
    }

    if (entry->reference_count == 0)
    {
	return GLOBUS_TRUE;
    }
    else
    {
	return GLOBUS_FALSE;
    }
}
/* globus_l_module_decrement() */


void
globus_i_module_dump(
    FILE *				out_f)
{
    globus_list_t *			module_list;

    globus_libc_fprintf(out_f, "==========\nModule List\n----------\n");
    
    module_list = globus_l_module_list;
    while(!globus_list_empty(module_list))
    {
	globus_list_t *			client_list;
	globus_l_module_entry_t *	module_entry;

	module_entry = globus_list_first(module_list);
	module_list = globus_list_rest(module_list);

	globus_libc_fprintf(out_f, "%s; cnt=%d",
		module_entry->descriptor->module_name,
		module_entry->reference_count);

	client_list = module_entry->clients;

	if (!globus_list_empty(client_list))
	{
	    void *			client_entry;
	    globus_l_module_entry_t *	client_module_entry;
	    
	    client_entry = globus_list_first(client_list);
	    client_list = globus_list_rest(client_list);
	    client_module_entry =
		globus_hashtable_lookup(&globus_l_module_table, client_entry);
	    globus_libc_fprintf(out_f, "; clients=%s",
		    client_module_entry->descriptor->module_name);
	    
	    while(!globus_list_empty(client_list))
	    {
		client_entry = globus_list_first(client_list);
		client_list = globus_list_rest(client_list);
		client_module_entry =
		    globus_hashtable_lookup(&globus_l_module_table,
					    client_entry);
		globus_libc_fprintf(out_f, ",%s",
			client_module_entry->descriptor->module_name);
	    }
	}

	globus_libc_fprintf(out_f, "\n");
    }

    globus_libc_fprintf(out_f, "==========\n");
}


/******************************************************************************
		     Recursive mutex function definitions
******************************************************************************/

/*
 * globus_l_module_mutex_init()
 */
static void
globus_l_module_mutex_init(
	globus_l_module_mutex_t *		mutex)
{
    globus_mutex_init(&mutex->mutex, (globus_mutexattr_t *) GLOBUS_NULL);
    globus_cond_init(&mutex->cond, (globus_condattr_t *) GLOBUS_NULL);

    mutex->level = 0;
}
/* globus_l_module_mutex_init() */

/*
 * globus_l_module_mutex_lock()
 */
static void
globus_l_module_mutex_lock(
	globus_l_module_mutex_t *		mutex)
{
    globus_mutex_lock(&mutex->mutex);
    {
	globus_assert(mutex->level >= 0);
	
	while (mutex->level > 0
	       && mutex->thread_id != globus_thread_self())
	{
	    globus_cond_wait(&mutex->cond, &mutex->mutex);
	}

	mutex->level++;
	mutex->thread_id = globus_thread_self();
	
    }
    globus_mutex_unlock(&mutex->mutex);
}
/* globus_l_module_mutex_lock() */

#if UNNECESSARY
/*
 * globus_l_module_mutex_get_level()
 */
static int
globus_l_module_mutex_get_level(
	globus_l_module_mutex_t *		mutex)
{
    return mutex->level;
}
/* globus_l_module_mutex_get_level() */
#endif

/*
 * globus_l_module_mutex_unlock()
 */
static void
globus_l_module_mutex_unlock(
	globus_l_module_mutex_t *		mutex)
{
    globus_mutex_lock(&mutex->mutex);
    {
	globus_assert(mutex->level > 0);
	globus_assert(mutex->thread_id == globus_thread_self());
	
	mutex->level--;
	if (mutex->level == 0)
	{
	    globus_cond_signal(&mutex->cond);
	}
    }
    globus_mutex_unlock(&mutex->mutex);
}
/* globus_l_module_mutex_unlock() */

#if UNNECESSARY
/*
 * globus_l_module_mutex_destroy()
 */
static void
globus_l_module_mutex_destroy(
	globus_l_module_mutex_t *		mutex)
{
    globus_mutex_destroy(&mutex->mutex);
    globus_cond_destroy(&mutex->cond);
}
/* globus_l_module_mutex_destroy() */
#endif
