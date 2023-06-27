#include "common.h"

#include <map>
#include <sstream>

#ifndef _KVSTORE_H
#define _KVSTORE_H

class kvstore {
public:
  typedef unsigned int client_id_t;
  typedef unsigned int seqnum_t;

  class request {
  public:
    enum tag { GET, PUT, DEL, CAS, CAD };

    client_id_t        client_id;
    seqnum_t           seqnum;
    tag                tag;
    std::string        key;
    std::string        value;           // used only for PUT, CAS, CAD
    optional_string    expected_value;  // used only for CAS; NULL if expected not to be present

    std::string to_string() const;
  };

  class response {
  public:
    client_id_t        client_id;
    seqnum_t           seqnum;
    std::string        key;
    optional_string    value;
    optional_string    old_value;

    std::string to_string() const;
  };

protected:
  typedef std::map<std::string, std::string>     map_t;
  typedef std::map<client_id_t, response>        response_cache;

  map_t                             m_kvstore;
  response_cache                    m_response_cache;

public:
  static void parse_request(const std::string& s, request& req);
  static client_id_t parse_request_client_id(const std::string& s);

  bool execute_request(const request& req, response& resp);
};

#endif  // _KVSTORE_H
