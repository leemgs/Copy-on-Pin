#include <chrono>
#include <ctime>
#include "server.h"
#include "common.h"
#include <raft.h>

#ifndef _RAFT_CONSENSUS_H
#define _RAFT_CONSENSUS_H

std::string msg_kind_to_string(raft::msgkind k) {
  switch (k) {
  case raft::rqvote:
    return "rqvote";
  case raft::rqvotenolog:
    return "rqvotenolog";
  case raft::vtcandidate:
    return "vtcandidate";
  case raft::appentry:
    return "appentry";
  case raft::enappeneded:
    return "enappeneded";
  case raft::keepalive:
    return "keepalive";
  case raft::nack:
    return "nack";
  default:
	perror("unhandled enum value");
	exit(1);
  }
}

std::string msg_to_string(const raft::msg& m) {
  return msg_kind_to_string(m.m_kind) + " " +
    "ix = " + std::to_string(m.m_ix) + " " +
    "term = " + std::to_string(m.m_term) + " " +
    "node = " + std::to_string(m.m_node) + " " +
    "cand = " + std::to_string(m.m_cand) + " " +
    "value = " + m.m_val + " " +
    "logt = " + std::to_string(m.m_logt) + " " +
    "prevlogt = " + std::to_string(m.m_prevlogt) + " " +
	"isrecover = " + std::to_string(m.m_isrecover);
}


//class cluster_tcp_config : public tcp_config {
//public:
//  cluster_t cluster;
//
//  cluster_tcp_config (const cluster_t& cluster) :
//    cluster(cluster)
//  {}
//
//  virtual void get(int id, unsigned long &inetaddr, unsigned long &inetport) {
//    assert(0 <= id && id < cluster.size(), "id out of bounds");
//
//    auto host = cluster[id];
//
//    inetaddr = host.first;
//    inetport = host.second;
//  }
//  virtual ~cluster_tcp_config() {}
//};

class cluster_udp_config : public udp_config {
public:
  cluster_t cluster;

  cluster_udp_config (const cluster_t& cluster) :
    cluster(cluster)
  {}

  virtual void get(int id, unsigned long &inetaddr, unsigned long &inetport) {
    assert(0 <= id && id < cluster.size(), "id out of bounds");

    auto host = cluster[id];

    inetaddr = host.first;
    inetport = host.second;
  }
  virtual ~cluster_udp_config() {}
};

class raft_consensus : public consensus, public raft {
public:
  raft_consensus(const options& opt) :
    consensus(opt),
    raft(opt.cluster.size(), opt.nodeid)
  {
	  the_udp_config = new cluster_udp_config(opt.cluster);
	  __unlock(); /* Seems that the raft() constructor leaves the lock locked, is this on purpose? */
  }

  virtual bool replicate(std::string s) {
    __lock();

    bool rejected = ext__system__req_append_new_entry(s);
    __unlock();

    return rejected;
  }

  virtual void imp__shim__send_debug(const msg& m) {
      //auto tm = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	  //std::cout << std::ctime(&tm) << " " << "SENDING " << msg_to_string(m) << std::endl;
  }

  virtual void imp__shim__recv_debug(const raft::msg& m) {
      //auto tm = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	  //std::cout << std::ctime(&tm) << " " << "RECEIVING " << msg_to_string(m) << std::endl;
  }

//  virtual void imp__shim__debug_print_handle_vote_cand(const node& self, const node& cand, int node_t, int t, bool not_leader, bool ok) override {
//      auto tm = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
//      std::cout << std::ctime(&tm) << " HANDLE VOTE" << self << " " << cand << " " << node_t << " " << t << " " << not_leader << " " << ok << std::endl;
//  }

  void imp__system__commited_at_ix(int ix, std::string v) {
      callback->committed(ix, v);
  }

};

#endif  // _RAFT_CONSENSUS_H
