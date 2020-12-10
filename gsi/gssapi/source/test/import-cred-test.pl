#! /usr/bin/perl

use strict;
use File::Copy qw(copy);
use File::Temp qw(tempfile tempdir);
use Test::More;

my @tests = ();
my $current_test = "";
my $test_exec = "./import-cred-test";

sub test_import_data
{
    my $data = "";
    my ($cert, $key);

    open($cert, "testcred.cert");
    $data .= join("", <$cert>);

    open($key, "testcred.key");
    $data .= join("", <$key>);

    ok(system($test_exec, "-o", "0", "-i", $data) == 0, "$current_test");
}

sub test_import_file
{
    my ($fh, $tempfile);
    my $data = "";
    my ($cert, $key);

    ($fh, $tempfile) = tempfile();

    open($cert, "testcred.cert");
    $data .= join("", <$cert>);

    open($key, "testcred.key");
    $data .= join("", <$key>);

    $fh->write($data);

    ok(system($test_exec, "-o", "1", "-i", "p=$tempfile") == 0,
        "$current_test");
}

sub test_import_dir
{
    my $tempdir = tempdir(CLEANUP => 1);
    my $data = "";

    copy("testcred.cert", "$tempdir/hostcert.pem");
    chmod(0644, "$tempdir/hostcert.pem");

    copy("testcred.key", "$tempdir/hostkey.pem");
    chmod(0600, "$tempdir/hostkey.pem");

    ok(system($test_exec, "-o", "1", "-i", "p=$tempdir") == 0,
        "$current_test");
}

sub test_import_vhost_intermediate
{
    my $tempdir = tempdir(CLEANUP => 1);
    my $data = "";
    my $cert_dir = "${tempdir}/certificates";
    my $vhost_dir = "${tempdir}/vhosts";
    my $test_vhost = "${tempdir}/vhosts/testcred";

    my $old_cert_dir = $ENV{X509_CERT_DIR};
    my $old_vhost_dir = $ENV{X509_VHOST_CRED_DIR};
    my $ca_hash;

    $ENV{X509_CERT_DIR} = $cert_dir;
    $ENV{X509_VHOST_CRED_DIR} = $vhost_dir;

    chomp(my $cahash = `openssl x509 -in ca.cacert -hash -noout`);
    mkdir($cert_dir, 0755);
    copy("ca.cacert", "$tempdir/certificates/$ca_hash.0");
    copy("ca.signing_policy", "$tempdir/certificates/$ca_hash.signing_policy");

    mkdir($vhost_dir, 0755);
    mkdir(${test_vhost}, 0755);
    copy("testcred.cert", "${test_vhost}/hostcert.pem");
    chmod(0644, "${test_vhost}/hostcert.pem");

    copy("testcred.key", "${test_vhost}/hostkey.pem");
    chmod(0600, "${test_vhost}/hostkey.pem");

    copy("intermediate.cacert", "${test_vhost}/chain.pem");
    chmod(0644, "${test_vhost}/chain.pem");

    chomp(my $output = `$test_exec -o 1 -i p=${test_vhost} -D`);
    my $rc = $?;

    ok($rc == 0 && scalar(grep /BEGIN CERTIFICATE/, split(/\n/, $output)) == 2,
       $current_test);

    if ($old_cert_dir) {
        $ENV{X509_CERT_DIR} = $old_cert_dir;
    } else {
        delete $ENV{X509_CERT_DIR};
    }

    if ($old_vhost_dir) {
        $ENV{X509_VHOST_CRED_DIR} = $old_vhost_dir;
    } else {
        delete $ENV{X509_VHOST_CRED_DIR} ;
    }
}

push(@tests, 'test_import_data');
push(@tests, 'test_import_file');
push(@tests, 'test_import_dir');
push(@tests, 'test_import_vhost_intermediate');

plan tests => scalar(@tests);

foreach (@tests)
{
    $current_test = $_;
    eval $current_test;
}
