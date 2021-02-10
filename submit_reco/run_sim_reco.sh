#!/bin/bash

taskId=${SLURM_ARRAY_TASK_ID}
 
inputFile=${inputFile}_${taskId}.root
job_out_dir=${out_dir}/${taskId}
mkdir -p ${job_out_dir}
cd ${job_out_dir}
ln -s ${VMCWORKDIR}/macro/run/.rootrc .

elapsed=$SECONDS

if [ ${run_transport} == 1 ] && [ ! -e transport.log.gz ]; then
  echo VMCWORKDIR: ${VMCWORKDIR}
  cp -v ../macro/run_transport.C .
  plutoFileNumber=$((${taskId}-${offset}))
  unzip ${plutoPath}.zip *_${plutoFileNumber}.root 
  plutoFile=$(ls ${PWD}/*_${plutoFileNumber}.root)
  if [ -z ${plutoFile} ]; then
    plutoFileNumber=$(printf "%05d" ${plutoFileNumber})
    unzip ${plutoPath}.zip *_${plutoFileNumber}.root 
    plutoFile=$(ls ${PWD}/*_${plutoFileNumber}.root)
  fi
  sed -i -- "s~PLUTOFILE~\"${plutoFile}\"~g" run_transport.C
  echo inputFile=${inputFile}
  echo plutoFile=${plutoFile}
  echo Execute: ${job_out_dir}/run_transport.C
  root -b -q "run_transport.C (${nEvents}, \"${base_setup}\", \"${taskId}\", \"${inputFile}\")" &> transport.log
  #&> /dev/null 
  gzip -f transport.log
  rm run_transport.C
  rm ${plutoFile}
fi

if [ ${run_digi} == 1 ] && [ ! -e digi.log.gz ]; then 
  echo VMCWORKDIR: ${VMCWORKDIR}
  cp -v ../macro/run_digi.C .
  sed -i -- "s~TASKID~${taskId}~g" run_digi.C
  echo Execute: ${job_out_dir}/run_digi.C
  root -b -q "${job_out_dir}/run_digi.C (${nEvents}, \"${taskId}\", 1.e7, 1.e4, kTRUE)" &> digi.log
  gzip -f digi.log
  rm run_digi.C
fi

if [ ${run_reco} == 1 ] && [ ! -e reco.log.gz ]; then  
  echo VMCWORKDIR: ${VMCWORKDIR}
  cp -v ../macro/run_reco_event.C .
  sed -i -- "s~TASKID~${taskId}~g" run_reco_event.C
  echo Execute: ${job_out_dir}/run_reco_event.C
  root -b -q "${job_out_dir}/run_reco_event.C (${nEvents}, \"${taskId}\", \"${base_setup}\")" &> reco.log 
  gzip -f reco.log
  rm run_reco_event.C
fi

if [ ${run_treemaker} == 1 ] && [ -e reco.log.gz ]; then 
  echo VMCWORKDIR: ${VMCWORKDIR}
  cp -v ../macro/run_treemaker.C .
  sed -i -- "s~TASKID~${taskId}~g" run_treemaker.C
  echo Execute: ${job_out_dir}/run_treemaker.C
  root -b -q "${job_out_dir}/run_treemaker.C (${nEvents}, \"${taskId}\", \"${base_setup}\")" &> tree.log
  gzip -f tree.log
  mv ${taskId}.tree.root ${tree_dir}
  rm run_treemaker.C
fi

if [ ${run_at_maker} == 1 ] && [ -e reco.log.gz ]; then
  . ${cbmroot_with_AT_config} 
  echo VMCWORKDIR: ${VMCWORKDIR}
  cp -v ../macro/run_analysis_tree_maker.C .
  sed -i -- "s~TASKID~${taskId}~g" run_analysis_tree_maker.C
  echo Execute: ${job_out_dir}/run_analysis_tree_maker.C
  root -b -q "${job_out_dir}/run_analysis_tree_maker.C (${nEvents}, \"${taskId}\", \"${base_setup}\")" &> atree.log
  gzip -f atree.log
  mv ${taskId}.analysistree.root ${atree_dir}
  rm run_analysis_tree_maker.C
fi
rm .rootrc

[ ${delete_sim_files} = 1 ] && [ $(( $taskId % 100 )) -ne 0 ] && rm -r ${job_out_dir} 

elapsed=$(expr $SECONDS - $elapsed)
echo "Done!"
echo Elapsed time: $(expr $elapsed / 60) minutes
