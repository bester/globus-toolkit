/*
 * Copyright 1999-2006 University of Chicago
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 * http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "globus_common.h"

int
main()
{
    int rc;
    printf("1..1\n");
    
    rc = (sizeof(globus_off_t) < 8);
    
    printf("%sok 1 globus_off_t_at_least_64_bits\n", rc == 0 ? "" : "not ");

    return rc;
}
/* main() */
