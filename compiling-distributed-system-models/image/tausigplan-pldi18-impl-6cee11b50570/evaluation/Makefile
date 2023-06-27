all: raft

raft_dir = ../examples/raft
multipaxos_dir = ../examples/multi-paxos

raft_files = $(raft_dir)/raft.cpp $(raft_dir)/raft.h
multipaxos_files = $(multipaxos_dir)/multi_paxos_system.cpp $(multipaxos_dir)/multi_paxos_system.h

extract_raft:
	cd $(raft_dir); ivy_to_cpp isolate=test target=class raft.ivy

extract_multipaxos:
	cd $(multipaxos_dir); ivy_to_cpp isolate=iso_impl target=class multi_paxos_system.ivy

extract: extract_raft extract_multipaxos

src/version.h:
	echo '#define VERSION "'1'"' > src/version.h

raft: src/server.cpp src/version.h
	mkdir -p build
	g++ -O3 -std=c++11 -w -I $(raft_dir)/ -o build/server $(raft_dir)/raft.cpp src/common.cpp src/kvstore.cpp src/server.cpp -lpthread

multipaxos: src/server.cpp src/version.h extract_multipaxos
	mkdir -p build
	g++ -O3 -std=c++11 -w -DMULTIPAXOS -I $(multipaxos_dir)/ -o build/server $(multipaxos_dir)/multi_paxos_system.cpp src/common.cpp src/kvstore.cpp src/server.cpp -lpthread

clean:
	rm -rf build/
	rm -rf $(raft_files) $(multipaxos_files)

.PHONY: clean multipaxos raft src/version.h all extract_raft extract_multipaxos extract
