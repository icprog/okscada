#!/usr/bin/perl

use strict;

my $author_name = "Victor Semizarov";
my $author_email = "vitkinet" . '@' . "gmail" . '.' . "com";

my $postamble_bugs = "BUGS\n----\nSee the Optikus distribution BUGS file.\n\n\n";

my $postamble_resources = <<End_Of_Resources;
RESOURCES
---------
SourceForge: 'http://sourceforge.net/projects/asciidoc'

Main web site: 'http://optikus.sourceforge.net'


End_Of_Resources

my $postamble_copying = <<End_Of_Copying;
COPYING
-------
Copyright \\(C) 2006-2007 vitki.net.
The Optikus library is free software. You can redistribute and/or
modify it under the the terms of the GNU General Public License (GPL).

End_Of_Copying

my $postamble_all = "$postamble_bugs$postamble_resources$postamble_copying";

my ($mode, $article, $sect, $manfile, $textfile);
my ($heading, $heading_underline, $nblank);
my (@names, $names, @proto, $proto, %acc);

my $libref = defined $ARGV[0] ? $ARGV[0] : "lib-reference.txt";

open(LR, $libref) || die "cannot open $libref\n";
$mode = "init";
my $allcount = 0;
my $newcount = 0;
while(<LR>)
{
  if ($mode eq "init") {
    next unless /^>>>\s+(\w+)\s*\((\d+)\)\s*$/;
    $mode = "next";
    ($article, $sect) = ($1, $2);
    $_ = <LR>;
  }

  if ($mode eq "next") {
    last unless defined $article;
    $sect = 3 unless defined $sect;
    $manfile = "$article.$sect";
    $textfile = "$manfile.txt";
    $heading = "$article($sect)";
    $heading_underline = '=' x length($heading);
    @names = ();
    push @names, $article;
    @proto = ();
    $proto = "";
    $acc{desc} = $acc{ret} = $acc{see} = $acc{note} = "";
    $nblank = 0;
    $mode = "proto";
  }

  if ($mode eq "proto") {
    if (/^\s\s+(\S.*?)\s*$/) {
      my $s = $1;
      $s =~ s/(\*+)/\$\$\1\$\$/g;
      $s = "*$s*;";
      push @proto, $s;
      next;
    }
    $mode = "desc";
  }

  if (exists $acc{$mode}) {
    if (/^>>>\s+(\w+)\s*\((\d+)\)\s*$/ && $nblank > 0) {
      ($article, $sect) = ($1, $2);
      make_article();
      $mode = "next";
      next;
    }
    if (/^\s*$/) {
      $nblank++;
    } else {
      $nblank = 0;
    }
    if (/^\(\*(\w+)\*\)\s*$/) {
       my $m = $1;
       $m =~ tr/A-Z/a-z/;
       $m = "ret" if $m eq "retval" or $m eq "returns";
       if (exists $acc{$m}) {
         $mode = $m;
         next;
       }
    }
    $acc{$mode} .= $_;
    next;
  }
}
make_article() if length($article);
close(LR);
print "made $newcount out of $allcount articles for $libref\n";


sub make_article
{
  $names = join ',', @names;

  $proto = join '\n', @proto;
  $proto = "blah blah" unless length($proto);

  for (keys %acc) {
    $acc{$_} =~ s/^\s*//;
    $acc{$_} =~ s/\s*$//;
  }

  $acc{desc} = "blah blah blah" unless length($acc{desc});
  $acc{ret} = "RETURN VALUE\n------------\n$acc{ret}\n\n\n" if length($acc{ret});
  $acc{see} = "SEE ALSO\n--------\n$acc{see}\n\n\n" if length($acc{see});
  $acc{note} = "NOTE\n----\n$acc{note}\n\n\n" if length($acc{note});

  #print "\t$manfile \\\n";
  output();
}


sub output
{
  my $old_text = "";
  if (open(OLD, $textfile)) {
    while (<OLD>)  { $old_text .= $_; }
    close(OLD);
  }
  my $new_text = <<End_Of_Article;
$heading
$heading_underline
$author_name <$author_email>

NAME
----
$names - ...


SYNOPSIS
--------
*#include <optikus/watch.h>*

$proto


DESCRIPTION
-----------
$acc{desc}


$acc{ret}$acc{see}$acc{note}${postamble_all}
End_Of_Article
  unless ($old_text eq $new_text) {
    open(NEW, "> $textfile") || die "cannot create $textfile\n";
    print NEW $new_text;
    close(NEW);
    $newcount++;
  } else {
    if (-w $textfile) {
      my $text_age = (-M $textfile);
      system("touch $textfile");
      if (-w $manfile) {
        my $man_age = (-M $manfile);
        if ($man_age <= $text_age) {
          system("touch $manfile");
        }
      }
    }
  }
  $allcount++;
  close(A);
}

