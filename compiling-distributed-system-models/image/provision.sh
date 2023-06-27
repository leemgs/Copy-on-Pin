set -e -x

if [ ! -e ~/.apt-updated ]; then
  sudo apt-get update
  touch ~/.apt-updated
fi

sudo apt-get install -y gnupg2 wget curl default-jdk maven scons python3 virtualenv cmake pkg-config python3-ply python3-pygraphviz python3-tk tix libssl-dev libreadline-dev valgrind
java -version
mvn -version

bash ./ensure_sbt.sh
sbt -version

./amm hello.sc

pushd ironkv-client
sbt publishM2
popd

pushd vard-client
sbt publishM2
popd

pushd YCSB
mvn -Psource-run -pl site.ycsb:ironkv-binding -am package -DskipTests
mvn -Psource-run -pl site.ycsb:vard-binding -am package -DskipTests
popd

./etcd-v3.5.4-linux-amd64/etcd --version

if [ ! -e ~/.dotnet-installed ]; then
  wget https://packages.microsoft.com/config/ubuntu/20.04/packages-microsoft-prod.deb -O packages-microsoft-prod.deb
  sudo dpkg -i packages-microsoft-prod.deb
  rm packages-microsoft-prod.deb

  sudo apt-get update
  sudo apt-get install -y apt-transport-https
  sudo apt-get update
  sudo apt-get install -y dotnet-sdk-6.0

  touch ~/.dotnet-installed
fi

pushd Ironclad/ironfleet
scons --no-verify --dafny-path=../../dafny
popd

sudo apt-get install -y make gcc opam

if [ ! -e ~/.verdi-raft-built ]; then
  sudo apt-get install -y opam ocaml ocaml-native-compilers camlp4-extra

  echo 'no' | opam init --disable-sandboxing

  opam repo add coq-extra-dev https://coq.inria.fr/opam/extra-dev
  opam pin -y coq-cheerios https://github.com/uwplse/cheerios.git#9c7f66e57b91f706d70afa8ed99d64ed98ab367d
  opam install -y coq-struct-tact coq-cheerios coq-verdi

  # need this specific version because the newer one, 2.0.0, removes a deprecated type that verdi-runtime uses
  opam install -y yojson.1.7.0

  opam repo add distributedcomponents-dev http://opam-dev.distributedcomponents.net
  opam install -y verdi-runtime cheerios-runtime ocamlbuild

  eval $(opam env)

  pushd verdi-raft
  ./configure && make quick && make vard
  popd

  touch ~/.verdi-raft-built
fi

export PATH=$PATH:$(pwd)/go1.18.2.linux-amd64/go/bin

pushd pgo/systems/raftkvs
make build
popd

pushd pgo/systems/raftres
make build
popd

pushd pgo/systems/shopcart
make build
popd

pushd pgo/systems/pbkvs
make build
popd

pushd go-ycsb
make
popd

if [ ! -e ~/.redis-installed ]; then
  sudo apt-get install -y redis-server
  sudo systemctl stop redis-server.service
  sudo systemctl disable redis-server.service

  touch ~/.redis_installed
fi

pushd roshi/roshi-server
go build -buildvcs=false
popd

pushd roshi/roshi-walker
go build -buildvcs=false
popd

pushd roshiapp
make build
popd

if [ ! -e ~/py_venv ]; then
  virtualenv --clear ~/py_venv
fi
source ~/py_venv/bin/activate

#pushd ivy
#  if [ ! -e ~/.ivy-z3-built ]; then
#    pushd submodules/z3
#      python scripts/mk_make.py --python
#      pushd build
#        make
#        make uninstall
#        make install
#      popd
#    popd
#    touch ~/.ivy-z3-built
#  fi
#  if [ ! -e ~/.ivy-deps-built ]; then
#    python build_submodules.py
#    touch ~/.ivy-deps-built
#  fi
#  python setup.py develop
#popd

pushd tausigplan-pldi18-impl-6cee11b50570
  pushd evaluation
    make raft
  popd
popd
