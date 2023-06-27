#!/bin/bash

if [ "$EUID" -ne 0 ]
  then echo "Please run as root"
  exit
fi

# Install required packages
echo "Installing dependencies"
dnf install -y gcc patch make wget git bc tar openssl-devel openssl flex bison elfutils-libelf-devel zstd numactl numactl-devel libnsl gcc-gfortran g++ || { echo 'installing packages failed' ; exit 1; }

# Download Linux v5.18
if [ ! -f "linux-5.18.tar.gz" ]; then
	echo "Downloading Linux 5.18"
	wget https://cdn.kernel.org/pub/linux/kernel/v5.x/linux-5.18.tar.gz || { echo 'download failed' ; exit 1; }
fi

# Compile and install the individual custom kernels
KERNELS="relcop nocop precop"
for K in $KERNELS; do
	echo "Processing kernel $K"

	# extract and copy all patches over
	tar xzf linux-5.18.tar.gz > /dev/null
	cp $K/*.patch linux-5.18 > /dev/null

	# apply all patches
	pushd linux-5.18
	for F in *.patch; do
		patch -p1 -i $F > /dev/null
	done

	# compile and install the kernels
	make -j20  > /dev/null 2>&1 || { echo 'compilation failed' ; exit 1; }
	make modules_install > /dev/null 2>&1 || { echo 'module installation failed' ; exit 1; }
	make install > /dev/null || { echo 'installation failed' ; exit 1; }
	popd

	# cleanup
	rm -rf linux-5.18
done
