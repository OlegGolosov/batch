#!/bin/bash

#SBATCH -J compress
#SBATCH -p long
#SBATCH -t 24:00:00

folder=$1
echo folder=$folder
cd $folder

date
echo archiving src folder
\time -f "%E" tar -cjf src.tgz src
chmod 600 src.tgz
rm -rf src

echo zipping dat files
date
for f in dat/dcmqgsm_*.dat;
do 
  gzip -f $f;
done
date

echo tarring dat_pure folder
\time -f "%E" tar -cf dat_pure.tar dat_pure

echo removing log folder
\time -f "%E" rm -fr log

echo removing dati_pure folder
\time -f "%E" rm -fr dat_pure

echo removing dat folder
\time -f "%E" rm -fr dat

date
echo finish!
