# Paper Artifacts

This repository contains the software artifacts for the paper
"Copy-on-Pin: The Missing Piece for Correct Copy-on-Write". They include patches for three custom Linux kernels based on Linux 5.18, three micro-benchmarks, a SPECrate2017 config file, some helper scripts, and a copy of the O_DIRECT+fork test cases.

The artifacts are prepared to be run on an x86-64 machine with root and access, at least 12 cores and at least 32 GiB of memory. The artifacts assume that Fedora 36 is installed. Running these artifacts on other machines or under other Linux distributions might require adaptions to the installation script and benchmark run scripts.

All commands are expected to be run by a Linux user with sudo (root) permissions.

## Installing Custom Linux Kernels

The __install.sh__ script in the *kernels* directory will automatically install required packages using the DNF package manager, download Linux 5.18, and compile and install the three custom Linux kernels. Note that this script will also install required packages for the benchmarks.

```
$ cd kernels
$ sudo ./install.sh
```

If everything went as expected, the three custom Linux kernels should be visible as possible boot options. __grubby__ can list the installed kernels.

```
$ sudo grubby --info=ALL
```

There should be entries referencing kernels containing the string __5.18.0-relcop__, __5.18.0-nocop__ and __5.18.0-precop__.

## Switching Between Custom Linux Kernels

To run the experiments on the different custom Linux kernels, reboots are required. To minimize users errors, all benchmarks store results into directories named after the currently booted Linux kernel.

While selecting a custom Linux kernel to boot in the boot manager (GRUB) is one approach, the suggestion is to configure the next kernel to boot from Linux directly.

After installing the custom Linux kernels, observe all installed
kernels via the __grubby__ command:

```
$ sudo grubby --info=ALL
```

Select a kernel by noting the "index=" value and supply it to the
__grubby__ command, then reboot. Assuming “index=1”:

```
sudo grubby --set-default-index=1
sudo shutdown −r now
```

The relationship between the index assignment and the custom Linux kernels might be as follows:

| Index | Kernel Name   | Approach |
|-------|---------------|----------|
| 0     | 5.18.0-relcop | RelCOP   |
| 1     | 5.18.0-nocop  | NoCOP    |
| 2     | 5.18.0-precop | PreCOP   |

After the reboot, verify the correct kernel was booted using the __uname__ command. Assuming "index=1" corresponds to 5.18.0-nocop:

```
$ uname −r
5.18.0−nocop
```

## SWAP Configuration

The modified STREAM benchmark requires configuration of a proper disk-based swap backend (with at least 2 GiB) in order to use the swapcache as expected; otherwise, the benchmark results might differ. This is usually the case after a Fedora 36 installation, however, __zram__ might be used instead if no swap space was configured.

If */proc/swaps* has no entry, or lists a zram entry, manual SWAP configuration is required. As one example, the following entry would not require manual configuration.

```
$ cat /proc/swaps
Filename                                Type ...
/dev/dm-2                               partition 
```

To disable zram, a file has to be created and the machine has to be rebooted:
```
$ sudo touch /etc/systemd/zram-generator.conf
$ sudo shutdown -r now
```

Then, another SWAP backend can be configured, for example, residing in a file on the root filesystem in a 2 GiB */swapfile*:
```
$ sudo touch /swapfile
$ sudo chmod 600 /swapfile
$ sudo chattr +C /swapfile
$ sudo dd if=/dev/zero of=/swapfile bs=1M count=2048
$ sudo mkswap /swapfile
$ sudo swapon /swapfile
```
Note that the *chattr* command might fail on some file-systems, which is expected as it might only be required on *btrfs*.

Observe */proc/swaps* again, to make sure the change was successful:
```
$ cat /proc/swaps
Filename                                Type ...
/swapfile                               file ...
```

Then, make the swap configuration persistent and reboot:
```
  $ echo "/swapfile swap swap defaults 0 0" | sudo tee -a /etc/fstab
  $ sudo shutdown -r now
```

Observe */proc/swaps* again, to make sure the change is persistent.

## vm-scalability Benchmark

Compile and run the benchmark via the __run-vm-scalability.sh__ script. This will execute the *anon-cow-seq* and *anon-cow-rand* benchmarks, once with THP enabled and once with THP disabled.

```
$ cd benchmarks
$ sudo ./run-vm-scalability.sh
```

The results will be stored into a sub-directory in
*benchmarks/results/vm-scalability* that corresponds to the current Linux kernel, e.g., into *benchmarks/results/vm-scalability/5.18.0-relcop/*.

Note that if anything goes wrong while compiling or running the benchmark, the relevant sub-directory in *benchmarks/results/vm-scalability* has to be deleted in order to re-run the benchmark.

The following CSV files are created by the benchmark in that sub-directory, whereby each file contains the results of each each individual benchmark
iteration:
* *thp/case-anon-cow-seq-[2,4,6,8,10,12].csv*: Results from running the
  *anon-cow-seq* benchmark with 2,4,6,8,10,12 tasks and THP enabled.
* *thp/case-anon-cow-rand-[2,4,6,8,10,12].csv*: Results from running the
  *anon-cow-rand* benchmark with 2,4,6,8,10,12 tasks and THP enabled.
* *nothp/case-anon-cow-seq-[2,4,6,8,10,12].csv*: Results from running the
  *anon-cow-seq* benchmark with 2,4,6,8,10,12 tasks and THP disabled.
* *nothp/case-anon-cow-rand-[2,4,6,8,10,12].csv*: Results from running the
  *anon-cow-rand* benchmark with 2,4,6,8,10,12 tasks and THP disabled.

### Evaluation

A helper script is provided to compute the average GB/s across all benchmark iterations as given in the paper. The __eval-vm-scalability.sh__ script will process the results for each kernel.

```
$ cd benchmarks
$ sudo ./eval-vm-scalability.sh
```

The output of this script is a set of files for each processed kernel in the *benchmarks/results* directory. We'll use __5.18.0-nocop__ as an example:
* *5.18.0-nocop-case-anon-cow-seq-thp.csv*: Average results from running the *anon-cow-seq* benchmark with 2,4,6,8,10,12 tasks and THP enabled.
* *5.18.0-nocop-case-anon-cow-rand-thp.csv*: Average results from running the*anon-cow-rand* benchmark with 2,4,6,8,10,12 tasks and THP enabled.
* *5.18.0-nocop-case-anon-cow-seq-nothp.csv*: Average results from running the *anon-cow-seq* benchmark with 2,4,6,8,10,12 tasks and THP disabled.
* 5.18.0-nocop-case-anon-cow-rand-nothp.csv: Average results from running the *anon-cow-rand* benchmark with 2,4,6,8,10,12 tasks and THP disabled.

The speedup of __5.18.0-relcop__ in relation to __5.18.0-precop__ as given in the paper has to be calculated manually.

## Modified STREAM Benchmark

Compile and run the benchmark via the __run-stream.sh__ script. This will execute the modified STREAM benchmark for each action with THP disabled.

```
$ cd benchmarks
$ sudo ./run-stream.sh
```

The results will be written into a sub-directory in
*benchmarks/results/stream* that corresponds to the current Linux kernel, e.g., into *benchmarks/results/stream/5.18.0-relcop/*.

Note that if anything goes wrong while compiling or running the benchmark, the relevant sub-directory in *benchmarks/results/stream* has to be deleted in order to re-run the benchmark.

The following CSV files are created by the benchmark in that sub-directory, whereby each file contains the results of each each individual benchmark
iteration:
* *fork.csv* Results from running the modified STREAM benchmark with the "fork" action before each iteration.
* *mprotect+ksm.csv* Results from running the modified STREAM benchmark with the "mprotect+ksm" action before each iteration.
* *mprotect.csv* Results from running the modified STREAM benchmark with the "mprotect" action before each iteration.
* *swap.csv* Results from running the modified STREAM benchmark with the "swap" action before each iteration.
* *fork+ksm.csv* Results from running the modified STREAM benchmark with the "fork+ksm" action before each iteration.
* *swap+fork.csv* Results from running the modified STREAM benchmark with the "swap+fork" action before each iteration.

### Evaluation

A helper script is provided to compute the average memory bandwidth and number of page copies per second across all benchmark iterations as given in the paper. The __eval-stream.sh__ script will process the results for each kernel.

```
$ cd benchmarks
$ sudo ./eval-stream.sh
```

The output of this script is a single CSV file for each processed kernel in the *benchmarks/results* directory. With __5.18.0-nocop__ as an example, the output file is *5.18.0-nocop-stream.csv*.

## write-fault-duration Benchmark

Compile and run the benchmark via the __run-write-fault-duration.sh__ script. This will execute the simple write-fault-duration benchmark with THP disabled.

```
$ cd benchmarks
$ sudo ./run-write-fault-duration.sh
```

The results will be written into a single *results.csv* in a sub-directory in *benchmarks/results/write-fault-duration* that corresponds to the current Linux kernel, e.g., *benchmarks/results/write-fault-duration/5.18.0-relcop/results.csv*.

Note that if anything goes wrong while compiling or running the benchmark, the relevant sub-directory in *benchmarks/results/write-fault-duration* has to be deleted in order to re-run the benchmark.

### Evaluation

A helper script is provided to compute the average duration of a single memory access (dominated by the write fault overhead), across all benchmark iterations. The __eval-write-fault-duration.sh__ script will process the results for each kernel.

```
$ cd benchmarks
$ sudo ./write-fault-duration.sh
```

The output of this script is a single CSV file *benchmarks/results/write-fault-duration.csv*.

## SPECrate2017 Benchmark

Please follow the official SPEC CPU 2017 installation instructions. Assume SPEC CPU 2017 is installed under */usr/cpu2017*. If the installation directory differs, adjust the path accordingly in the commands below and in the __run-spec.sh__ script. 

Note that the expected outcome of this experiment is that most results are practically identically and that there is no real difference when running the benchmarks under the different custom Linux kernels: common workloads have no observable performance impact. Other benchmarks that similarly measure performance of common workloads might be used as a replacement.

### Preparations

Copy the config file from *benchmarks/specrate2017/cop-paper.cfg* to */usr/cpu2017/config/*:

```
$ cp benchmarks/specrate2017/cop-paper.cfg /usr/cpu2017/config/
```

It might be necessary to update SPEC CPU 2017 to the newest version:

```
$ sudo sh -c 'source /usr/cpu2017/shrc; runcpu --update'
```

### Running the Benchmark

To run the benchmark once with THP enabled and once with THP disabled, use the __run-spec.sh__ script in the *benchmarks* directory; this will execute a reportable intrate and fprate benchmark run with 12 copies and 3 iterations. The number of iterations can be adjusted in the script:

```
$ cd benchmarks
$ sudo ./run-spec.sh
```

The __run-spec.sh__ scrip will automatically move the results into a sub-directory in *benchmarks/results/specrate2017* that corresponds to the current Linux kernel, e.g., *benchmarks/results/specrate2017/5.18.0-relcop*. Results with THP enabled can be found in the "thp" sub-directory, results with THP disabled can be found in the "nothp" sub-directory.

Note that if anything goes wrong while compiling or running the benchmark, the relevant kernel-specific sub-directory in benchmarks/results/specrate2017 has to be deleted in order to re-run the benchmark.

### Evaluation

The benchmark results ("base run time" and "base rate" for each individual benchmark) are contained in the *CPU2017.001.fprate.csv* and *CPU2017.001.intrate.csv* files inside the "thp" and "nothp" directories in the kernel-specific sub-directorys under *benchmarks/results/specrate2017*. Please refer to the SPEC CPU 2017 documentation for details.

## O_DIRECT+Fork Test Cases
The test cases mentioned in the paper in Section 3.2 are contained in the *o_direct_fork_tests* sub-directory.
