#!/bin/bash

function getJsonVal () {
  val=$(python -c "import json;print(json.dumps(json.load(open('${config}'))$1))";)
  eval echo ${val}
} # e.x. 'out_path=$(getJsonVal "['transport']['output']['path']")'

#function checkJsonKey () {
#  python -c "import json,sys;print ('$1' in json.load(open('${config}')))";
#} # returns True if key exists, False if not, e.x. 'run_transport=$(checkJsonKey "transport")'

function read_step_info () {
  run=$(getJsonVal "['accessory']['${step}']['run']")
  if [ ${run} == true ]; then
    out_dir=$(getJsonVal "['${step}']['output']['path']")
    [[ ${out_dir} != */ ]] && out_dir=$(dirname ${out_dir})
    src_dir=${out_dir}/macro
    macro=$(getJsonVal "['accessory']['${step}']['macro']")
    macro_name=$(basename ${macro})
  fi
}

submit_script=${0}
config=${1}
batch=$(getJsonVal "['accessory']['batch']")
jobScript=$(getJsonVal "['accessory']['jobScript']")
export cbmRoot=$(getJsonVal "['accessory']['cbmRoot']")
source ${cbmRoot}

steps="transport digitization reconstruction AT"
for step in ${steps}; do
  read_step_info
  if [ ${run} == true ]; then
    mkdir -pv ${src_dir}
    cp -v ${macro} ${src_dir}
    cp -v ${config} ${src_dir}
    cp -v ${submit_script} ${src_dir}
    cp -v ${jobScript} ${src_dir}
  fi
done

export -f getJsonVal
export -f read_step_info

export config_name=$(basename ${config})
export config=${src_dir}/${config_name}
export nEvents=$(getJsonVal "['accessory']['nEvents']")
jobRange=$(getJsonVal "['accessory']['jobRange']")

if [ ${batch} == true ];then
  account=$(getJsonVal "['accessory']['account']")
  ram=$(getJsonVal "['accessory']['ram']")
  partition=$(getJsonVal "['accessory']['partition']")
  time=$(getJsonVal "['accessory']['time']")
  jobName=$(getJsonVal "['accessory']['jobName']")
  logDir=$(getJsonVal "['accessory']['logDir']")
  mkdir -pv ${logDir}
  sbatch -A ${account} --mem=${ram} -p ${partition} -t ${time} -J ${jobName}\
    -a ${jobRange} -o ${logDir}/%a_%A.log --export=ALL -- ${jobScript}
else
  export SLURM_ARRAY_TASK_ID=${jobRange}
  ${jobScript} &> ${logDir}/${jobRange}.log &
fi
