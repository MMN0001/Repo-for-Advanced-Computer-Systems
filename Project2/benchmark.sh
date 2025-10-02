for sz in 8K 16K 32K 256K 512K 1M 2M 16M 32M 128M 512M 1G; do
  ./bench --size $sz --stride 64 --iters 50000000 >> bench_result.txt
done
