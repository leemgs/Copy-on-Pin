#include "server.h"
#include "common.h"
//#include "assert.h"
#include <multi_paxos_system.h>
#include <chrono>
#include <thread>

#ifndef _MULTIPAXOS_CONSENSUS_H
#define _MULTIPAXOS_CONSENSUS_H

class cluster_tcp_config : public tcp_config {
public:
  cluster_t cluster;

  cluster_tcp_config (const cluster_t& cluster) :
    cluster(cluster)
  {}

  virtual void get(int id, unsigned long &inetaddr, unsigned long &inetport) override {
    assert(0 <= id && id < cluster.size(), "id out of bounds");

    auto host = cluster[id];

    inetaddr = host.first;
    inetport = host.second;
  }
  virtual ~cluster_tcp_config() {}
};


class multipaxos_consensus : public consensus, public multi_paxos_system {
protected:
  int m_applied_index;
  int m_max_index; // max commit ever seen
  bool do_log;
  std::thread timed_thread;

public:
  multipaxos_consensus(const options& opt) :
    consensus(opt),
    multi_paxos_system(opt.cluster.size(), opt.nodeid), // number of nodes, id of localhost.
    m_applied_index(-1),
    m_max_index(-1),
    do_log(false),
    timed_thread(&multipaxos_consensus::activate_logging, this)
  {
    set_tcp_config(new cluster_tcp_config(opt.cluster));
    timed_thread.detach();
    __unlock();
  }

  virtual bool replicate(__strlit s) override {
    // std::cout << "replicate(" << s << ")" << std::endl;

    __lock();
    // std::cout << "in replicate: after __lock(): " << std::endl;
    bool leader = ext__system__server__propose(s);
    __unlock();

    if (do_log) {
      std::cout << "propose returned " << leader << std::endl;
    }
    return !leader;
  }

  virtual void ext__system__server__decide(int ix, __strlit v) override {
    if (do_log) {
      std::cout << "ext__system__server__decide(" << ix <<") called!" << std::endl;
    }

    m_max_index = max(m_max_index, ix);
    if (do_log) {
      std::cout << "ext__system__server__decide: m_max_index is " << m_max_index << std::endl;
      std::cout << "ext__system__server__decide: m_applied_index is " << m_applied_index << std::endl;
    }

    __strlit d;
    while (m_applied_index < m_max_index) {
      if (do_log) {
        std::cout << "in ext__system__server__decide: callig server__query(" << m_applied_index+1 << ")" << std::endl;
      }
      int to_query = m_applied_index+1;
      if (to_query == ix) {
        d= v;
      } else {
        system__decision_struct decision = ext__system__server__query(m_applied_index+1);
        if (!decision.present) {
          if (do_log) {
            std::cout << "in ext__system__server__decide: server__query said no decision" << std::endl;
          }
          break;
        }
        else {
          d = decision.decision;
        }
      }
      m_applied_index++;

      if (do_log) {
        std::cout << "commited[" << m_applied_index << " ] = "
          << d << ";" << std::endl;
      }
      if (d == "") {
        if (do_log) {
          std::cout << "skipping committed nop" << std::endl;
        }
        continue;
      }
      if (do_log) {
        std::cout << "calling committed callback" << std::endl;    }
      callback->committed(m_applied_index, d);
    }
  }

  __strlit msg_kind_to_string(msg_kind k) {
    switch (k) {
    case msg_kind__one_a:
      return "1a";
    case msg_kind__one_b:
      return "1b";
    case msg_kind__two_a:
      return "2a";
    case msg_kind__two_b:
      return "2b";
    case msg_kind__keep_alive:
      return "keep-alive";
    case msg_kind__missing_two_a:
      return "missing_two_a";
    case msg_kind__missing_decision:
      return "missing_decision";
    case msg_kind__decide:
      return "decide";
    }
  }

  __strlit msg_to_string(msg m) {
    return msg_kind_to_string(m.m_kind) + " " +
      "round = " + std::to_string(m.m_round) + " " +
      "inst = " + std::to_string(m.m_inst) + " " +
      "node = " + std::to_string(m.m_node) + " " +
      "value = " + m.m_value;
  }

  void activate_logging() {
    std::this_thread::sleep_for(std::chrono::seconds(20));
    //std::cout << "activating logging" << std::endl;
    //do_log = true;
  }

  void imp__shim__debug_sending(const node& dst, const msg& m)override {
    if (m.m_kind == msg_kind__one_b || m.m_kind == msg_kind__one_a) {
      std::cout << "imp__shim__debug_sending to " << dst << " msg: " << msg_to_string(m) << std::endl;
    }
    if (do_log) {
      std::cout << "imp__shim__debug_sending to " << dst << " msg: " << msg_to_string(m) << std::endl;
    }
  }

  void imp__shim__debug_receiving(const msg& m)override {
    if (m.m_kind == msg_kind__one_b || m.m_kind == msg_kind__one_a) {
      std::cout << "imp__shim__debug_receiving msg: " << msg_to_string(m) << std::endl;
    }
    if (do_log) {
      std::cout << "imp__shim__debug_receiving msg: " << msg_to_string(m) << std::endl;
    }
  }

  /*
  void ext__system__server__debug_sending_one_b(int n)override {
      std::cout << "imp__shim__debug_sending_one_b size: " << std::to_string(n) << std::endl;
  }

  void ext__system__server__debug_info(int c, int first, int next, int inst_status_end) override {
    std::cout << "ext__system__server__debug_info retcount: "
      << std::to_string(c) << " first undecided: " << std::to_string(first) << " next inst: " << std::to_string(next) << " inst_status_end: " << std::to_string(inst_status_end) << std::endl;
  }

  void ext__system__server__debug_two_a_retransmitter(int i) override {
    std::cout << "ext__system__server__debug_two_a_retransmitter for instance "
      << std::to_string(i) << std::endl;
  }

  void ext__system__server__debug_leadership_acquired(int i) override {
    std::cout << "ext__system__server__debug_leadership_aquired for round "
      << std::to_string(i) << std::endl;
  }
  */

};

#endif //_MULTIPAXOS_CONSENSUS_H
