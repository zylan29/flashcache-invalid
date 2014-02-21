cd flashcache-invalid
make KERNEL_TREE=$1
make KERNEL_TREE=$1 install
cd ..
cd flashcache
make KERNEL_TREE=$1
make KERNEL_TREE=$1 install
cd ..
