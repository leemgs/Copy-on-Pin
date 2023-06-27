# Installing IVY

Before you begin make sure the basic packages (python, g++, make) are installed.

Z3
--
Download z3 4.6.0, a theorem prover which is the basis for Ivy:
    https://github.com/Z3Prover/z3/releases
For example if you are on ubuntu 16 download z3-4.6.0 from:
    ~$ wget https://github.com/Z3Prover/z3/releases/download/z3-4.6.0/z3-4.6.0-x64-ubuntu-16.04.zip
Open the zip file (in our example we will open this at the home directory: ~)
    ~$ unzip z3-4.6.0-x64-ubuntu-16.04.zip
Add environment variables you need to ~/.bashrc file (or any other file loaded in your shell):
    export Z3HOME=$HOME/z3-4.6.0-x64-ubuntu-16.04
    export PATH=$PATH:$Z3HOME/bin
    export PYTHONPATH=$PYTHONPATH:$Z3HOME/bin/python:$Z3HOME/bin
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$Z3HOME/bin
    export C_INCLUDE_PATH=$C_INCLUDE:$Z3HOME/include
    export CPLUS_INCLUDE_PATH=$C_INCLUDE:$Z3HOME/include
    export LIBRARY_PATH=$LIBRARY_PATH:$Z3HOME/bin


Other packages:
--------------
Packages mentioned at Ivy website (http://microsoft.github.io/ivy/install.html)
    $ sudo apt-get install python-ply python-pygraphviz
    $ sudo apt install python-tk tix
    $ pip install tarjan

Ivy
---
Now we will download the latest ivy repository from git. Go back to the home directory and type:
    ~$ git clone https://github.com/Microsoft/ivy.git ivy

Then, checkout the following commit (we do not guarantee that working with the latest master version will work):
    ~/ivy $ git checkout ab0ade9073759fde83a3560d92714086e90fd83f

Now compile and install:
     ~/ivy $ python setup.py build
     ~/ivy $ sudo python setup.py develop
     # develop is like install but links back to ~/ivy, so if you check out a different ivy version it will be run automatically.

# Extracting verified C++ code

For this guide we will assume you cloned our paper repository to ~/pldi18-impl, which means this file is located in ~/pldi18-impl/evaluation/README.md

## RAFT

Now do:

    ~$ cd ~/pldi18-impl/examples/raft
    ~/pldi18-impl/examples/raft$ ivy_to_cpp isolate=test target=class raft.ivy

That will generate a raft.cpp and raft.h file which is the compilation of raft.ivy to C++.
These files should be pretty similar to raft.cpp and raft.h from directory ~/pldi18-impl/evaluation/extracted/raft
The copies are already included in that directory for your convenience.

Notice ivy_to_cpp is the tool which compiles an ivy file to C++, in addition you could run the tool which checks the correctness of all the invariants in file raft.ivy:

    ~/pldi18-impl/examples/raft$ ivy_check raft.ivy

## Multi-Paxos

    ~$ cd ~/pldi18-impl/examples/multi-paxos
    ~/pldi18-impl/examples/multi-paxos$ ivy_to_cpp isolate=iso_impl target=class multi_paxos_system.ivy

That will generate a multi_paxos_system.cpp and multi_paxos_system.h file which is the compilation of multi_paxos_system.ivy to C++.

As for Raft, to check multi_paxos_system.ivy:

    ~/pldi18-impl/examples/multi-paxos$ ivy_check multi_paxos_system.ivy

In the case of Multi-Paxos, the specification and proof are separated in 4 files: nodes.ivy, network_shim.ivy, multi_paxos_protocol.ivy, and multi_paxos_system.ivy.
The command given above checks all of them.

# Evaluation


To run the systems locally, first build using either `make raft` or `make multi-paxos`.
Then, start the cluster of servers. For your convenience, we have provided a script,
`./scripts/start-tmux.sh` that will start a `tmux` session with 4 windows:
one each running a node of a 3-server cluster, and one extra
terminal for interacting with the cluster as a client.

If you want to set up the cluster manually, you can run three copies of the `./build/server`
executable with the following options (a complete list of options is given by `./build/server --help`):

    ./build/server --log --node-id 0 --client-port 8000 --cluster localhost:4990,localhost:4991,localhost:4992
    ./build/server --log --node-id 1 --client-port 8001 --cluster localhost:4990,localhost:4991,localhost:4992
    ./build/server --log --node-id 2 --client-port 8002 --cluster localhost:4990,localhost:4991,localhost:4992

These are exactly the commands run by `start-tmux.sh`. Each server process listens for client connections on
the given client port, and talks to the servers at the given cluster addresses and ports. The `--node-id`
option tells a server which element of cluster list contains its own address and port.

Now you can interact with the cluster by sending it commands. For example, you can make simple queries
by running `python2` in the bench/ directory and using the provided API:

    from vard import Client
    
    cluster = [("localhost", 8000), ("localhost", 8001), ("localhost", 8002)]

    leader_host, leader_port = Client.find_leader(cluster)

    c = Client(leader_host, leader_port)

    c.put("x", "7")
    c.put("y", "10")
    c.get("x")        # returns "7"

You can also perform a local benchmarking run using the script `./bench/bench.py` (note that you must start the servers before running this script). If you first ran some simple queries,
either close the existing python session or open a new terminal and run the following command. It may also be interesting
to interact with the cluster manually via the python API during or after benchmarks are running.

    python2 ./bench/bench.py --cluster localhost:8000,localhost:8001,localhost:8002 --requests 10 --threads 1 --keys 10 --put-percentage 50

The `--cluster` option specifies the addresses and *client* ports of the servers in the cluster.
The rest of the options specify a benchmark consisting of 10 randomly generated requests,
running at most one request in parallel, over a key space with 10 keys, where on average 50% of requests are puts.

Our paper reports measurements taken with 500 keys, 50 threads, and 10000 requests.


