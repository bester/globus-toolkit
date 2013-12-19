#!/usr/bin/env perl

use strict;
use POSIX;
use Test;

my $test_prog = 'gssapi-import-context-test';

my $valgrind = "";
if (exists $ENV{VALGRIND})
{
    $valgrind = "valgrind --log-file=VALGRIND-gssapi_import_context_test.log";
    if (exists $ENV{VALGRIND_OPTIONS})
    {
        $valgrind .= ' ' . $ENV{VALGRIND_OPTIONS};
    }
}
sub basic_func
{
    my ($errors,$rc) = ("",0);
   
    $rc = system("$valgrind ./$test_prog");

    if ($rc != 0)
    {
        $errors .= "Test exited with $rc. ";
    }

    if ($rc & 128)
    {
        $errors .= "\n# Core file generated.";
    }
   
    if ($errors eq "")
    {
        ok('success', 'success');
    }
    else
    {
        ok($errors, 'success');
    }

}

my @tests = ("basic_func();");
my @todo = ();

# Now that the tests are defined, set up the Test to deal with them.
plan tests => scalar(@tests), todo => \@todo;

# And run them all.
foreach (@tests)
{
    eval "&$_";
}
