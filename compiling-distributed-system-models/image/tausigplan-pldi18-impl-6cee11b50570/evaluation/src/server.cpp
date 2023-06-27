#include <iostream>
#include <sstream>
#include <map>
#include <memory>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <arpa/inet.h>
#include <netdb.h>

#include <sys/time.h>

#include "kvstore.h"
#include "common.h"
#include "server.h"

#include "version.h"

#ifdef MULTIPAXOS
#    include "multipaxos-consensus.h"
#elif MULTIPAXOS_TCP
#    include "multipaxos-consensus-tcp.h"
#else
#    include "raft-consensus.h"
#endif

void usage() {
  std::cout << "USAGE" << std::endl;
  std::cout << "server --node-id n" << std::endl;
  std::cout << std::endl;
  std::cout << "REQUIRED ARGUMENTS" << std::endl;
  std::cout << "    --node-id n          set the node id (required; must be 0, 1, or 2)" << std::endl;
  std::cout << "    --cluster CLUSTER    set the addresses of the cluster (required)" << std::endl
            << "                         eg, --cluster localhost:8000,localhost:8001,localhost:8002" << std::endl;
  std::cout << std::endl;
  std::cout << "OPTIONS" << std::endl;
  std::cout << "    --help               print this message and exit" << std::endl;
  std::cout << "    --log                print log messages during execution" << std::endl;
  std::cout << "    --client-port n      set the client port to listen on (default: 8000)" << std::endl;
  std::cout << "    --version            print version and exit" << std::endl;

}

void server::send_response(const kvstore::response& resp) {
  //log("server::send_response on server " + std::to_string(m_opt.nodeid) + ", preparing to send response to " + std::to_string(resp.client_id));
  if (m_connected_clients.count(resp.client_id) > 0) {
    std::string resp_str = resp.to_string();

    int fd = m_connected_clients[resp.client_id];
    char buf[4 + resp_str.length()];
    *(int*)buf = resp_str.length();
    resp_str.copy(buf + 4, resp_str.length());
    //log("server::send_response on server" + std::to_string(m_opt.nodeid) + ", sending response to " + std::to_string(resp.client_id));
    send(fd, buf, 4+ resp_str.length(), 0);
  }
}

void server::committed_callback(int ix, const std::string& request_string) {
  std::cout << "server::committed_callback: " << request_string << std::endl;
  std::lock_guard<std::mutex> guard(mut);
  kvstore::request req;
  kvstore::parse_request(request_string, req);

  kvstore::response resp;
  if (m_kvstore.execute_request(req, resp)) {
    send_response(resp);
  } else {
    log("ignoring old request: " + request_string);
  }
}

void server::setup() {
  struct addrinfo hints;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;


  struct addrinfo *ai, *p;;
  int rv = getaddrinfo(NULL, m_opt.client_port.c_str(), &hints, &ai);
  if (rv != 0) {
    log(std::string("getaddrinfo: ") + gai_strerror(rv));
    exit(1);
  }

  for(p = ai; p != NULL; p = p->ai_next) {
    m_accept_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (m_accept_fd < 0) {
      continue;
    }

    int yes = 1;
    setsockopt(m_accept_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    if (bind(m_accept_fd, p->ai_addr, p->ai_addrlen) < 0) {
      close(m_accept_fd);
      continue;
    }

    break;
  }

  if (p == NULL) {
    log("failed to bind");
    exit(1);
  }

  freeaddrinfo(ai);

  if (listen(m_accept_fd, 10) == -1) {
    perror("listen");
    exit(1);
  }

  m_all_fds.add(m_accept_fd);
}

void server::event_loop() {
  setup();

  log("entering event loop of server " + std::to_string(m_opt.nodeid));

  while (true) {
    fd_set read_fds = m_all_fds.the_fd_set;
    // log("selecting...");

    if (select(m_all_fds.max+1, &read_fds, NULL, NULL, NULL) == -1) {
      log("select error!");
      perror("select");
      continue;
    }
    // log("select success!");

    for(int i = 0; i <= m_all_fds.max; i++) {
      if (FD_ISSET(i, &read_fds)) {
        
        if (i == m_accept_fd) {
          log("new client connection");
          struct sockaddr_storage their_addr;

          socklen_t addr_size = sizeof their_addr;
          int newfd = accept(m_accept_fd, (struct sockaddr *)&their_addr, &addr_size);
          int nodelayvalue = 1;
          int result = setsockopt( newfd, IPPROTO_TCP, TCP_NODELAY, (void *)&nodelayvalue, sizeof(nodelayvalue));
          if (newfd == -1) {
            perror("accept");
            break;
          }
          if (result < 0) {
        	perror("setsockop nodelay");
        	break;
          }
          {
            std::lock_guard<std::mutex> guard(mut);
            m_all_fds.add(newfd);
          }
        } else {
          // log("client socket ready");
          char buf[4];
          int nbytes = recv(i, buf, sizeof buf, 0);
          if (nbytes <= 0) {
            {
              std::lock_guard<std::mutex> guard(mut);
              if (nbytes < 0) {
                perror("recv");
              }
              log("client closed connection" );
              close(i);
              m_all_fds.remove(i);
              auto it = m_connected_clients.begin();
              while (it != m_connected_clients.end()) {
                if (it->second == i) {
                  it = m_connected_clients.erase(it);
                } else {
                  ++it;
                }
              }
            }
          } else {
            //log("got header");
            int n = *(int*)buf;
            char body[n+1];
            nbytes = recv(i, body, n, 0);
            if (nbytes < n) {
              //log("body did not come all at once");
              exit(1);
            }
            //log("got body");
            body[n] = '\0';
            //std::cout << "received body " << body << std::endl;

            {
              std::lock_guard<std::mutex> guard(mut);
            int client_id = kvstore::parse_request_client_id(body);
            m_connected_clients[client_id] = i;
            //log("request received on server " + std::to_string(m_opt.nodeid) + " from client " + std::to_string(client_id));
            }
            
            bool rejected = m_consensus->replicate(std::string(body));

            if (rejected) {
              //log("rejected on node " + std::to_string(m_opt.nodeid));
              *(int*)buf = 9;
              send(i, buf, 4, 0);
              send(i, "NotLeader", 9, 0);
            }
            else {
              //log("request accepted on node " + std::to_string(m_opt.nodeid));
            }
          }
        }
      }
    }
  }
}

void arg_parse_error(const std::string& s) {
  std::cout << s << std::endl;
  std::cout << std::endl;
  usage();
  exit(1);
}

void parse_node(std::string s, std::pair<unsigned long, unsigned long>& p) {
  size_t idx = s.find(':');
  assert(idx != std::string::npos, "parse_node: Could not find ':'.");

  std::string addr = s.substr(0, idx);
  p.second = std::stoi(s.substr(idx+1));

  struct hostent *he = gethostbyname(addr.c_str());
  struct in_addr* si = (struct in_addr*) he->h_addr;
  p.first = si->s_addr;

  p.first = ntohl(p.first);
}

void parse_cluster(std::string s, cluster_t& cluster) {
  size_t idx = 0;
  size_t next;

  while ((next = s.find(',', idx)) != std::string::npos) {
    std::pair<unsigned long, unsigned long> p;
    parse_node(s.substr(idx, next - idx), p);

    cluster.push_back(p);

    idx = next;

    assert(s[idx] == ',', std::string("parse_cluster: Unexpected character '") + s[idx] + "'");

    idx++;
  }

  assert(idx < s.length(), "Unexpected end of string.");

  std::pair<unsigned long, unsigned long> p;
  parse_node(s.substr(idx), p);

  cluster.push_back(p);
}

void parse_args(options& opt, int argc, char** argv) {
  while (argc > 0) {
    if (*argv == std::string("--help")) {
      usage();
      exit(0);
    } else if (*argv == std::string("--version")) {
      std::cout << VERSION << std::endl;
      exit(0);
    } else if (*argv == std::string("--node-id")) {
      argc--;
      argv++;
      if (argc == 0) {
        arg_parse_error("--node-id requires an argument");
      }

      opt.nodeid = atoi(*argv);
    } else if (*argv == std::string("--client-port")) {
      argc--;
      argv++;
      if (argc == 0) {
        arg_parse_error("--client-port requires an argument");
      }

      opt.client_port = *argv;
    } else if (*argv == std::string("--cluster")) {
      argc--;
      argv++;
      if (argc == 0) {
        arg_parse_error("--cluster requires an argument");
      }

      std::string s(*argv);

      parse_cluster(s, opt.cluster);

    } else if (*argv == std::string("--log")) {
      opt.log = true;
    } else {
      arg_parse_error(std::string("Unknown option ") + *argv);
    }

    argc--;
    argv++;
  }

  if (opt.nodeid == -1) {
    arg_parse_error("--node-id is required");
  }

  if (opt.cluster.size() == 0) {
    arg_parse_error("--cluster is required");
  }
}

void print_cluster(const cluster_t& cluster) {
  std::cout << "{";
  bool started = false;
  for (auto x = cluster.begin(); x != cluster.end(); ++x) {
    if (started) {
      std::cout << ", ";
    }
    started = true;

    std::cout << x->first << ":" << x->second;
  }
  std::cout << "}";
}

void print_options(const options& opt) {
  std::cout << "Options:" << std::endl
            << "  client_port = " << opt.client_port << std::endl
            << "  log = " << opt.log << std::endl
            << "  nodeid = " << opt.nodeid << std::endl
            << "  cluster = ";
  print_cluster(opt.cluster);
  std::cout << std::endl;

}

int main(int argc, char** argv) {
  options opt;
  parse_args(opt, argc-1, argv+1);

  print_options(opt);

  struct timeval tv;

  gettimeofday(&tv, NULL);

  srand(tv.tv_usec);

  int sleeptime = rand() % 3000000;
  std::cout << "Process ID: " << getpid() << std::endl;
  std::cout << "Random sleep: " << sleeptime << std::endl;

  usleep(sleeptime);

  //try {
#if defined MULTIPAXOS || defined MULTIPAXOS_TCP
  std::cout << "Starting Multi-Paxos based server" << std::endl;
  multipaxos_consensus consensus(opt);
#else
  std::cout << "Starting Raft based server" << std::endl;
  raft_consensus consensus(opt);
#endif

  server server(opt, &consensus);

  server.event_loop();

  /*}
  catch (const std::exception & ex) {
    std::cout << "failure" << std::endl;
    std::cerr << ex.what() << std::endl;
    exit(EXIT_FAILURE);
  }*/
}
