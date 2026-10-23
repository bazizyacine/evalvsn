#!/usr/bin/perl
use strict;
use warnings;
use File::Path qw(rmtree mkpath);
use File::Basename;
use Getopt::Long;
use List::Util qw(min);

# Check for help option
if (grep { $_ eq '-h' || $_ eq '--help' } @ARGV) {
	print_help();
}

# Print the help text
sub print_help {
	print <<"END_HELP";
Usage: $0 [options]

Options:
  -ip <file>      Input parameters file
  -vf <file>      Video file
  -od <dir>       Output directory
  -xl <label>     X axis label
  -metrics <list> What to compute: metric names or group names, comma-separated
                  (default: all). See -list.
  -list           Print the metric catalogue and exit
  -h, --help      Show this help message

Example:
  $0 -ip ../ipFiles/GC-PN-0p03-13.dat -vf ../../evalvsn/videos/fire_qcif.avi -od ../sets/set_AOMDV_QPV_GC-PN-0p03-13_p30-n100-d200 -xl GC-PN-0p03-13
  $0 -list
  $0 -metrics core,prio -ip ... -vf ... -od ... -xl GC-PN
END_HELP
	exit 0;
}

# ---------------------------------------------------------------------------
# Metric catalogue
#
# The catalogue is complete and stays complete. A set narrows what it computes
# through -metrics; it never removes anything from here. The thesis draws on
# metrics the Sensors article does not report, and a set computed without them
# has to be run again from scratch to get them back - hours, to save minutes.
my %CATALOGUE = (
    'Total Packets Sent'          => 'core',
    'Packet Delivery Ratio'       => 'core',
    'Packet Loss Ratio'           => 'core',
    'Packet Loss Ratio by Type-M' => 'core',
    'Packet Loss Ratio by Type-D' => 'core',
    'PSNR'                        => 'core',
    'Ref. PSNR'                   => 'core',
    'SSIM'                        => 'quality',
    'Ref. SSIM'                   => 'quality',
    'Mean Compression Ratio'      => 'quality',
    'Energy Consumption'          => 'energy',
    'Power Consumption'           => 'energy',
    'Life Time'                   => 'energy',
    'Delay'                       => 'network',
    'Jitter'                      => 'network',
    'Throughput'                  => 'network',
    'Alert Latency'               => 'application',
    'Alert Delivery'              => 'application',
    'Service Duration'            => 'application',
    'Usable Images'               => 'application',
    'Usable Image Ratio'          => 'application',
);

# The per-priority metrics are named at run time - Packet Loss Ratio by
# Priority-M1..M13 and D1..D13 - because how many exist follows the priorityNb
# of the set. They are matched by prefix rather than listed one by one.
my $PRIO_PREFIX = 'Packet Loss Ratio by Priority-';

# The per-image acceptability criterion of the applicative metrics (28 dB,
# the same floor every collapse frontier is defined by). Declared HERE,
# above the Main call: a file-scope my assigns only when its own line
# executes, and a declaration placed below the call site leaves the sub
# reading undef - a zero threshold that silently turns "first usable
# image" into "first image received at all".
my $USABLE_PSNR_DB = 28;

# Parameters
my $rute            = $ENV{PWD};
my ($ipPath, $videoFile, $outputDirectory, $xLabel);
my $metricsSpec = 'all';
my $listOnly    = 0;
my $runs        = 1;
GetOptions(
    'ip=s'      => \$ipPath,
    'vf=s'      => \$videoFile,
    'od=s'      => \$outputDirectory,
    'xl=s'      => \$xLabel,
    'metrics=s' => \$metricsSpec,
    'runs=i'    => \$runs,
    'list'      => \$listOnly,
) or die "Usage: $0 -ip <file> -vf <file> -od <dir> -xl <label> [-metrics <list>] [-runs <n>]\n";

die "$0: -runs must be a positive integer\n" if $runs < 1;

# What this run is asked to produce. undef means everything.
#
# Parsed before -list is honoured, so that "-metrics <spec> -list" doubles as a
# validator: it exits 0 when the spec is sound and non-zero, with the offending
# token named, when it is not. run_sets.sh uses that to reject a bad sets.conf
# before the encoding rather than after several hours of it.
my $selected = parse_metrics($metricsSpec);

print_catalogue() if $listOnly;

sub print_catalogue {
    my %by_group;
    push @{ $by_group{ $CATALOGUE{$_} } }, $_ for keys %CATALOGUE;
    print "Metric catalogue\n\n";
    for my $g (sort keys %by_group) {
        printf "  %-8s %s\n", $g, join(', ', sort @{ $by_group{$g} });
    }
    printf "  %-8s %sM1..Mn and %sD1..Dn\n", 'prio', $PRIO_PREFIX, $PRIO_PREFIX;
    print  "  all      every metric above (default)\n\n";
    print  "Pass names or groups to -metrics, comma-separated.\n";
    exit 0;
}

sub metric_group {
    my ($metric) = @_;
    return 'prio' if index($metric, $PRIO_PREFIX) == 0;
    return $CATALOGUE{$metric};
}

# Validation happens here rather than at first use: a typo in sets.conf must
# stop the run before the encoding, not after several hours of it.
sub parse_metrics {
    my ($spec) = @_;
    $spec = 'all' unless defined $spec && length $spec;
    return undef if $spec eq 'all';

    my %groups = map { $_ => 1 } (values %CATALOGUE, 'prio');
    my %want;
    for my $tok (split /\s*,\s*/, $spec) {
        next unless length $tok;
        unless ($groups{$tok} || exists $CATALOGUE{$tok}
                || index($tok, $PRIO_PREFIX) == 0) {
            die "$0: unknown metric or group '$tok'. Run '$0 -list' to see what exists.\n";
        }
        $want{$tok} = 1;
    }
    die "$0: -metrics selects nothing\n" unless %want;
    return \%want;
}

sub is_selected {
    my ($metric) = @_;
    return 1 unless defined $selected;          # -metrics all
    return 1 if $selected->{$metric};           # named outright
    my $group = metric_group($metric);
    return 1 if defined $group && $selected->{$group};
    return 0;
}

# Clear the previous results
unlink glob "$rute/*.dat";
unlink glob "$rute/*.tr";
unlink glob "$rute/*.nam";

# Output directories
my $working_folder = "$outputDirectory/graphs";
rmtree($working_folder) if -d $working_folder;

# One directory per metric
my $jitter_folder                   = "$working_folder/Jitter";
my $delay_folder                    = "$working_folder/Delay";
my $pkts_delivery_ratio_folder      = "$working_folder/Packet Delivery Ratio";
my $pkts_loss_ratio_folder          = "$working_folder/Packet Loss Ratio";
my $pkts_loss_ratio_by_type_folder  = "$working_folder/Packet Loss Ratio by Type";
my $pkts_loss_ratio_by_priority_folder = "$working_folder/Packet Loss Ratio by Priority";
my $throughput_folder               = "$working_folder/Throughput";
my $PSNR_folder                     = "$working_folder/PSNR";
my $SSIM_folder                     = "$working_folder/SSIM";
my $power_consumption_folder        = "$working_folder/Power Consumption";
my $lifetime_folder                 = "$working_folder/Life Time";
my $energy_consumption_folder       = "$working_folder/Energy Consumption";

# Metrics held in memory
my %metrics_data;   # $metrics_data{$metric_name}{$simId} = [ valeurs ]
my %metrics_unit; # $metrics_unit{$metric_name} = $unit

# Main
open my $fh, '<', $ipPath or die "Cannot open $ipPath: $!";
setGraphs($fh, $videoFile, $outputDirectory, $xLabel);
close $fh;

# setGraphs: collect, aggregate, write and plot
sub setGraphs {
    my ($fh, $videoFile, $outputDir, $xLabel) = @_;
    my @simid_order;

    # Read and collect the data
    while (my $line = <$fh>) {
        chomp $line;
        $line =~ s/#.*//;
        $line =~ s/^\s+|\s+$//g;
        next unless $line;

        my @fields  = split /\s+/, $line;
        my $simId   = $fields[0] // '';
        my $Encoder = $fields[1] // '';
        # MMPEG, or the JPEG line of the Pham witness (tranche 3b-2): the
        # traces it leaves carry the same columns, one priority level.
        next unless $Encoder eq 'MMPEG' || $Encoder eq 'JPEG';
        push @simid_order, $simId;

        my $videoName = fileparse($videoFile, qr/\.[^.]*/);
        my $rcvFile   = $fields[14] // '';

        # One pass per realisation, all feeding the same simId. record_Metric
        # appends, and the aggregation below already takes the mean and the
        # sample standard deviation of whatever it was given - so several
        # realisations need no aggregation code of their own, only several
        # passes. With -runs 1 this reduces to the historical single pass.
        foreach my $run (1 .. $runs) {
            my $runDir = sprintf('%s/r%02d', $outputDir, $run);
            my $stPath = "$runDir/${videoName}-$simId";

            # A realisation that did not produce its traces is skipped rather
            # than aborting the set: nine good runs out of ten still give a
            # mean and a spread, and the count is what says so.
            unless (-d $stPath) {
                print "  realisation $run: $stPath missing, skipped\n";
                next;
            }

            my $rtPath  = "$stPath/sim";
            my $stName  = "$stPath/st-packet.dat";
            # The reception trace. rcvFile stopped being an ipFile column when
            # the decoding pass moved to -decode; on the modern recipes the
            # field is empty and the name is the chain's own, rt-packet.dat.
            # Without this fallback the name resolved to the sim/ directory
            # itself and every reception-based metric - delay, jitter,
            # throughput - was silently zero.
            my $rcvBase = fileparse($rcvFile);
            $rcvBase    = 'rt-packet.dat' unless length $rcvBase;
            my $rtName  = "$rtPath/$rcvBase";
            my $mtName  = "$rtPath/mt.dat";
            my $trName  = "$stPath/st-packet.tr";
            my $stFrame = "$stPath/st-frame.dat";

            my $maxPrioNb = get_maxPrioNb($stName);

            print "Processing simulation ID: $simId (realisation $run/$runs)\n";

            calculate_Jitter($stName, $rtName, $simId);
            calculate_Packet_Loss_Ratio_by_Type($mtName, $simId);
            calculate_Packet_Loss_Ratio_by_Priority($maxPrioNb, $mtName, $simId);
            calculate_Throughput($rtName, $simId);
            calculate_PSNR($mtName, $simId);
            calculate_SSIM($mtName, $simId);
            calculate_Consumption($trName, $simId);
            calculate_Compression_Ratio($stFrame, $simId);
            calculate_Total_Packets_Sent($stName, $simId);
            calculate_Applicative_Metrics($stName, $rtName, $simId);
        }
    }

    # Aggregated .dat output
    foreach my $metric (sort keys %metrics_data) {
        my $folder = "$working_folder/$metric";
        mkpath($folder) unless -d $folder;
        my $file = "$folder/$metric.dat";
        open my $out, '>', $file or die "Cannot open $file: $!";
        foreach my $sim (@simid_order) {
            next unless exists $metrics_data{$metric}{$sim};
            my @values = @{ $metrics_data{$metric}{$sim} };
            next unless @values;
            my @valid_vals = grep { $_ ne 'na' } @values;
            if (@valid_vals) {
                my $mean = mean(\@valid_vals);
                my $std  = stddev(\@valid_vals);
                # Fourth column: how many realisations this line averages. A
                # mean alone cannot say whether a difference is a result or the
                # width of the noise, and a spread cannot be read without
                # knowing what it was computed over. The plotting and
                # verification scripts read columns 1 and 2 and ignore the rest.
                my $n = scalar @valid_vals;
                print $out "$sim\t$mean\t$std\t$n\n";
            } else {
                print $out "$sim\n";
            }
        }
        close $out;

        # Unit, for the plot
        my $unit = $metrics_unit{$metric} // '';

        # Plot generation
        graph_Metric($metric, $xLabel, $unit, ['eps', 'pdf', 'png']);
    }

    # Multi-plot. A combined plot needs every one of its metrics; when -metrics
    # leaves one out, the plot is skipped rather than drawn against a .dat file
    # that was never written.
    my @combined = (
        [ ['Packet Loss Ratio by Type-M', 'Packet Loss Ratio by Type-D'],
          'Packet Loss Ratio (%)',
          'EvalVSN - Packet Loss Ratio by Type-M vs Type-D',
          "$working_folder/Packet Loss Ratio by Type-M/Packet Loss Ratio by Type-M vs Type-D" ],
        [ ['PSNR', 'Ref. PSNR'],
          'PSNR (dB)',
          'EvalVSN - PSNR vs Ref. PSNR',
          "$working_folder/PSNR/PSNR_vs_Ref_PSNR" ],
        [ ['SSIM', 'Ref. SSIM'],
          'SSIM',
          'EvalVSN - SSIM vs Ref. SSIM',
          "$working_folder/SSIM/SSIM_vs_Ref_SSIM" ],
    );
    foreach my $plot (@combined) {
        my ($names, $yLabel, $title, $target) = @$plot;
        next if grep { !exists $metrics_data{$_} } @$names;
        graph_Multi_Metrics($names, $xLabel, $yLabel, $title, $target,
                            ['eps', 'pdf', 'png']);
    }
}

# The applicative metrics of the survival study (the author's R16 answer of
# 23/08/2026): what a surveillance operator reads, beside the image quality.
#
#   Alert Latency    seconds between detection - the first send instant of
#                    the capture calendar - and the completion of the first
#                    usable image at the sink.
#   Alert Delivery   100 when at least one usable image reached the sink in
#                    this realisation, 0 otherwise. The aggregation's mean
#                    over the realisations IS the delivery probability, in
#                    percent, with no aggregation code of its own.
#   Service Duration seconds between detection and the completion of the
#                    LAST usable image: how long the network kept rendering
#                    its service while - in the fire campaign - the front
#                    was destroying it. 0 when no usable image arrived.
#
# A usable image is a decoded frame at or above the study's own
# acceptability criterion - 28 dB of PSNR, the same floor every collapse
# frontier is defined by - read per frame from the decoder's rt-frame.dat.
# All-packets-received would be the wrong bar: the decoder degrades
# gracefully, and an image missing one refinement packet is still an image
# an operator acts on. A frame's completion instant is the reception time
# of the last of its packets that arrived, read from the reception trace.
# ($USABLE_PSNR_DB itself is declared with the other constants, above the
# Main call - see the note there.)

sub calculate_Applicative_Metrics {
    my ($senderFile, $receiverFile, $simId) = @_;

    # Detection: the capture calendar's first send instant.
    my $detection;
    open my $st, '<', $senderFile or die "Cannot open $senderFile: $!";
    while (my $line = <$st>) {
        next if $line =~ /^\s*#/;
        my ($sTime) = split /\t/, $line;
        next unless defined $sTime && length $sTime;
        $detection = $sTime if !defined $detection || $sTime < $detection;
    }
    close $st;

    # Completion instant of every frame that reached the sink at all.
    my %frame_done;
    open my $rt, '<', $receiverFile or die "Cannot open $receiverFile: $!";
    while (my $line = <$rt>) {
        next if $line =~ /^\s*#/;
        chomp $line;
        my @f = split /\t/, $line;
        next unless @f >= 4;
        my ($rTime, $frameNb) = @f[0, 3];
        $frame_done{$frameNb} = $rTime
            if !defined $frame_done{$frameNb} || $rTime > $frame_done{$frameNb};
    }
    close $rt;

    # Usability from the decoder's own verdict, frame by frame.
    my $frameFile = $receiverFile;
    $frameFile =~ s/[^\/]+$/rt-frame.dat/;
    my ($first_done, $last_done);
    # Counted alongside the span: how many images the operator could act
    # on, and what share of the decoded ones they were. A campaign axis
    # that moves the quality without moving these two has moved the
    # picture; one that moves these has moved the service.
    my ($usable, $decoded) = (0, 0);
    open my $ft, '<', $frameFile or die "Cannot open $frameFile: $!";
    while (my $line = <$ft>) {
        next if $line =~ /^\s*#/;
        chomp $line;
        my ($frameNb, $psnr) = split /\t/, $line;
        next unless defined $psnr && $psnr ne 'na';
        # A frame none of whose packets arrived is the previous image held
        # on screen: neither decoded nor usable (finding X-34).
        my $done = $frame_done{$frameNb};
        next unless defined $done;
        $decoded++;
        next unless $psnr >= $USABLE_PSNR_DB;
        $usable++;
        $first_done = $done if !defined $first_done || $done < $first_done;
        $last_done  = $done if !defined $last_done  || $done > $last_done;
    }
    close $ft;

    my $delivered = defined $first_done ? 100 : 0;
    my $latency   = defined $first_done ? $first_done - $detection : 'na';
    my $service   = defined $last_done  ? $last_done  - $detection : 0;

    print "Alert: delivery $delivered %, latency "
        . ($latency eq 'na' ? 'na' : sprintf('%.3f s', $latency))
        . sprintf(", service %.3f s\n", $service);

    record_Metric('Alert Delivery',   '(%)', $delivered, $simId);
    record_Metric('Alert Latency',    '(s)', $latency,   $simId);
    record_Metric('Service Duration', '(s)', $service,   $simId);
    record_Metric('Usable Images',      '', $usable, $simId);
    record_Metric('Usable Image Ratio', '(%)',
                  $decoded ? 100 * $usable / $decoded : 0, $simId);
}

# Highest priority level present in an st-packet.dat file
sub get_maxPrioNb {
    my ($filename) = @_;
    open my $fh, '<', $filename or die "Cannot open $filename: $!";
    my $maxPrioNb = 0;
    while (my $line = <$fh>) {
        next if $line =~ /^\s*#/; # skip comments
        chomp $line;
        my @fields = split /\t/, $line;
        my $prioNb = $fields[5] // 0; # 6e colonne (index 5)
        $maxPrioNb = $prioNb if $prioNb > $maxPrioNb;
    }
    close $fh;
    return $maxPrioNb;
}

# Record one value for a metric and a simId
sub record_Metric {
    my ($metric, $unit, $value, $simId) = @_;
    return unless defined $value;
    return unless is_selected($metric);
    $metrics_unit{$metric} = $unit if defined $unit;
    push @{ $metrics_data{$metric}{$simId} }, $value;
}

# Statistics
sub mean {
    my ($d) = @_;
    return 0 unless @$d;
    my $s = 0; $s += $_ for @$d;
    return $s / @$d;
}

sub stddev {
    my ($d) = @_;
    my $n = @$d;
    return 0 if $n < 2;
    my $m = mean($d);
    my $sum = 0; $sum += ($_ - $m)**2 for @$d;
    return sqrt($sum / ($n - 1));
}

# Calculation routines, each feeding record_Metric
sub calculate_Jitter {
    my ($senderFile, $receiverFile, $simId) = @_;

    # Read the packets sent
    my %sent_packets;
    my $num_pkts_tx = 0;
    open my $sfh, '<', $senderFile or die "Cannot open $senderFile: $!";
    while (my $line = <$sfh>) {
        next if $line =~ /^\s*#/;
        my @fields = split /\s+/, $line;
        my ($timestamp, $id) = @fields[0,1];
        $sent_packets{$id} = $timestamp;
        $num_pkts_tx++;
    }
    close $sfh;

    # Read the packets received
    my %recv_packets;
    my $num_pkts_recv = 0;
    open my $rfh, '<', $receiverFile or die "Cannot open $receiverFile: $!";
    while (my $line = <$rfh>) {
        next if $line =~ /^\s*#/;
        my @fields = split /\s+/, $line;
        my ($timestamp, $id) = @fields[0,1];
        $recv_packets{$id} = $timestamp;
        $num_pkts_recv++;
    }
    close $rfh;

    # Delay and jitter. The jitter is the mean variation of the delay
    # between two packets received in a row, |d(i) - d(i-1)| (the IP packet
    # delay variation of RFC 3393); it used to be the gap between their
    # arrival times, which measures the pacing, not the delay (X-34).
    my (@delays, @jitters);
    my $prev_delay;
    for my $id (sort { $recv_packets{$a} <=> $recv_packets{$b} } keys %recv_packets) {
        next unless exists $sent_packets{$id};
        my $delay = $recv_packets{$id} - $sent_packets{$id};
        push @delays, $delay;
        push @jitters, abs($delay - $prev_delay) if defined $prev_delay;
        $prev_delay = $delay;
    }

    # Ratios de livraison et perte
    my $delivery_ratio = $num_pkts_tx 
        ? ($num_pkts_recv / $num_pkts_tx) * 100 
        : 0;
    my $loss_ratio     = $num_pkts_tx 
        ? (($num_pkts_tx - $num_pkts_recv) / $num_pkts_tx) * 100 
        : 0;

    # Moyennes
    my $jitter_avg = @jitters 
        ? mean(\@jitters) 
        : 0;
    my $delay_avg  = @delays  
        ? mean(\@delays)  
        : 0;

	print "Jitter: $jitter_avg (s)\n";
	print "Delay: $delay_avg (s)\n";
	print "Packet Delivery Ratio: $delivery_ratio (%)\n";
	print "Packet Loss Ratio: $loss_ratio (%)\n";

    # Enregistrement via record_Metric
    record_Metric('Jitter', '(s)', $jitter_avg, $simId);
    record_Metric('Delay', '(s)', $delay_avg, $simId);
    record_Metric('Packet Delivery Ratio', '(%)', $delivery_ratio, $simId);
    record_Metric('Packet Loss Ratio', '(%)', $loss_ratio, $simId);
}

sub calculate_Packet_Loss_Ratio_by_Type {
    my ($mtFile, $simId) = @_;

    # Initialise
	my %pkts_loss_ratio_by_type;	
    
	open my $fh, '<', $mtFile or die "Cannot open $mtFile: $!";
    my $header = <$fh>;
    chomp $header;
    $header =~ s/^\s*#//; # strip any leading #
    my @columns = split /\s+/, $header;

    # Locate the pktLossM and pktLossD columns
    my ($idx_pktLossM, $idx_pktLossD);
    for my $i (0..$#columns) {
        $idx_pktLossM = $i if $columns[$i] eq 'pktLossM';
        $idx_pktLossD = $i if $columns[$i] eq 'pktLossD';
    }

    while (my $line = <$fh>) {
        next if $line =~ /^\s*#/;
        chomp $line;
        my @fields = split /\s+/, $line;
        $pkts_loss_ratio_by_type{'M'} = (defined $idx_pktLossM && $fields[$idx_pktLossM] ne 'na') ? $fields[$idx_pktLossM] : 'na';
        $pkts_loss_ratio_by_type{'D'} = (defined $idx_pktLossD && $fields[$idx_pktLossD] ne 'na') ? $fields[$idx_pktLossD] : 'na';
        last; # one data line expected
    }
    close $fh;

    # affichage
    print "Packet Loss M: $pkts_loss_ratio_by_type{'M'} (%)\n";
    print "Packet Loss D: $pkts_loss_ratio_by_type{'D'} (%)\n";

	# Record the metrics
    record_Metric('Packet Loss Ratio by Type-M', '(%)', $pkts_loss_ratio_by_type{'M'}, $simId);
    record_Metric('Packet Loss Ratio by Type-D', '(%)', $pkts_loss_ratio_by_type{'D'}, $simId);
}

sub calculate_Packet_Loss_Ratio_by_Priority {
    my ($priorityNb, $mtFile, $simId) = @_;

	# Initialise
	my %pkts_loss_ratio_by_priority;

    open my $fh, '<', $mtFile or die "Cannot open $mtFile: $!";
	my $header = <$fh>;
	chomp $header;
	$header =~ s/^\s*#//;
	my @columns = split /\s+/, $header;

	# Locate the pktlossM1..\$priorityNb and pktlossD1..\$priorityNb columns
	my %idx;
	for my $i (0..$#columns) {
		$idx{$columns[$i]} = $i;
	}

	while (my $line = <$fh>) {
		next if $line =~ /^\s*#/;
		chomp $line;
		my @fields = split /\s+/, $line;
		for my $p (1..$priorityNb) {
			my $colM = "pktlossM$p";
			my $colD = "pktlossD$p";
			my $keyM = "M$p";
			my $keyD = "D$p";
			$pkts_loss_ratio_by_priority{$keyM} = (defined $idx{$colM} && $fields[$idx{$colM}] ne 'na') ? $fields[$idx{$colM}] : 'na';
			$pkts_loss_ratio_by_priority{$keyD} = (defined $idx{$colD} && $fields[$idx{$colD}] ne 'na') ? $fields[$idx{$colD}] : 'na';
		}
		last; # one data line expected
	}
	close $fh;

	# affichage
	print "Packet Loss Ratio by Priority:\n";
	for my $p (1..$priorityNb) {
		print "pktlossM$p: $pkts_loss_ratio_by_priority{\"M$p\"} (%)\n";
		print "pktlossD$p: $pkts_loss_ratio_by_priority{\"D$p\"} (%)\n";
	}

	# Record the metrics
    for my $key (sort keys %pkts_loss_ratio_by_priority) {
        record_Metric("Packet Loss Ratio by Priority-$key", '(%)',
                      $pkts_loss_ratio_by_priority{$key}, $simId);
    }
}

sub calculate_Throughput {
    my ($receiverFile, $simId) = @_;

	# Initialise
	my $total_bits = 0;
    my $first_time;
    my $last_time;
    my $num_pkts = 0;

    open my $fh, '<', $receiverFile or die "Cannot open $receiverFile: $!";
    while (my $line = <$fh>) {
        next if $line =~ /^\s*#/;
        my @fields = split /\s+/, $line;
        next unless @fields >= 3;
        my ($sTime, $seqNb, $pSize) = @fields[0,1,2];
        $total_bits += $pSize * 8;
        $first_time = $sTime if !defined $first_time;
        $last_time = $sTime;
        $num_pkts++;
    }
    close $fh;

    my $duration = (defined $first_time && defined $last_time) ? ($last_time - $first_time) : 0;
    my $throughput = ($duration > 0) ? ($total_bits / 1024 / $duration) : 0; # Kbits/s

    print "Throughput: $throughput Kbits/s ($num_pkts packets, duration: $duration s)\n";

	# Record the metrics
    record_Metric('Throughput', '(Kbits/s)', $throughput, $simId);
}

sub calculate_PSNR {
    my ($mtFile, $simId) = @_;

	# Initialise
	my %psnr;
    
	open my $fh, '<', $mtFile or die "Cannot open $mtFile: $!";
	my $header = <$fh>;
	chomp $header;
	$header =~ s/^\s*#//; # strip any leading #
	my @columns = split /\s+/, $header;

	my ($idx_psnr, $idx_refpsnr) = (-1, -1);
	for my $i (0..$#columns) {
		$idx_psnr    = $i if $columns[$i] eq 'psnr';
		$idx_refpsnr = $i if $columns[$i] eq 'refPSNR';
	}

	while (my $line = <$fh>) {
		next if $line =~ /^\s*#/;
		chomp $line;
		my @fields = split /\s+/, $line;
		$psnr{'psnr'}    = ($idx_psnr    >= 0 && defined $fields[$idx_psnr]    && $fields[$idx_psnr]    ne 'na') ? $fields[$idx_psnr]    : 'na';
		$psnr{'refPSNR'} = ($idx_refpsnr >= 0 && defined $fields[$idx_refpsnr] && $fields[$idx_refpsnr] ne 'na') ? $fields[$idx_refpsnr] : 'na';
		last; # one data line expected
	}
	close $fh;

	print "PSNR: $psnr{'psnr'} (dB)\n";
	print "Ref. PSNR: $psnr{'refPSNR'} (dB)\n";

	# Record the metrics
    record_Metric('PSNR', '(dB)', $psnr{'psnr'}, $simId);
    record_Metric('Ref. PSNR', '(dB)', $psnr{'refPSNR'}, $simId);
}

sub calculate_SSIM {
    my ($mtFile, $simId) = @_;

	# Initialise
	my %ssim;
    
	open my $fh, '<', $mtFile or die "Cannot open $mtFile: $!";
	my $header = <$fh>;
	chomp $header;
	$header =~ s/^\s*#//;
	my @columns = split /\s+/, $header;

	my ($idx_ssim, $idx_refssim) = (-1, -1);
	for my $i (0..$#columns) {
		$idx_ssim    = $i if $columns[$i] eq 'ssim';
		$idx_refssim = $i if $columns[$i] eq 'refSSIM';
	}

	while (my $line = <$fh>) {
		next if $line =~ /^\s*#/;
		chomp $line;
		my @fields = split /\s+/, $line;
		$ssim{'ssim'}    = ($idx_ssim    >= 0 && defined $fields[$idx_ssim]    && $fields[$idx_ssim]    ne 'na') ? $fields[$idx_ssim]    : 'na';
		$ssim{'refSSIM'} = ($idx_refssim >= 0 && defined $fields[$idx_refssim] && $fields[$idx_refssim] ne 'na') ? $fields[$idx_refssim] : 'na';
		last; # one data line expected
	}
	close $fh;

	print "SSIM: $ssim{'ssim'}\n";
	print "Ref. SSIM: $ssim{'refSSIM'}\n";

	# Record the metrics
    record_Metric('SSIM', '', $ssim{'ssim'}, $simId);
    record_Metric('Ref. SSIM', '', $ssim{'refSSIM'}, $simId);
}

sub calculate_Consumption {
    my ($trPath, $simId) = @_;
    
	# --- Read the .tr trace and collect the residual energies ---
    # Every node's last reported energy: the N lines the PHY logs on a
    # reception, and the -Ne field every send, receive, drop and forward
    # line carries for its node -Ni. The N lines alone missed a node that
    # transmits but never receives - at a grid spacing beyond the radio's
    # reach nothing is received anywhere, the trace holds no N line, and
    # the consumption came out as zero while the -Ne fields showed the
    # camera's battery draining (test bench, 14 September 2026).
    open my $fh, '<', $trPath
      or die "Cannot open trace file '$trPath': $!";

    my %res_energy;
    my %res_time;
    my $initial_energy = 0;
    while (my $line = <$fh>) {
        my ($t, $node, $energy);
        if ($line =~ /^N\s/) {
            my @fields = split /\s+/, $line;
            ($t, $node, $energy) = ($fields[2], $fields[4], $fields[6]);
        } elsif ($line =~ /^[srdf]\s.*-t\s+(\S+).*-Ni\s+(\d+).*-Ne\s+(\S+)/) {
            ($t, $node, $energy) = ($1, $2, $3);
        } else {
            next;
        }
        # the highest energy seen is taken as the initial energy
        $initial_energy = $energy if $energy > $initial_energy;
        # the latest report of each node is its residual energy
        if (!defined $res_time{$node} || $t >= $res_time{$node}) {
            $res_energy{$node} = $energy;
            $res_time{$node}   = $t;
        }
    }
    close $fh;

    # --- Consumption per node ---
    my $total_energy = 0;
    my @cons_per_node;
    for my $node (keys %res_energy) {
        my $cons = $initial_energy - $res_energy{$node};
        $total_energy += $cons;
        push @cons_per_node, $cons;
    }
    my $n_nodes    = scalar @cons_per_node;
    my $energy_avg = $n_nodes ? $total_energy / $n_nodes : 0;

    # --- Minimum residual energy, as a percentage: the lifetime indicator ---
    my $min_residual = defined $initial_energy && $initial_energy
                     ? min(values %res_energy)
                     : 0;
    my $lifetime_avg = $initial_energy
                     ? ($min_residual / $initial_energy) * 100
                     : 0;

    # --- Print the metrics ---
    print "total energy consumed: $total_energy (J)\n";
    print "average per node energy: $energy_avg (J)\n";
    print "minimum residual (lifetime): $lifetime_avg (%)\n";

	# --- Record the metrics ---
    record_Metric('Power Consumption', '(J)', $total_energy, $simId);
    record_Metric('Energy Consumption', '(J)', $energy_avg, $simId);
    record_Metric('Life Time', '(%)', $lifetime_avg, $simId);
}

sub calculate_Compression_Ratio {
    my ($stFrameFile, $simId) = @_;

    open my $fh, '<', $stFrameFile or die "Cannot open $stFrameFile: $!";
    my $header = <$fh>;
    chomp $header;
    $header =~ s/^\s*#//;
    my @columns = split /\s+/, $header;

    # Find the index of the compRatio column
    my $idx_compRatio = -1;
    for my $i (0..$#columns) {
        $idx_compRatio = $i if $columns[$i] eq 'compRatio';
    }
    die "compRatio column not found in $stFrameFile" if $idx_compRatio == -1;

    my @ratios;
    while (my $line = <$fh>) {
        next if $line =~ /^\s*#/;
        chomp $line;
        my @fields = split /\s+/, $line;
        next unless defined $fields[$idx_compRatio];
        push @ratios, $fields[$idx_compRatio] if $fields[$idx_compRatio] ne 'na';
    }
    close $fh;

    my $mean_ratio = @ratios ? mean(\@ratios) : 'na';

    print "Mean Compression Ratio (%): $mean_ratio\n";

    record_Metric('Mean Compression Ratio', '(%)', $mean_ratio, $simId);
}

sub calculate_Total_Packets_Sent {
    my ($stName, $simId) = @_;
    open my $fh, '<', $stName or die "Cannot open $stName: $!";
    my $maxSeqNb = 0;
    while (my $line = <$fh>) {
        next if $line =~ /^\s*#/; # skip comments
        chomp $line;
        my @fields = split /\t/, $line;
        my $seqNb = $fields[1] // 0; # 2e colonne (index 1)
        $maxSeqNb = $seqNb if $seqNb > $maxSeqNb;
    }
    close $fh;

    my $total_pkts = $maxSeqNb + 1;

    print "Total Packets Sent: $total_pkts\n";
    
    record_Metric('Total Packets Sent', '', $total_pkts, $simId);
}

# PNG at 600 dpi is what the article ships: MDPI lists PNG, JPEG and TIFF, and
# PDF is not on that list even though it is vector.
#
# The PNG is rendered from the PDF, not re-plotted. gnuplot's png terminal
# re-lays the figure out at different font metrics, and at the size 600 dpi
# demands the twelve rotated x tick labels collide into an unreadable smear.
# Rendering the PDF keeps the drawing gnuplot already validated, pixel for
# pixel. 17cm x 10cm at 600 dpi is 4009 x 2359 px.
sub render_png {
    my ($pdf, $png) = @_;
    unless (-f $pdf) {
        warn "render_png: $pdf missing, PNG skipped\n";
        return;
    }
    (my $base = $png) =~ s/\.png$//;
    system('pdftoppm', '-png', '-r', 600, '-singlefile', $pdf, $base) == 0
        or warn "render_png: pdftoppm failed on $pdf - is poppler-utils installed?\n";
}

# Graphes
sub graph_Metric {
    my ($metric, $xlabel, $unit, $formats) = @_;
    # pdf before png: the PNG is rendered from the PDF written on the pass before.
    $formats ||= ['eps', 'pdf', 'png'];
    my $folder = "$working_folder/$metric";
    my $datafile = "$folder/$metric.dat";
    my $title = "EvalVSN - $metric";
    my $ylabel = $metric;
    $ylabel =~ s/_/ /g;
    $ylabel .= " $unit" if $unit;

    foreach my $fmt (@$formats) {
        my $outfile = "$folder/$metric.$fmt";
        if ($fmt eq 'png') {
            render_png("$folder/$metric.pdf", $outfile);
            next;
        }
        my $terminal;
        if ($fmt eq 'pdf') {
            $terminal = "set terminal pdf dashed size 17cm,10cm";
        } elsif ($fmt eq 'eps') {
            $terminal = "set terminal postscript eps color dashed size 17cm,10cm";
        } else {
            die "graph_Metric: unsupported format '$fmt'\n";
        }

        open my $GNUPLOT, "|-", "gnuplot" or die "Cannot open gnuplot: $!";
        print $GNUPLOT <<"gnuplot_commands";
set title "$title"
set xlabel "$xlabel"
set xtics rotate by -30
set ylabel "$ylabel"
set key outside right
set key vertical
set key spacing 1.5
set key box
set grid
$terminal
set output "$outfile"
set style line 1 lc rgb 'red' lt 1 lw 2 pt 7 pi -1 ps 1.5
plot '$datafile' using 2:xticlabels(1) with linespoints ls 1 title '$metric'
gnuplot_commands
        close $GNUPLOT;
    }
}

sub graph_Multi_Metrics {
    my ($metrics, $xlabel, $ylabel, $title, $outfile, $formats) = @_;
    # pdf before png: the PNG is rendered from the PDF written on the pass before.
    $formats ||= ['eps', 'pdf', 'png'];
    my @plot_cmds;
    my $i = 1;

    foreach my $metric (@$metrics) {
        my $folder = "$working_folder/$metric";
        my $datafile = "$folder/$metric.dat";
        push @plot_cmds, "'$datafile' using 2:xticlabels(1) with linespoints ls $i title '$metric'";
        $i++;
    }

    foreach my $fmt (@$formats) {
        my $out = $outfile;
        $out =~ s/\.\w+$//; # remove extension if present
        my $stem = $out;
        $out = "$out.$fmt";
        if ($fmt eq 'png') {
            render_png("$stem.pdf", $out);
            next;
        }
        my $terminal;
        if ($fmt eq 'pdf') {
            $terminal = "set terminal pdf dashed size 17cm,10cm";
        } elsif ($fmt eq 'eps') {
            $terminal = "set terminal postscript eps color dashed size 17cm,10cm";
        } else {
            die "graph_Multi_Metrics: unsupported format '$fmt'\n";
        }

        open my $GNUPLOT, "|-", "gnuplot" or die "Cannot open gnuplot: $!";
        print $GNUPLOT <<"gnuplot_commands";
set title "$title"
set xlabel "$xlabel"
set xtics rotate by -30
set ylabel "$ylabel"
set key outside right
set key vertical
set key spacing 1.5
set key box
set grid
$terminal
set output "$out"
set style line 1 lc rgb 'red' lt 1 lw 2 pt 7 pi -1 ps 1.5
set style line 2 lc rgb 'blue' lt 1 lw 2 pt 5 pi -1 ps 1.5
set style line 3 lc rgb 'green' lt 1 lw 2 pt 9 pi -1 ps 1.5
plot @{[ join(", \\\n    ", @plot_cmds) ]}
gnuplot_commands
        close $GNUPLOT;
    }
}
