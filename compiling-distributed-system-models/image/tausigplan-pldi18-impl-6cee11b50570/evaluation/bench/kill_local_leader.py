import sys
import os
import signal
import subprocess
import argparse
import time
import vard
import etcd

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--service', default='vard', choices=['etcd', 'vard'])
    parser.add_argument('--host', required=True)    
    parser.add_argument('--port', type=int, required=True)
    parser.add_argument('--sleep', type=int, required=True)
    args = parser.parse_args()

    Client = vard.Client
    if args.service == 'etcd':
        Client = etcd.Client

    try:
        Client.find_leader([(args.host, args.port)])
    except Client.NoLeader:
        print "No local leader to kill"
        sys.exit(1)

    print "time is %s" % time.time()
    print "Going to kill the leader in %s seconds" % args.sleep

    time.sleep(args.sleep)

    lsof_results = subprocess.check_output(['lsof', '-iTCP:%d' % args.port,
                                            '-n', '-P', '-sTCP:LISTEN', '-Fp'])

    pid = None
    for field in lsof_results.split('\n'):
        if field.startswith('p'):
            pid = int(field[1:])
    if not pid:
        print "Could not find leader's process ID"
        sys.exit(1)


    print "Killing leader at time %s" % time.time()
    os.kill(pid, signal.SIGKILL)
    
        
if __name__ == '__main__':
    main()
