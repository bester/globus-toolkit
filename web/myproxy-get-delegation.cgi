#!/usr/bin/perl -w
# Thanks to Steve Mock @SDSC for expect stuff
#
use CGI qw/:standard/;

use Expect;

# Edit this line to reflect location of myproxy-get-delegation
my $program  = "/home/novotny/bin/myproxy-get-delegation"; 

my $username    = param(USERNAME);
my $password    = param(PASSWORD);
my $lifetime    = param(LIFETIME);

my $outfile = "$username.cred";
my $args     = "-s localhost -l $username -t $lifetime -o $outfile";

# Check length of password
my $len = length ($password);
if (($len < 5) || ($len > 10))
{
  passwordtoolong();
}

# zap everything past first nonword character
$username =~ s/\W.*//;

# use expect to run the command
my $cmd_filehandle = Expect->spawn("$program $args");

# this looks for the string "myproxy-server:" for 20 seconds
# and failing that, does the "error" subroutine.
unless ($cmd_filehandle->expect(20, "myproxy-server:")) 
{
  printerror();
}

print $cmd_filehandle "$password\n";

# gather the output into the array
@output = <$cmd_filehandle>;

# close the filehandle to the command
$cmd_filehandle->soft_close();

# now you have an array called @outputmsg which has the rest of the output... 
# get rid of output[0], since it contains the password

$outputmsg = join(" ", $output[1]);
if ($outputmsg =~ /^Error/) {
    $outputmsg =~ s/(.*):\s//;
    &printerror($outputmsg);
} else {
    &printsuccess;
}

sub passwordtoolong
{
    print header;
    print "<BODY BGCOLOR=#efefef>";
    print "<TITLE>Incorrect Password</TITLE>";
    print "<H1><FONT FACE=Arial COLOR=Red><STRONG>";
    print "The password must be between 5 and 10 characters.";
    print "</STRONG></FONT></H1>";
    exit;
}

sub printerror
{
    my $errmsg = $_[0];
    print header;
    print "<BODY BGCOLOR=#efefef>"; 
    print "<TITLE>Error!</TITLE>";
    print "<H1><FONT FACE=Arial COLOR=Red><STRONG>";
    print "Error executing myproxy-get-delegation!\n";
    print "</STRONG></FONT></H1>";
    print "$errmsg";
    exit;
}

sub printsuccess
{
    print header;
    print "<BODY BGCOLOR=#efefef>"; 
    print  "<TITLE>Error!</TITLE>";
    print "<H1><FONT FACE=Arial COLOR=Blue><STRONG>";
    print "Received a delegated proxy for $username good for $lifetime hours.";
    print "</STRONG></FONT></H1>";
    exit;
}
