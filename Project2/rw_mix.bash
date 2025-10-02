outfile="rw_mix.csv"
echo "size,stride,iters,mix,avg_ns,BW_GBps" > $outfile

for mix in 100R 100W 70/30 50/50; do
  ./rw_mix --size 256M --stride 64 --iters 200000000 --mix $mix >> $outfile
done

