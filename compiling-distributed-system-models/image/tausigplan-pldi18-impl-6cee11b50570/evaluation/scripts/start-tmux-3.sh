#!/bin/bash

function start-server-command {
  PORT=800${1}
  echo -e "./build/server" "--log" "--node-id \"$1\"" \
                           "--client-port \"$PORT\"" \
                           "--cluster \"localhost:4990,localhost:4991,localhost:4992\""

}

tmux new-session -d -s 'server'
tmux split-window -h "$(start-server-command 0)"
tmux split-window -v -p 66 "$(start-server-command 1)"
tmux split-window -v -p 50 "$(start-server-command 2)"
tmux select-pane -L
exec tmux attach
