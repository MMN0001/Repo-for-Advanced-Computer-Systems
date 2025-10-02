set -euo pipefail

TARGET_FILE="/home/jaron/Repo-for-Advanced-Computer-Systems/Project3/testfile/fio_testfile"

for ratio in 100 70 50 0; do
  fio --name=Mix_${ratio}R_${100-${ratio}}W \
      --filename="${TARGET_FILE}" --direct=1 \
      --rw=randrw --rwmixread=${ratio} \
      --bs=4k --iodepth=1 --numjobs=1 \
      --size=10G --time_based --runtime=30s --ramp_time=5s --group_reporting \
      --output=Mix_${ratio}R_${100-${ratio}}W.json --output-format=json
done
