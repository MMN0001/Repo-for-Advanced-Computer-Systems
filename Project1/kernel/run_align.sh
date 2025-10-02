# alignment + no tail
./kernel_simd   --kernel saxpy --dtype f32 --M 131072  --stride 1 --align aligned    --repeats 10 --csv saxpy_f32_L2.csv
./kernel_scalar --kernel saxpy --dtype f32 --M 131072  --stride 1 --align aligned    --repeats 10 --csv saxpy_f32_L2.csv
./kernel_simd   --kernel saxpy --dtype f32 --M 4194304  --stride 1 --align aligned    --repeats 10 --csv saxpy_f32_L2.csv
./kernel_scalar --kernel saxpy --dtype f32 --M 4194304  --stride 1 --align aligned    --repeats 10 --csv saxpy_f32_L2.csv
# alignment + tail
./kernel_simd   --kernel saxpy --dtype f32 --M 131075  --stride 1 --align aligned    --repeats 10 --csv saxpy_f32_L2.csv
./kernel_scalar --kernel saxpy --dtype f32 --M 131075  --stride 1 --align aligned    --repeats 10 --csv saxpy_f32_L2.csv
./kernel_simd   --kernel saxpy --dtype f32 --M 4194307  --stride 1 --align aligned    --repeats 10 --csv saxpy_f32_L2.csv
./kernel_scalar --kernel saxpy --dtype f32 --M 4194307  --stride 1 --align aligned    --repeats 10 --csv saxpy_f32_L2.csv
# misalignment + no tail
./kernel_simd   --kernel saxpy --dtype f32 --M 131072  --stride 1 --align misaligned --misalign_bytes 13 --repeats 10 --csv saxpy_f32_L2.csv
./kernel_scalar --kernel saxpy --dtype f32 --M 131072  --stride 1 --align misaligned --misalign_bytes 13 --repeats 10 --csv saxpy_f32_L2.csv
./kernel_simd   --kernel saxpy --dtype f32 --M 4194304  --stride 1 --align misaligned --misalign_bytes 13 --repeats 10 --csv saxpy_f32_L2.csv
./kernel_scalar --kernel saxpy --dtype f32 --M 4194304  --stride 1 --align misaligned --misalign_bytes 13 --repeats 10 --csv saxpy_f32_L2.csv
# misalignment + tail
./kernel_simd   --kernel saxpy --dtype f32 --M 131075  --stride 1 --align misaligned --misalign_bytes 13 --repeats 10 --csv saxpy_f32_L2.csv
./kernel_scalar --kernel saxpy --dtype f32 --M 131075  --stride 1 --align misaligned --misalign_bytes 13 --repeats 10 --csv saxpy_f32_L2.csv
./kernel_simd   --kernel saxpy --dtype f32 --M 4194307  --stride 1 --align misaligned --misalign_bytes 13 --repeats 10 --csv saxpy_f32_L2.csv
./kernel_scalar --kernel saxpy --dtype f32 --M 4194307  --stride 1 --align misaligned --misalign_bytes 13 --repeats 10 --csv saxpy_f32_L2.csv
