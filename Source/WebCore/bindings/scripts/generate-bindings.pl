#!/usr/bin/perl -w
#
# Copyright (C) 2005 Apple Inc.
# Copyright (C) 2006 Anders Carlsson <andersca@mac.com>
# 
# This file is part of WebKit
# 
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
# 
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
# 
# You should have received a copy of the GNU Library General Public License
# along with this library; see the file COPYING.LIB.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
# Boston, MA 02110-1301, USA.
# 

# This script is a temporary hack. 
# Files are generated in the source directory, when they really should go
# to the DerivedSources directory.
# This should also eventually be a build rule driven off of .idl files
# however a build rule only solution is blocked by several radars:
# <rdar://problems/4251781&4251785>

use strict;
use FindBin;
use lib '.', $FindBin::Bin;

use File::Path;
use File::Basename;
use Getopt::Long;
use Text::ParseWords;
use Cwd;

use IDLParser;
use CodeGenerator;

my @idlDirectories;
my $outputDirectory;
my $outputHeadersDirectory;
my $generator;
my $defines;
my $filename;
my $prefix;
my $preprocessor;
my $writeDependencies;
my $verbose;
my $supplementalDependencyFile;
my $additionalIdlFiles;
my $idlAttributesFile;

GetOptions('include=s@' => \@idlDirectories,
           'outputDir=s' => \$outputDirectory,
           'outputHeadersDir=s' => \$outputHeadersDirectory,
           'generator=s' => \$generator,
           'defines=s' => \$defines,
           'filename=s' => \$filename,
           'prefix=s' => \$prefix,
           'preprocessor=s' => \$preprocessor,
           'verbose' => \$verbose,
           'write-dependencies' => \$writeDependencies,
           'supplementalDependencyFile=s' => \$supplementalDependencyFile,
           'additionalIdlFiles=s' => \$additionalIdlFiles,
           'idlAttributesFile=s' => \$idlAttributesFile);

my $targetIdlFile = $ARGV[0];

die('Must specify input file.') unless defined($targetIdlFile);
die('Must specify generator') unless defined($generator);
die('Must specify output directory.') unless defined($outputDirectory);

if (!$outputHeadersDirectory) {
    $outputHeadersDirectory = $outputDirectory;
}
$targetIdlFile = Cwd::realpath($targetIdlFile);
if ($verbose) {
    print "$generator: $targetIdlFile\n";
}
my $targetInterfaceName = fileparse(basename($targetIdlFile), ".idl");

my $idlFound = 0;
my @supplementedIdlFiles;
if ($supplementalDependencyFile) {
    # The format of a supplemental dependency file:
    #
    # DOMWindow.idl P.idl Q.idl R.idl
    # Document.idl S.idl
    # Event.idl
    # ...
    #
    # The above indicates that DOMWindow.idl is supplemented by P.idl, Q.idl and R.idl,
    # Document.idl is supplemented by S.idl, and Event.idl is supplemented by no IDLs.
    # The IDL that supplements another IDL (e.g. P.idl) never appears in the dependency file.
    open FH, "< $supplementalDependencyFile" or die "Cannot open $supplementalDependencyFile\n";
    while (my $line = <FH>) {
        my ($idlFile, @followingIdlFiles) = split(/\s+/, $line);
        if ($idlFile and basename($idlFile) eq basename($targetIdlFile)) {
            $idlFound = 1;
            @supplementedIdlFiles = sort @followingIdlFiles;
        }
    }
    close FH;

    # $additionalIdlFiles is list of IDL files which should not be included in
    # DerivedSources*.cpp (i.e. they are not described in the supplemental
    # dependency file) but should generate .h and .cpp files.
    if (!$idlFound and $additionalIdlFiles) {
        my @idlFiles = shellwords($additionalIdlFiles);
        $idlFound = grep { $_ and basename($_) eq basename($targetIdlFile) } @idlFiles;
    }

    if (!$idlFound) {
        my $codeGen = CodeGenerator->new(\@idlDirectories, $generator, $outputDirectory, $outputHeadersDirectory, $preprocessor, $writeDependencies, $verbose);

        # We generate empty .h and .cpp files just to tell build scripts that .h and .cpp files are created.
        generateEmptyHeaderAndCpp($codeGen->FileNamePrefix(), $targetInterfaceName, $outputHeadersDirectory, $outputDirectory);
        exit 0;
    }
}

# Parse the target IDL file.
my $targetParser = IDLParser->new(!$verbose);
my $targetDocument = $targetParser->Parse($targetIdlFile, $defines, $preprocessor);

if ($idlAttributesFile) {
    my $idlAttributes = loadIDLAttributes($idlAttributesFile);
    checkIDLAttributes($idlAttributes, $targetDocument, basename($targetIdlFile));
}

foreach my $idlFile (@supplementedIdlFiles) {
    next if $idlFile eq $targetIdlFile;

    my $interfaceName = fileparse(basename($idlFile), ".idl");
    my $parser = IDLParser->new(!$verbose);
    my $document = $parser->Parse($idlFile, $defines, $preprocessor);

    foreach my $interface (@{$document->interfaces}) {
        if (!$interface->isPartial || $interface->type->name eq $targetInterfaceName) {
            my $targetDataNode;
            my @targetGlobalContexts;
            foreach my $interface (@{$targetDocument->interfaces}) {
                if ($interface->type->name eq $targetInterfaceName) {
                    $targetDataNode = $interface;
                    my $exposedAttribute = $targetDataNode->extendedAttributes->{"Exposed"} || "Window";
                    $exposedAttribute = substr($exposedAttribute, 1, -1) if substr($exposedAttribute, 0, 1) eq "(";
                    @targetGlobalContexts = split(",", $exposedAttribute);
                    last;
                }
            }
            die "Not found an interface ${targetInterfaceName} in ${targetInterfaceName}.idl." unless defined $targetDataNode;

            # Support for attributes of partial interfaces.
            foreach my $attribute (@{$interface->attributes}) {
                next unless shouldPropertyBeExposed($attribute, \@targetGlobalContexts);

                # Record that this attribute is implemented by $interfaceName.
                $attribute->extendedAttributes->{"ImplementedBy"} = $interfaceName if $interface->isPartial;

                # Add interface-wide extended attributes to each attribute.
                foreach my $extendedAttributeName (keys %{$interface->extendedAttributes}) {
                    $attribute->extendedAttributes->{$extendedAttributeName} = $interface->extendedAttributes->{$extendedAttributeName};
                }
                push(@{$targetDataNode->attributes}, $attribute);
            }

            # Support for methods of partial interfaces.
            foreach my $function (@{$interface->functions}) {
                next unless shouldPropertyBeExposed($function, \@targetGlobalContexts);

                # Record that this method is implemented by $interfaceName.
                $function->extendedAttributes->{"ImplementedBy"} = $interfaceName if $interface->isPartial;

                # Add interface-wide extended attributes to each method.
                foreach my $extendedAttributeName (keys %{$interface->extendedAttributes}) {
                    $function->extendedAttributes->{$extendedAttributeName} = $interface->extendedAttributes->{$extendedAttributeName};
                }
                push(@{$targetDataNode->functions}, $function);
            }

            # Support for constants of partial interfaces.
            foreach my $constant (@{$interface->constants}) {
                next unless shouldPropertyBeExposed($constant, \@targetGlobalContexts);

                # Record that this constant is implemented by $interfaceName.
                $constant->extendedAttributes->{"ImplementedBy"} = $interfaceName if $interface->isPartial;

                # Add interface-wide extended attributes to each constant.
                foreach my $extendedAttributeName (keys %{$interface->extendedAttributes}) {
                    $constant->extendedAttributes->{$extendedAttributeName} = $interface->extendedAttributes->{$extendedAttributeName};
                }
                push(@{$targetDataNode->constants}, $constant);
            }
        } else {
            die "$idlFile is not a supplemental dependency of $targetIdlFile. There maybe a bug in the the supplemental dependency generator (preprocess-idls.pl).\n";
        }
    }
}

# Generate desired output for the target IDL file.
my $codeGen = CodeGenerator->new(\@idlDirectories, $generator, $outputDirectory, $outputHeadersDirectory, $preprocessor, $writeDependencies, $verbose, $targetIdlFile);
$codeGen->ProcessDocument($targetDocument, $defines);

# Attributes / Operations / Constants of supplemental interfaces can have an [Exposed=XX] attribute which restricts
# on which global contexts they should be exposed.
sub shouldPropertyBeExposed
{
    my ($context, $targetGlobalContexts) = @_;

    my $exposed = $context->extendedAttributes->{Exposed};

    return 1 unless $exposed;

    $exposed = substr($exposed, 1, -1) if substr($exposed, 0, 1) eq "(";
    my @sourceGlobalContexts = split(",", $exposed);

    for my $targetGlobalContext (@$targetGlobalContexts) {
        return 1 if grep(/^$targetGlobalContext$/, @sourceGlobalContexts);
    }
    return 0;
}

sub generateEmptyHeaderAndCpp
{
    my ($prefix, $targetInterfaceName, $outputHeadersDirectory, $outputDirectory) = @_;

    my $headerName = "${prefix}${targetInterfaceName}.h";
    my $cppName = "${prefix}${targetInterfaceName}.cpp";
    my $contents = "/*
    This file is generated just to tell build scripts that $headerName and
    $cppName are created for ${targetInterfaceName}.idl, and thus
    prevent the build scripts from trying to generate $headerName and
    $cppName at every build. This file must not be tried to compile.
*/
";
    open FH, "> ${outputHeadersDirectory}/${headerName}" or die "Cannot open $headerName\n";
    print FH $contents;
    close FH;

    open FH, "> ${outputDirectory}/${cppName}" or die "Cannot open $cppName\n";
    print FH $contents;
    close FH;
}

sub loadIDLAttributes
{
    my $idlAttributesFile = shift;

    my %idlAttributes;
    open FH, "<", $idlAttributesFile or die "Couldn't open $idlAttributesFile: $!";
    while (my $line = <FH>) {
        chomp $line;
        next if $line =~ /^\s*#/;
        next if $line =~ /^\s*$/;

        if ($line =~ /^\s*([^=\s]*)\s*=?\s*(.*)/) {
            my $name = $1;
            $idlAttributes{$name} = {};
            if ($2) {
                foreach my $rightValue (split /\|/, $2) {
                    $rightValue =~ s/^\s*|\s*$//g;
                    $rightValue = "VALUE_IS_MISSING" unless $rightValue;
                    $idlAttributes{$name}{$rightValue} = 1;
                }
            } else {
                $idlAttributes{$name}{"VALUE_IS_MISSING"} = 1;
            }
        } else {
            die "The format of " . basename($idlAttributesFile) . " is wrong: line $.\n";
        }
    }
    close FH;

    return \%idlAttributes;
}

sub checkIDLAttributes
{
    my $idlAttributes = shift;
    my $document = shift;
    my $idlFile = shift;

    foreach my $interface (@{$document->interfaces}) {
        checkIfIDLAttributesExists($idlAttributes, $interface->extendedAttributes, $idlFile);

        foreach my $attribute (@{$interface->attributes}) {
            checkIfIDLAttributesExists($idlAttributes, $attribute->extendedAttributes, $idlFile);
        }

        foreach my $function (@{$interface->functions}) {
            checkIfIDLAttributesExists($idlAttributes, $function->extendedAttributes, $idlFile);
            foreach my $argument (@{$function->arguments}) {
                checkIfIDLAttributesExists($idlAttributes, $argument->extendedAttributes, $idlFile);
            }
        }
    }
}

sub checkIfIDLAttributesExists
{
    my $idlAttributes = shift;
    my $extendedAttributes = shift;
    my $idlFile = shift;

    my $error;
    OUTER: for my $name (keys %$extendedAttributes) {
        if (!exists $idlAttributes->{$name}) {
            $error = "Unknown IDL attribute [$name] is found at $idlFile.";
            last OUTER;
        }
        if ($idlAttributes->{$name}{"*"}) {
            next;
        }
        for my $rightValue (split /\s*[|&]\s*/, $extendedAttributes->{$name}) {
            if (!exists $idlAttributes->{$name}{$rightValue}) {
                $error = "Unknown IDL attribute [$name=" . $extendedAttributes->{$name} . "] is found at $idlFile.";
                last OUTER;
            }
        }
    }
    if ($error) {
        die "IDL ATTRIBUTE CHECKER ERROR: $error
If you want to add a new IDL attribute, you need to add it to WebCore/bindings/scripts/IDLAttributes.txt and add explanations to the WebKit IDL document (https://trac.webkit.org/wiki/WebKitIDL).
";
    }
}
