
fallocate -l 10G ~/fio_testfile

fio --name=4K_RandRead --filename=~/fio_testfile --direct=1 \
    --rw=randread --bs=4k --iodepth=1 --numjobs=1 \
    --size=10G --time_based --runtime=30s --group_reporting \
    --output=4K_RandRead.log

fio --name=4K_RandWrite --filename=~/fio_testfile --direct=1 \
    --rw=randwrite --bs=4k --iodepth=1 --numjobs=1 \
    --size=10G --time_based --runtime=30s --group_reporting \
    --output=4K_RandWrite.log

fio --name=128K_SeqRead --filename=~/fio_testfile --direct=1 \
    --rw=read --bs=128k --iodepth=1 --numjobs=1 \
    --size=10G --time_based --runtime=30s --group_reporting \
    --output=128K_SeqRead.log

fio --name=128K_SeqWrite --filename=~/fio_testfile --direct=1 \
    --rw=write --bs=128k --iodepth=1 --numjobs=1 \
    --size=10G --time_based --runtime=30s --group_reporting \
    --output=128K_SeqWrite.log
