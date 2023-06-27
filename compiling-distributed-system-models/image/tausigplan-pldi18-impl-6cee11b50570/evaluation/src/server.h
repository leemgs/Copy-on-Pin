#include <vector>
#include <mutex>

#ifndef _SERVER_H
#define _SERVER_H

typedef std::vector<std::pair<unsigned long, unsigned long>> cluster_t;

class options {
public:
  std::string   client_port;
  bool          log;
  int           nodeid;
  cluster_t     cluster;

  options() :
    client_port("8000"),
    log(false),
    nodeid(-1)
  {}
};

class committed_callback_t {
public:
  virtual void committed(int ix, const std::string& v) = 0;
};

class consensus {
protected:
  const options&        m_opt;
  committed_callback_t* callback;

public:
  consensus(const options& opt) :
    m_opt(opt),
    callback(nullptr)
  {}

  virtual bool replicate(std::string s) = 0;

  void set_callback(committed_callback_t* c) {
    callback = c;
  }
};

class server {
public:

protected:
  typedef std::map<kvstore::client_id_t, int> connected_clients;

  const options&                    m_opt;
  int                               m_accept_fd;
  socket_set                        m_all_fds;
  kvstore                           m_kvstore;
  connected_clients                 m_connected_clients;
  consensus*                        m_consensus;
  std::mutex mut;
  
  void setup();

  void send_response(const kvstore::response& resp);

  void committed_callback(int ix, const std::string& v);

  class server_callback : public committed_callback_t {
  protected:
    server* m_server;
  public:
    server_callback(server* s) :
      m_server(s)
    {}

    virtual void committed(int ix, const std::string& v) {
      m_server->committed_callback(ix, v);
    }
  };

public:
  server(const options& opt, consensus* c) :
    m_opt(opt),
    m_consensus(c)
  {
    m_consensus->set_callback(new server_callback(this));
  }

  void log(const std::string& s) {
    if (m_opt.log) {
      std::cout << s << std::endl;
    }
  }

  void event_loop();

  virtual ~server() {  }
};

#endif  // _SERVER_H
