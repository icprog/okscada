#!/usr/bin/perl
print "this utility converts ini files to strings which can be used with iniCache\n";
my $fname = $ARGV[0];
die "usage: $0 ini_file_name\n" unless $fname;
open(F,"<$fname") or die "cannot open $fname\n";
my $cname = $fname;
$cname .= ".c";
open (C,">$cname") or die "cannot create $cname\n";
my $n = 0;
my $vname = "str_$fname";
$vname =~ y/a-zA-Z0-9/_/cs;
print C "\nconst char *$vname = \n";
while(<F>) {
  $n++;
  chop;chomp;
  s/\t/\\t/g;
  s/\r//g;
  s/\\/\\\\/g;
  s/\"/\\\"/g;
  print C "/*".sprintf("%03d",$n)."*/ \"$_\\n\"\n";
}
print C ";\n\n";
print C "iniCache(\"$fname\",$vname);\n\n";
close F;
close C;
print "$cname OK\n";
