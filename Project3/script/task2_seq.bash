set -euo pipefail

TARGET_FILE="/home/jaron/Repo-for-Advanced-Computer-Systems/Project3/testfile/fio_testfile"

for bs in 4k 8k 16k 32k 64k 128k 256k; do
  fio --name=SeqRead_bs_${bs} --filename="${TARGET_FILE}" --direct=1 \
      --rw=read --bs=${bs} --iodepth=1 --numjobs=1 \
      --size=10G --time_based --runtime=30s --ramp_time=5s --group_reporting \
      --output=SeqRead_${bs}.json --output-format=json
done

for bs in 4k 8k 16k 32k 64k 128k 256k; do
  fio --name=SeqWrite_bs_${bs} --filename="${TARGET_FILE}" --direct=1 \
      --rw=write --bs=${bs} --iodepth=1 --numjobs=1 \
      --size=10G --time_based --runtime=30s --ramp_time=5s --group_reporting \
      --output=SeqWrite_${bs}.json --output-format=json
done