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

# Affiche l'aide
sub print_help {
	print <<"END_HELP";
Usage: $0 [options]

Options:
  -ip <file>      Input parameters file
  -vf <file>      Video file
  -od <dir>       Output directory
  -xl <label>     X axis label
  -h, --help      Show this help message

Example:
  $0 -ip ../ipFiles/decoder.dat -vf ../videos/mobile_qcif.avi -od ../sets/set1 -xl GC-PN
END_HELP
	exit 0;
}

# Paramètres
my $rute            = $ENV{PWD};
my ($ipPath, $videoFile, $outputDirectory, $xLabel);
GetOptions(
    'ip=s' => \$ipPath,
    'vf=s' => \$videoFile,
    'od=s' => \$outputDirectory,
    'xl=s' => \$xLabel,
) or die "Usage: $0 -ip <file> -vf <file> -od <dir> -xl <label>\n";

# Nettoi les anciens résultats
unlink glob "$rute/*.dat";
unlink glob "$rute/*.tr";
unlink glob "$rute/*.nam";

# Répertoires de sortie
my $working_folder = "$outputDirectory/graphs";
rmtree($working_folder) if -d $working_folder;

# Création des répertoires pour les métriques
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

# Stockage des métriques en mémoire
my %metrics_data;   # $metrics_data{$metric_name}{$simId} = [ valeurs ]
my %metrics_unit; # $metrics_unit{$metric_name} = $unit

# Main
open my $fh, '<', $ipPath or die "Cannot open $ipPath: $!";
setGraphs($fh, $videoFile, $outputDirectory, $xLabel);
close $fh;

# setGraphs : collecte, agrégation, écriture et génération des graphes
sub setGraphs {
    my ($fh, $videoFile, $outputDir, $xLabel) = @_;
    my @simid_order;

    # Lecture et collecte des données
    while (my $line = <$fh>) {
        chomp $line;
        $line =~ s/#.*//;
        $line =~ s/^\s+|\s+$//g;
        next unless $line;

        my @fields  = split /\s+/, $line;
        my $simId   = $fields[0] // '';
        my $Encoder = $fields[1] // '';
        next unless $Encoder eq 'MMPEG';
        push @simid_order, $simId;

        # Construction des chemins
        my $videoName = fileparse($videoFile, qr/\.[^.]*/);
        my $stPath    = "$outputDir/${videoName}-$simId";
        my $rtPath    = "$stPath/sim";
        my $stName    = "$stPath/st-packet.dat";
        my $rcvFile   = $fields[14] // '';
        my $rtName    = "$rtPath/" . fileparse($rcvFile);
        my $mtName    = "$rtPath/mt.dat";
        my $trName    = "$stPath/st-packet.tr";
        my $stFrame   = "$stPath/st-frame.dat";

        my $maxPrioNb = get_maxPrioNb($stName);

		print "Processing simulation ID: $simId\n";

        # Calcul des métriques
        calculate_Jitter($stName, $rtName, $simId);
        calculate_Packet_Loss_Ratio_by_Type($mtName, $simId);
        calculate_Packet_Loss_Ratio_by_Priority($maxPrioNb, $mtName, $simId);
        calculate_Throughput($rtName, $simId);
        calculate_PSNR($mtName, $simId);
        calculate_SSIM($mtName, $simId);
        calculate_Consumption($trName, $simId);
        calculate_Compression_Ratio($stFrame, $simId);
        calculate_Total_Packets_Sent($stName, $simId);
    }

    # Écriture agrégée des fichiers .dat
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
                print $out "$sim\t$mean\t$std\n";
            } else {
                print $out "$sim\n";
            }
        }
        close $out;

        # Récupère l’unité pour le graphe
        my $unit = $metrics_unit{$metric} // '';

        # Génération des graphes
        graph_Metric($metric, $xLabel, $unit, ['eps', 'pdf']);
    }

	# Multi-plot
	graph_Multi_Metrics(
        ['Packet Loss Ratio by Type-M', 'Packet Loss Ratio by Type-D'],
        $xLabel,
        'Packet Loss Ratio (%)',
        'EvalVSN - Packet Loss Ratio by Type-M vs Type-D',
        "$working_folder/Packet Loss Ratio by Type-M/Packet Loss Ratio by Type-M vs Type-D",
        ['eps', 'pdf']
    );
    graph_Multi_Metrics(
        ['PSNR', 'Ref. PSNR'],
        $xLabel,
        'PSNR (dB)',
        'EvalVSN - PSNR vs Ref. PSNR',
        "$working_folder/PSNR/PSNR_vs_Ref_PSNR",
        ['eps', 'pdf']
    );
	graph_Multi_Metrics(
        ['SSIM', 'Ref. SSIM'],
        $xLabel,
        'SSIM',
        'EvalVSN - SSIM vs Ref. SSIM',
        "$working_folder/SSIM/SSIM_vs_Ref_SSIM",
        ['eps', 'pdf']
    );
}

# Fonction pour obtenir le maxPrioNb à partir d'un fichier st-packet.dat
sub get_maxPrioNb {
    my ($filename) = @_;
    open my $fh, '<', $filename or die "Cannot open $filename: $!";
    my $maxPrioNb = 0;
    while (my $line = <$fh>) {
        next if $line =~ /^\s*#/; # Sauter les commentaires
        chomp $line;
        my @fields = split /\t/, $line;
        my $prioNb = $fields[5] // 0; # 6e colonne (index 5)
        $maxPrioNb = $prioNb if $prioNb > $maxPrioNb;
    }
    close $fh;
    return $maxPrioNb;
}

# Enregistre une valeur pour une métrique et un simId
sub record_Metric {
    my ($metric, $unit, $value, $simId) = @_;
    return unless defined $value;
    $metrics_unit{$metric} = $unit if defined $unit;
    push @{ $metrics_data{$metric}{$simId} }, $value;
}

# Statistiques
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

# Fonctions de calcul modifiées pour appeler record_Metric
sub calculate_Jitter {
    my ($senderFile, $receiverFile, $simId) = @_;

    # Lecture des paquets envoyés
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

    # Lecture des paquets reçus
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

    # Calcul des délais et jitter
    my (@delays, @jitters);
    my $prev_recv_time;
    for my $id (sort { $recv_packets{$a} <=> $recv_packets{$b} } keys %recv_packets) {
        next unless exists $sent_packets{$id};
        my $delay = $recv_packets{$id} - $sent_packets{$id};
        push @delays, $delay;
        if (defined $prev_recv_time) {
            push @jitters, abs($recv_packets{$id} - $prev_recv_time);
        }
        $prev_recv_time = $recv_packets{$id};
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

    # Initialisation des variables
	my %pkts_loss_ratio_by_type;	
    
	open my $fh, '<', $mtFile or die "Cannot open $mtFile: $!";
    my $header = <$fh>;
    chomp $header;
    $header =~ s/^\s*#//; # retire le # éventuel
    my @columns = split /\s+/, $header;

    # Chercher les index des colonnes pktLossM et pktLossD
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
        last; # une seule ligne de données attendue
    }
    close $fh;

    # affichage
    print "Packet Loss M: $pkts_loss_ratio_by_type{'M'} (%)\n";
    print "Packet Loss D: $pkts_loss_ratio_by_type{'D'} (%)\n";

	# Enregistrement des métriques
    record_Metric('Packet Loss Ratio by Type-M', '(%)', $pkts_loss_ratio_by_type{'M'}, $simId);
    record_Metric('Packet Loss Ratio by Type-D', '(%)', $pkts_loss_ratio_by_type{'D'}, $simId);
}

sub calculate_Packet_Loss_Ratio_by_Priority {
    my ($priorityNb, $mtFile, $simId) = @_;

	# Initialisation des variables
	my %pkts_loss_ratio_by_priority;

    open my $fh, '<', $mtFile or die "Cannot open $mtFile: $!";
	my $header = <$fh>;
	chomp $header;
	$header =~ s/^\s*#//;
	my @columns = split /\s+/, $header;

	# Chercher les index des colonnes pktlossM1..$priorityNb et pktlossD1..$priorityNb
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
		last; # une seule ligne de données attendue
	}
	close $fh;

	# affichage
	print "Packet Loss Ratio by Priority:\n";
	for my $p (1..$priorityNb) {
		print "pktlossM$p: $pkts_loss_ratio_by_priority{\"M$p\"} (%)\n";
		print "pktlossD$p: $pkts_loss_ratio_by_priority{\"D$p\"} (%)\n";
	}

	# Enregistrement des métriques
    for my $key (sort keys %pkts_loss_ratio_by_priority) {
        record_Metric("Packet Loss Ratio by Priority-$key", '(%)',
                      $pkts_loss_ratio_by_priority{$key}, $simId);
    }
}

sub calculate_Throughput {
    my ($receiverFile, $simId) = @_;

	# Initialisation des variables
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

	# Enregistrement des métriques
    record_Metric('Throughput', '(Kbits/s)', $throughput, $simId);
}

sub calculate_PSNR {
    my ($mtFile, $simId) = @_;

	# Initialisation des variables
	my %psnr;
    
	open my $fh, '<', $mtFile or die "Cannot open $mtFile: $!";
	my $header = <$fh>;
	chomp $header;
	$header =~ s/^\s*#//; # retire le # éventuel
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
		last; # une seule ligne de données attendue
	}
	close $fh;

	print "PSNR: $psnr{'psnr'} (dB)\n";
	print "Ref. PSNR: $psnr{'refPSNR'} (dB)\n";

	# Enregistrement des métriques
    record_Metric('PSNR', '(dB)', $psnr{'psnr'}, $simId);
    record_Metric('Ref. PSNR', '(dB)', $psnr{'refPSNR'}, $simId);
}

sub calculate_SSIM {
    my ($mtFile, $simId) = @_;

	# Initialisation des variables
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
		last; # une seule ligne de données attendue
	}
	close $fh;

	print "SSIM: $ssim{'ssim'}\n";
	print "Ref. SSIM: $ssim{'refSSIM'}\n";

	# Enregistrement des métriques
    record_Metric('SSIM', '', $ssim{'ssim'}, $simId);
    record_Metric('Ref. SSIM', '', $ssim{'refSSIM'}, $simId);
}

sub calculate_Consumption {
    my ($trPath, $simId) = @_;
    
	# --- Lecture du .tr et collecte des énergies résiduelles ---
    open my $fh, '<', $trPath
      or die "Cannot open trace file '$trPath': $!";

    my %res_energy;
    my $initial_energy = 0;
    while (my $line = <$fh>) {
        next unless $line =~ /^N\s/;           # seules les lignes débutant par "N"
        my @fields = split /\s+/, $line;
        my ($node, $energy) = ($fields[4], $fields[6]);
        # on garde l'énergie maximale comme énergie initiale
        $initial_energy = $energy if $energy > $initial_energy;
        $res_energy{$node} = $energy;
    }
    close $fh;

    # --- Calcul de la consommation par nœud ---
    my $total_energy = 0;
    my @cons_per_node;
    for my $node (keys %res_energy) {
        my $cons = $initial_energy - $res_energy{$node};
        $total_energy += $cons;
        push @cons_per_node, $cons;
    }
    my $n_nodes    = scalar @cons_per_node;
    my $energy_avg = $n_nodes ? $total_energy / $n_nodes : 0;

    # --- Calcul du pourcentage d'énergie résiduelle minimale (lifetime indicator) ---
    my $min_residual = defined $initial_energy && $initial_energy
                     ? min(values %res_energy)
                     : 0;
    my $lifetime_avg = $initial_energy
                     ? ($min_residual / $initial_energy) * 100
                     : 0;

    # --- Affichage des métriques calculées ---
    print "total energy consumed: $total_energy (J)\n";
    print "average per node energy: $energy_avg (J)\n";
    print "minimum residual (lifetime): $lifetime_avg (%)\n";

	# --- Enregistrement des métriques ---
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

    # Cherche l'index de la colonne compRatio
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
        next if $line =~ /^\s*#/; # Sauter les commentaires
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

# Graphes
sub graph_Metric {
    my ($metric, $xlabel, $unit, $formats) = @_;
    $formats ||= ['eps', 'pdf'];
    my $folder = "$working_folder/$metric";
    my $datafile = "$folder/$metric.dat";
    my $title = "EvalVSN - $metric";
    my $ylabel = $metric;
    $ylabel =~ s/_/ /g;
    $ylabel .= " $unit" if $unit;

    foreach my $fmt (@$formats) {
        my $outfile = "$folder/$metric.$fmt";
        my $terminal;
        if ($fmt eq 'pdf') {
            $terminal = "set terminal pdf dashed size 17cm,10cm";
        } elsif ($fmt eq 'eps') {
            $terminal = "set terminal postscript eps color dashed size 17cm,10cm";
        } else {
            $terminal = "set terminal png size 1700,1000"; # fallback for other formats
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
    $formats ||= ['eps', 'pdf'];
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
        $out = "$out.$fmt";
        my $terminal;
        if ($fmt eq 'pdf') {
            $terminal = "set terminal pdf dashed size 17cm,10cm";
        } elsif ($fmt eq 'eps') {
            $terminal = "set terminal postscript eps color dashed size 17cm,10cm";
        } else {
            $terminal = "set terminal png size 1700,1000"; # fallback for other formats
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
