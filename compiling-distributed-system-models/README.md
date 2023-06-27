# PGo Benchmarks

This repository aggregates all the tools and data necessary to reproduce the results in the evaluation section of our ASPLOS 2023 paper.

Our artifact has two components. We provide the PGo compiler itself, which can compile MPCal specifications, and we also provide a method for reproducing our performance results from our ASPLOS 2023 paper. These files describe how to reproduce our performance results.

## Description

Our benchmark runner is a pre-compiled collection of JAR files runnable on Linux.
Its source code is provided in the `azbench/` folder.

The benchmark runner machines, which will be controlled by the benchmark runner, require that a large number of dependencies be installed.
This installation is automated via the script `image/provision.sh`, which is assumed to be run in the context of Ubuntu 20.04 with the current directory set to a copy of the `image/` directory.
This script lists full dependencies and build steps for all our systems and all the related work against which we evaluate, resulting in a directory containing compiled versions of all necessary artifacts.

### How to access

Clone the repository as shown below, recursing over submodules.
Some, but not all, dependencies are included as submodules.

```bash
$ git clone --recurse-submodules https://github.com/DistCompiler/pgo-artifact
```

### Hardware dependencies

Our original experiments were run on Microsoft Azure VMs.
We provide an automated workflow for recreating our setup, as long as you have a locally logged-in Microsoft Azure account with funds available.

Given that requiring Azure credits is not ideal, we also support two other modes of operation:
- Creating local VMs via Vagrant.
  In this way, it is possible to test basic functionality of our experimental setup without investing in a server cluster.
  We provide a complete set of scripts for deploying the required virtual machines locally and running our experiments in this way.

- To realistically recreate our measurements, we recommend using multiple machines and a real network.
  We do not provide automation support for doing this without Azure, but we instead document the required overrides to our experiment runner as well as how to run the installer scripts for our dependencies manually.

It may also be viable to re-use the Vagrant VMs we provide by placing them on different servers, but we have not investigated how to specifically do that.

### Software dependencies

On the machine that will be used to collect experimental results, our experiment runner's software dependencies are just a working installation of Java 11+.
To run our data processing, we additionally require Jupyter with ipykernel 6.13.0 or compatible, as well as pandas 1.3.5, matplotlib 3.5.1, and numpy 1.21.5.
For our Vagrant-based provisioning solution, Vagrant 2.2.16 or compatible is required to run the provided Vagrant files.
For our Azure-based provisioning solution, Azure CLI 2.41.0 or compatible is required to log into the Azure account that you will use with our provisioner.

All other dependencies must be installed on remote machines that our benchmark runner controls.
That list is very long as all our experiments together use a wide variety of runtimes and libraries.
Our provisioning script, in the context of Ubuntu 20.04, should be considered authoritative for versions and build steps.

## Installation

Our installation process has three variations, each of which has a tradeoff in terms of faithfulness to our original setup, ease of use, and financial investment.

In all cases, the included benchmark runner `./azurebench` will run all the experiments listed in `experiments.json` and deposit the results in `results/`.
The `--settling-delay 20` flag is needed to run Ivy-Raft correctly, as it is unstable on start-up.
Note that the `serverCount` key in the configuration JSON indicates how many servers an experiment will need, excluding one extra client machine.
Therefore, a given experiment will need `serverCount + 1` machines.

### Manage machines with Vagrant

This is the easiest solution to setup, as it launches all the required servers as VMs on the local machine with Vagrant.
Because all the servers run on a single machine, it is unlikely to produce useful results for distributed systems we evaluate.
We provide this mode as an easy way to check the integrity of our scripts and see that the experiments can be run at all.
To use this mode, first install Vagrant and VirtualBox in order to be able to manage VMs.
On Ubuntu, the following command should install appropriate versions of the required software:

```bash
$ apt install vagrant virtualbox
```

Then, build a VM using the following commands:

```bash
$ vagrant up
$ vagrant package # produces ./package.box
$ vagrant box add package.box --name azbench
```

This will take up to an hour unassisted, and will build a Vagrant ``box'' called `azbench`.
Once that is done, enter the `vagrant_fleet` subdirectory and run the command `vagrant up`.
This will launch all the VMs.
Do this on a powerful enough machine (at least 16GB of RAM; 8 CPU cores).
Note that our default setup will only launch 4 VMs, enough to run the default simple workload.
To re-run our full workload, you should edit `vagrant_fleet/Vagrantfile` and change the number `4` to `14`.

To allow the benchmark runner to find the VMs, rename the following configuration file to remove the `vagrant_` prefix:

```bash
$ cp vagrant_static_server_map.json static_server_map.json
```

Once this is done, the following command will run some simple experiments on those VMs:

```bash
$ ./azurebench --settling-delay 20 .
```

### Manage machines with Azure

Given sufficient funds, the most accurate method to reproduce our results is to run experiments on Microsoft Azure servers. 

To do this, install Azure CLI https://learn.microsoft.com/en-us/cli/azure/install-azure-cli, and log in using the account and tenant to which you intend to charge experiments.
Note down your tenant ID and your subscription ID.

Once this is done, launching the provisioning and experiment running process can be done with this command:
```bash
$ ./azurebench --settling-delay 20 --azure-subscription <subscription ID> --azure-tenant-id <tenant ID> .
```

Note that this will take a long time, possibly over an hour.
This is because the runner will be creating the VMs it needs and running the provisioning script on them.
Once a VM is created, it can be re-used.
Experiments that do not require VM provisioning only take minutes.

Note that Azure can be flaky.
If the process fails, try re-running it.
It might work the second time.
If in doubt, try shutting down the VMs the provisioner has created from the Azure UI.
Once that is done, the runner will start them back up with any zombie processes or file locks removed.

### Manage machines manually

To reproduce usable results without using Microsoft Azure, it is possible to hand-provision a set of servers and instruct our runner to use them.
Servers should have Ubuntu 20.04 installed, and should ideally be freshly re-imaged.
Our best advice for this is to study the provisioning process embedded in the other options and recreate it.
The general dependencies are the `image/` directory being in the remote user's home directory, and running `cd image && bash provision.sh` with the expectation that it will use `sudo`.
The benchmark runner `./azurebench` additionally expects `id_rsa.pub` to be added to `~/.ssh/authorized_hosts` on the remote machine for SSH purposes.
Note that this script is meant to run on a throwaway VM, and will make many changes using root privileges.

Once the machines are set up, use `vagrant_static_server_map.json` as a template to inform the benchmark runner of the usernames and IP addresses of the provisioned machines.
The resulting file should be named `static_server_map.json` for the runner to use it.
If the runner picks up this file, it will not try to run `image/provision.sh` itself, unlike when running on an Azure machine.
It will also not attempt to upload the contents of `image/` to each server, which it will otherwise do.

Once everything is set up, the following command will start some simple experiments:
```bash
$ ./azurebench --settling-delay 20 .
```

The template of machines with 8vCPUs and 32GB of RAM is an over-approximation, so it may be possible to run these experiments on less powerful machines.
We have not studied how much the available system resources may be decreased without impacting the experiments.

## Experiment workflow

Each experiment run by `./azurebench` will be recorded in a subfolder of `results/`.
This includes logs containing the outputs from all the SSH sessions used.
If a run was successful, `results.txt` will exist and contain the results as human-readable text.
If a run was unsuccessful or interrupted, `results.txt` will not exist and the other files will indicate what happened.

Which experiments occur is controlled by the `experiments.json` file, which lists configuration values and shell commands to execute when running experiments.
All script dependencies for experiments exist in the `image/` folder, so a script invoked by the name `foo` can be inspected by reading `image/foo`.

On checkout, the initial contents of `experiments.json` is a copy of `experiments_simple.json`.
This is a small workload designed to ensure all kinds of experiment can be performed.
The true set of experiments from the paper, including our machine-dependent tuning values, is in `experiments_full.json`.
Copying that over to `experiments.json` will cause all experiments from the paper to be run in full.

Note that for results describing peak throughput (Figures 3 and 5), our configuration lists the values at which we measured peak throughput on our machines.
Results are known to vary even across different Azure VMs of the same type.
To recreate meaningful results, we recommend splitting the experiments into two passes: varying only the number of client threads, then varying workload and cluster size.
This initial set of experiments is in `experiments_tuning.json`.
The number of client threads that causes the highest throughput should then be edited into key `threadCount` of the template `experiments_tuned.json` which will gather data that depends on peak throughput.

## Evaluation and expected results

To run our full set of experiments, run the following commands:

```bash
$ cp experiments_full.json experiments.json
$ ./azurebench --settling-delay 20 . # specify Azure IDs if needed
```

Once complete, `graphs-python.ipynb` can be used to parse data from `results/ and recreate each of the performance graphs from this paper.
Which cell corresponds to which figure is annotated in the comments.

Our existing data set is included under the name `results_paper/`.
To test that the notebook is set up properly, you can copy that data over to `results/` and see the same graphs from the paper regenerated.

We expect a recreation of our results to preserve the relationships between artifact performance numbers, but not the numbers themselves.
