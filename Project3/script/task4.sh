set -euo pipefail

TARGET_FILE="/home/jaron/Repo-for-Advanced-Computer-Systems/Project3/testfile/fio_testfile"

for jobs in 32, 64; do
  for run in 1 2 3; do
    fio --name=NJ${jobs}_run${run} \
        --filename="${TARGET_FILE}" --direct=1 \
        --ioengine=io_uring --iodepth=1 --numjobs=${jobs} \
        --rw=randread --bs=4k \
        --time_based --runtime=30s --ramp_time=5s \
        --group_reporting \
        --output=NJ_${jobs}_run${run}.json --output-format=json
  done
done
