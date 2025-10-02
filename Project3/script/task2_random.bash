set -euo pipefail

TARGET_FILE="/home/jaron/Repo-for-Advanced-Computer-Systems/Project3/testfile/fio_testfile"

for bs in 4k 8k 16k 32k 64k 128k 256k; do
  fio --name=RandRead_bs_${bs} --filename="${TARGET_FILE}" --direct=1 \
      --rw=randread --bs=${bs} --iodepth=1 --numjobs=1 \
      --size=10G --time_based --runtime=30s --ramp_time=5s --group_reporting \
      --output=RandRead_${bs}.json --output-format=json
done

for bs in 4k 8k 16k 32k 64k 128k 256k; do
  fio --name=RandWrite_bs_${bs} --filename=~/fio_testfile --direct=1 \
      --rw=randwrite --bs=${bs} --iodepth=1 --numjobs=1 \
      --size=10G --time_based --runtime=30s --ramp_time=5s --group_reporting \
      --output=RandWrite_${bs}.json --output-format=json
done