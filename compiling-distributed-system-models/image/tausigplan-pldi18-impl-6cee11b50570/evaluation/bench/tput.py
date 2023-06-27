import argparse
import Queue
import random
import threading
import time
import vard
import etcd

threads = Queue.Queue()
done = threading.Event()
keys = None
put_prob = None

class Worker(threading.Thread):
    def __init__(self, thread_id, client, my_turn, exit):
        threading.Thread.__init__(self)

        self.thread_id = thread_id
        self.client = client
        self.my_turn = my_turn
        self.exit = exit

        self.reqs = []
        self.gets = []
        self.puts = []


    def run(self):
        i = 0
        while True:
            self.my_turn.wait()
            self.my_turn.clear()
            if self.exit.is_set():
                break
            i += 1

            key = str(random.randint(0, keys))
            if random.random() < put_prob:
                start = time.time()
                self.client.put('key' + key, str(i))
                end = time.time()
                if not done.is_set():
                    self.puts.append(end-start)
                    self.reqs.append((self.thread_id, end, end-start))
            else:
                start = time.time()
                self.client.get('key' + key)
                end = time.time()
                if not done.is_set():
                    self.gets.append(end-start)
                    self.reqs.append((self.thread_id, end, end-start))

            threads.put(self)


def cluster(addrs):
    ret = []
    for addr in addrs.split(','):
        (host, _, port) = addr.partition(':')
        ret.append((host, int(port)))
    return ret

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--threads', default=50, type=int)
    parser.add_argument('--rate', default=100, type=int)
    parser.add_argument('--time', default=30, type=int)
    parser.add_argument('--service', default='vard', choices=['etcd', 'vard'])
    parser.add_argument('--cluster', type=cluster, required=True)
    parser.add_argument('--keys', default=100, type=int)
    parser.add_argument('--put-percentage', default=50, type=int)
    args = parser.parse_args()

    global keys
    global put_prob
    keys = args.keys
    put_prob = args.put_percentage / 100.0

    Client = vard.Client
    if args.service == 'etcd':
        Client = etcd.Client

    host, port = Client.find_leader(args.cluster)

    for thread_id in range(args.threads):
        c = Client(host, port)
        ev = threading.Event()
        ex = threading.Event()
        worker = Worker(thread_id, c, ev, ex)
        worker.start()
        threads.put(worker)

    n_requests = args.rate * args.time
    delay = 1.0 / args.rate
    print "delay = ", delay
    start = time.time()
    i = 0
    while True:
        worker = threads.get_nowait()
        worker.my_turn.set()

        i += 1
        if i == n_requests:
            break

        time.sleep(delay)

    done.set()
    end = time.time()

    reqs = []
    gets = []
    puts = []
    for i in range(args.threads):
        print threads.qsize()
        worker = threads.get()
        worker.exit.set()
        worker.my_turn.set()  # wake up thread to allow it to exit

        reqs.extend(worker.reqs)
        gets.extend(worker.gets)
        puts.extend(worker.puts)

    print 'Requests:'
    for (tid, ts, latency) in reqs:
        print 'REQUEST: THREAD %s TIME %s LATENCY %s' % (tid, ts, latency)
    print 'Total time: %f' % (end - start)
    print 'Throughput: %f reqs/s' % (len(reqs) / (end - start))
    print '%f gets, avg = %f' % (len(gets), sum(gets)/len(gets))
    print '%f puts, avg = %f' % (len(puts), sum(puts)/len(puts))

if __name__ == '__main__':
    main()
