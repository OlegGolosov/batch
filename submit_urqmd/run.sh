#!/bin/bash

#hades
#pbeam=1.95

#cbm
#pbeam=3.3
#pbeam=4
#pbeam=5.36
#pbeam=6
#pbeam=8
#pbeam=10
#pbeam=12

#ags:
#pbeam=2.78
#pbeam=4.85
#pbeam=6.87
#pbeam=8.89

#mpd:
#pbeam=9.81

#star
#pbeam=30.65
pbeam=69.55
#pbeam=111.13

#na49/61:
#pbeam=13
#pbeam=30
#pbeam=40
#pbeam=158

eos=0
events_per_file=2000 # set double to get desired amount after removing empty events
jobRange=0-0
#partition=long
partition=main
#partition=debug
system=auau
#system=pbpb

[ "$partition" == "debug" ] && time=0:20:00
[ "$partition" == "main" ] && time=8:00:00
[ "$partition" == "long" ] && time=1-00:00:00

[ "$events_per_file" == "" ] && echo "Empty events_per_file. Provide it as a 2nd argument" && exit
[ "$jobRange" == "" ] && echo "Empty jobRange in the file sequence. Provide it as a 3rd argument" && exit
[ "$partition" == "" ] && echo "Empty partition. Provide it as a 4th argument (options: main / debug)" && exit

source_dir_orig=/lustre/nyx/cbm/users/ogolosov/mc/macros/submit_urqmd

user=$USER  # test it

root_config=/cvmfs/fairroot.gsi.de/fairsoft/jun19p1/bin/thisroot.sh
unigen_path=/lustre/nyx/cbm/users/ogolosov/soft/UniGen
outdir="/lustre/nyx/cbm/users/$user/mc/generators/urqmd/v3.4/"$system"/pbeam"$pbeam"agev_eos"$eos"/mbias"
outdir_root="$outdir/root/"
outdir_dat="$outdir/dat/"
source_dir="$outdir/src/"
log_dir="$outdir/log/"

mkdir -p "$outdir"
mkdir -p $source_dir
mkdir -p $outdir_root
mkdir -p $outdir_dat

mkdir -p $log_dir

rsync -av $source_dir_orig/ $source_dir/

cp $source_dir/src/inputfile.template $source_dir/src/inputfile

[ "$system" == "pbpb" ] && targetA=208 && targetZ=82 && projA=208 && projZ=82
[ "$system" == "auau" ] && targetA=197 && targetZ=79 && projA=197 && projZ=79

sed -i -- "s~targetA~$targetA~g" $source_dir/src/inputfile
sed -i -- "s~targetZ~$targetZ~g" $source_dir/src/inputfile
sed -i -- "s~projA~$projA~g" $source_dir/src/inputfile
sed -i -- "s~projZ~$projZ~g" $source_dir/src/inputfile
sed -i -- "s~EOS~$eos~g" $source_dir/src/inputfile
sed -i -- "s~nEvents~$events_per_file~g" $source_dir/src/inputfile
sed -i -- "s~plab~$pbeam~g" $source_dir/src/inputfile

currentDir=`pwd`
echo "current dir:" $currentDir

run_gen="$source_dir/run_gen.sh"
#seed=$(expr $SECONDS / 2)
seed=0

sbatch -J uqmd_$pbeam --mem=8G -o $log_dir/%a_%A.o -e $log_dir/%a_%A.e -p $partition -t $time -a $jobRange -D $outdir --export=root_config=$root_config,unigen_path=$unigen_path,outdir_dat=$outdir_dat,outdir_root=$outdir_root,log_dir=$log_dir,source_dir=$source_dir,seed=$seed,pbeam=$pbeam,events_per_file=$events_per_file $run_gen


echo "========================================================"
echo "Output will be written to:"
echo ""
echo "source code: $source_dir"
echo "Temporary dir (do not forget to clean up after the jobs are finished) $log_dir"
echo "root files: $outdir_root"
echo ""
echo "dat files: $outdir_dat"
echo "========================================================"


