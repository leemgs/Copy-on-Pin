#!/bin/bash

function start-server-command {
  PORT=800${1}
  echo -e "./build/server" "--log" "--node-id \"$1\"" \
                           "--client-port \"$PORT\"" \
                           "--cluster \"$2\" \
                           ; bash -i"

}

nodes=${1-3}

cluster="localhost:4990"
for i in `seq 1 $(( nodes - 1 ))`;
do
    cluster=$cluster,localhost:$((4990 + $i))
done

tmux  new-session -d -s 'server'
tmux  split-window -h "$(start-server-command 0 $cluster)"
for i in `seq 1 $(( nodes - 1 ))`;
do
    tmux split-window -v -p $((100 * ($nodes - $i) / ($nodes - $i + 1))) "$(start-server-command $i $cluster)"
done
tmux select-pane -L
exec tmux attach
