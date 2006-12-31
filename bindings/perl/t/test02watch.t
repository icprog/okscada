#!/usr/bin/perl

use strict;
use FindBin qw"$Bin";
use lib "$Bin/../lib", "$Bin/../blib/arch";

use Test::More tests => 27;

BEGIN { use_ok 'Optikus::Watch', ':all'; }

my $debug = 0;

my %subjects;

sub alive_sub {
  my ($subject, $state) = @_;
  $subjects{$subject} = $state;
  print "$subject ---> $state\n" if $debug;
}

sub data_sub {
  my ($ooid, $value, $data, $name, $type, $len) = @_;
  print "got ooid=$ooid name=\"$name\" value=\"$value\"\n" if $debug;
}

sub arr_dbg_sub
{
  my ($ooid, $value, $data, $name, $type, $len) = @_;
  print "+++ {name=$name}{data=$data}=[$value]\n" if $debug;
}

is ( Optikus::Watch::debug($debug ? -2 : 2), 2,
   , "can change verbosity" );

ok(
  Optikus::Watch::init(client_name => "test02watch",
                       server => "localhost",
                       def_timeout => 1000,
                       conn_timeout => 1000,
                       period => 100,
                       alive_handler => \&alive_sub,
                       data_handler => \&data_sub,
                      )
  , "initialization" );

is ( $subjects{"*"}, "*",
   , "hub subject is online" );

is ( $subjects{"+"}, "+"
   , "domain is full" );

is ( $subjects{sample}, "sample"
   , "sample subject is online" );

is ( keys(%subjects), 3,
   , "there are not spurious subjects" );

is ( Optikus::Watch::subject(), "sample"
   , "subjects include sample subject" );

ok ( Optikus::Watch::idle()
   , "idle running without parameters" );

ok ( Optikus::Watch::idle(10)
   , "idle running for 10 milliseconds" );

my ($param3, $param8);

ok ( tie( $param3, "Optikus::Watch", 'sample@param3' )
   , "can tie 'param3'" );
ok ( tie( $param8, "Optikus::Watch", 'sample@param8' )
   , "can tie 'param8'" );

print "before: param3=$param3 param8=$param8\n" if $debug;
$param8 = $param3;
print "after: param3=$param3 param8=$param8\n" if $debug;

my $name = 'param4';
ok ( Optikus::Watch::write($name, '0.3')
   , "can write to 'param4'" );

my (@info, $ooid, $desc, $type, $len);
@info = Optikus::Watch::info("arr1[0]");
($ooid, $desc, $type, $len) = @info;
is ( "desc:$desc type:$type len:$len", 'desc:sample/.@arr1[0] type:i len:4'
   , "info for arr1[0] is correct" );
print "info for arr1[0]: ooid=$ooid desc=$desc type=$type len=$len\n" if $debug;

@info = Optikus::Watch::info("arr1[9999]");
is ( $#info, -1
   , "arr1[9999] does not exist" );

$ooid = Optikus::Watch::monitor("arr1[0]", \&arr_dbg_sub, 0);
ok ( $ooid
   , "can watch arr1[0]" );
is ( Optikus::Watch::monitor("arr1[0]", \&arr_dbg_sub, 1), $ooid
   , "can rewatch arr1[0]" );
Optikus::Watch::idle(10);
is ( Optikus::Watch::unmonitor("arr1[0]", \&arr_dbg_sub, 1), 1
   , "can unmonitor arr1[0]" );

my $strout;
ok ( tie( $strout, "Optikus::Watch", 'sample@strout' )
   , "can tie 'strout'" );
$strout = "";
is ( $strout, ""
   , "'strout' cleared" );

my @strvars = ('sample@strinc', 'sample@strinh', 'sample@strini',
               'sample@strinf', 'sample@strind');
my @strvals = (ord('x'), -5, 65537, -5.02, 833);
my $nvars = $#strvars + 1;
my ($written, $checked, $tied);

$written = $checked = 0;
for my $var (@strvars) {
  $written++ if Optikus::Watch::write($var, 0);
  $checked++ if Optikus::Watch::read($var) == 0;
}
is ( $written, $nvars
   , "writing input zeros" );
is ( $checked, $nvars
   , "checking input zeros" );

my ($strinc, $strinh, $strini, $strinf, $strind);
$tied = 0;
$tied++ if tie( $strinc, 'Optikus::Watch', 'sample@strinc' );
$tied++ if tie( $strinh, 'Optikus::Watch', 'sample@strinh' );
$tied++ if tie( $strini, 'Optikus::Watch', 'sample@strini' );
$tied++ if tie( $strinf, 'Optikus::Watch', 'sample@strinf' );
$tied++ if tie( $strind, 'Optikus::Watch', 'sample@strind' );
is ( $tied, $nvars
   , "can tie different types" );

$written = $checked = 0;
for (my $i = 0; $i < $nvars; $i++) {
  $written++ if Optikus::Watch::write($strvars[$i], $strvals[$i]);
  my $val = Optikus::Watch::read($strvars[$i]);
  if ($val == $strvals[$i]) {
    $checked++;
  } elsif ($strvars[$i] eq 'sample@strinf') {
    my $dif = abs($val - $strvals[$i]);
    if ($dif < 1.0e-7) {
      $checked++;
      print "info: tolerable difference in 'strinf': $dif\n";
    } else {
      print "intolerable $strvars[$i]: wanted $strvals[$i] got $val\n";
    }
  } else {
    print "unexpected $strvars[$i]: wanted $strvals[$i] got $val\n";
  }
}
is ( $written, $nvars
   , "writing input values" );
is ( $checked, $nvars
   , "checking input values" );
Optikus::Watch::idle(200);
is ( $strout, "c=x h=-5 i=65537 f=-5.02 d=833.00"
   , "output is correct" );

ok ( Optikus::Watch::exit()
   , "exitting" );

