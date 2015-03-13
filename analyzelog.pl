#!/usr/bin/perl
use warnings;
use strict;

use feature 'say';

use Algorithm::Diff 'LCS_length';
use Time::Local;
use POSIX;

sub Main;
sub GetTimestamp($);
sub Usage();

exit Main(@ARGV);

sub Main
{

    my $ARGC = scalar(@_);
    if ($ARGC < 1)
    {
        Usage();
        return 1;
    }
    my $filename = $_[0];
    my $timeSlice;
    my $pct;

    if ($ARGC > 2)
    {
        $pct = $_[2];
    } else
    {
        $pct = 0.8;
    }

    if ($ARGC > 1)
    {
        $timeSlice = $_[1] * 60;
    } else
    {
        $timeSlice = 30 * 60;
    }

    my @messages;
    my $sliceBeginning = 0;
    my $timestamp = 0;
    my $contents = '';

    # Main hash.  Full breakdown of slices
    my %slices;

    open(my $file, '<', $_[0]);
    while (my $line = <$file>)
    {
        chomp($line);
        my $timestamp;

        # If the line has a timestamp, use it, otherwise use the slice beginning
        if ($line =~ m/^([-\d]+\s+[\d:,]+\s+\d+)\s+(.+)$/)
        {
            $timestamp = GetTimestamp($1);
            my $length = length($2);
            if ($length > 100)
            {
                $contents = substr($2, 0, 100);
            } else
            {
                $contents = $2;
            }
        } else
        {
            next;
        }

        # If the timestamp out-slices the current slice beginning, advance the slice beginning
        while (($timestamp - $sliceBeginning) > $timeSlice)
        {
            $sliceBeginning += $timeSlice;
        }
        my @contentList = split /\b\s*/, $contents;

        # The length of the longest common subsequence (LCS) for each message
        my $longestLength = 0;
        # The LCS's closeness to the current message
        my $lengthPct = 0;
        # The index of the LCS
        my $longestIndex = -1;
        # The length of the current message
        my $fullLength = scalar(@contentList);

        for (my $i = 0; $i < scalar(@messages); ++$i)
        {
            my $message = $messages[$i][1];
            my $length = LCS_length(\@contentList, $message);
            if ($length > $longestLength)
            {
                $longestIndex = $i;
                $longestLength = $length;
                $lengthPct = $length / $fullLength;
            }
        }

        if ($lengthPct >= $pct)
        {
            if (exists($slices{$sliceBeginning}[$longestIndex]))
            {
                $slices{$sliceBeginning}[$longestIndex] += 1;
            } else
            {
                $slices{$sliceBeginning}[$longestIndex] = 1;
            }
        } else
        {
            $slices{$sliceBeginning}[scalar(@messages)] = 1;

            # Push the message list into it
            push(@messages, [$contents, \@contentList]);
        }
    }
    close($file);

    print('Time');

    for (my $i = 0; $i < scalar(@messages); ++$i)
    {
        my $message = $messages[$i][0];
        $message =~ s/"/""/g;
        print(",\"$message\"");
    }

    print("\n");

    for my $key (keys(%slices))
    {
        print(POSIX::strftime('%F %T', gmtime($key)));
        my $slice = $slices{$key};

        for (my $i = 0; $i < scalar(@messages); ++$i)
        {
            if (exists($slice->[$i]))
            {
                print(",$slice->[$i]");
            } else
            {
                print(",0");
            }
        }
        print("\n");
    }

    return 0;
}

sub GetTimestamp($)
{
    my $time = 0;
    if ($_[0] =~ m/^\s*(\d+)-(\d+)-(\d+)\s+(\d+):(\d+)(?::(\d+)(?:,(\d+))?)?\s*(\d*)\s*$/)
    {
        my $msec = $7 or 0;
        my $sec = ($6 or 0) + ($msec / 1000);
        my $min = $5;
        my $hour = $4;
        my $mday = $3;
        my $mon = $2;
        my $year = $1;
        $time = timegm($sec, $min, $hour, $mday, $mon - 1, $year - 1900);
    }
    return $time;
}

sub Usage()
{
    print qq(USAGE:
    $0 {filename} [{slice size in minutes} [{pct match required}]]
        Analize a log by time slices and its percent matching required in its timestamps
        Default slice size is 30 minutes, default pct match is 80% (done by words).
        Outputs csv to standard output.
);
}
