#!/usr/bin/env perl

=pod

=head1 Tests for the globus openssl error code

Tests to exercise the error handling functionality of the globus
 openssl error library.

=cut

use strict;
use Test::More;

my $test_prog = 'globus_openssl_error_test';

my @tests;
my @todo;

sub basic_func
{
    my ($errors,$rc) = ("",0);
   
    $rc = system("./$test_prog 1>$test_prog.log.stdout 2>$test_prog.log.stderr");

    if($rc != 0)
    {
        $errors .= "Test exited with $rc. ";
    }

    if($rc & 128)
    {
        $errors .= "\n# Core file generated.";
    }
    open(EXPECTED, "<$test_prog.stdout") || die "Couldn't open $test_prog.stdout: $!\n";
    open(LOGGED, "<$test_prog.log.stdout") || die "Couldn't open $test_prog.log.stdout: $!\n";
    $rc = 0;
    while ( my $line = <EXPECTED> )
    {
        my $logged = <LOGGED>;
        $rc++ unless ( $logged =~ /$line/ );
    }
					   
    if($rc != 0)
    {
        $errors .= "Test produced unexpected output, see $test_prog.log.stdout";
    }

    ok($errors eq '', 'globus_error_wrap_openssl_error');

    if($errors eq '')
    {
        if( -e "$test_prog.log.stdout" )
        {
	    unlink("$test_prog.log.stdout");
        }
    }
    else
    {
        ok($errors, 'success');
    }

}

sub sig_handler
{
    if( -e "$test_prog.log.stdout" )
    {
        unlink("$test_prog.log.stdout");
    }
}

$SIG{'INT'}  = 'sig_handler';
$SIG{'QUIT'} = 'sig_handler';
$SIG{'KILL'} = 'sig_handler';


push(@tests, "basic_func();");

# Now that the tests are defined, set up the Test to deal with them.
plan tests => scalar(@tests), todo => \@todo;

# And run them all.
foreach (@tests)
{
    eval "&$_";
}
