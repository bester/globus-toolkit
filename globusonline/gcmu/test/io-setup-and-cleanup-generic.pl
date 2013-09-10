#! /usr/bin/perl
#
# Copyright 1999-2013 University of Chicago
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

END {$?=0}

use strict;
use File::Path;
use File::Temp;
use IPC::Open3;
use Test::More;

my $config_file = "test-io.conf";

sub setup_server()
{
    my @cmd = ("globus-connect-multiuser-setup", "-c", $config_file, "-v");

    my ($pid, $in, $out, $err);
    $pid = open3($in, $out, $err, @cmd);
    close($in);
    waitpid($pid, 0);
    my $rc = $? >> 8;
    print STDERR join("", <$out>);
    print STDERR join("", <$err>);
    return $rc == 0;
}

sub is_gridftp_running()
{
    my @cmd = ("/etc/init.d/globus-gridftp-server", "status");

    my ($pid, $in, $out, $err);
    $pid = open3($in, $out, $err, @cmd);
    close($in);
    waitpid($pid, 0);
    my $rc = $? >> 8;
    print STDERR join("", <$out>);
    print STDERR join("", <$err>);
    return $rc == 0;
}

sub cleanup()
{
    my @cmd = ("globus-connect-multiuser-cleanup", "-c", $config_file, "-v");
    my ($pid, $in, $out, $err);
    $pid = open3($in, $out, $err, @cmd);
    close($in);
    waitpid($pid, 0);
    my $rc = $? >> 8;
    print STDERR join("", <$out>);
    print STDERR join("", <$err>);

    return $rc == 0;
}

sub force_cleanup()
{
    # Just to make sure that doesn't fail
    foreach my $f (</etc/gridftp.d/globus-connect*>)
    {
        unlink($f);
    }
    foreach my $f (</etc/myproxy.d/globus-connect*>)
    {
        unlink($f);
    }
    File::Path::rmtree("/var/lib/globus-connect-multiuser");
    unlink("/var/lib/myproxy-oauth/myproxy-oauth.db");
}

# Prepare
plan tests => 4;

# Test Step #1:
# Setup ID server
ok(setup_server(), "setup_server");

# Test Step #2:
# Is GridFTP server running?
ok(is_gridftp_running(), "is_gridftp_running");

# Test Step #3:
# Clean up the services
ok(cleanup(), "cleanup");

# Test Step #4:
# Is GridFTP server running?
ok(!is_gridftp_running(), "is_gridftp_not_running");

# Remove everything in GCMU dir
force_cleanup();
