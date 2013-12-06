#!/usr/bin/env perl

# 
# Copyright 1999-2006 University of Chicago
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
# http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# 

my $rc;
my $count = 1;

if(!defined($ENV{FTP_TEST_NO_GSI}))
{
    $count = 2;
}

for(my $i = 0; $i < $count; $i++)
{
    $rc = system('./run-tests.pl -runserver');
    if($rc != 0)
    {
        exit $rc;
    }
    
    if(defined($ENV{GLOBUS_TEST_EXTENDED}))
    {
        $rc = system('./many-server.pl');
        if($rc != 0)
        {
            exit $rc;
        }
    }
    $ENV{FTP_TEST_NO_GSI} = 1;
}
exit $rc;
