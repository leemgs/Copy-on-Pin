#include "kvstore.h"

std::string kvstore::request::to_string() const {
  std::ostringstream dest;

  dest << client_id << " ";
  dest << seqnum << " ";

  switch (tag) {
  case GET:
    dest << "GET" << " " 
         << key   << " " 
         << "-"   << " " 
         << "-";
    break;
  case PUT:
    dest << "PUT" << " " 
         << key   << " " 
         << value << " " 
         << "-";
    break;
  case DEL:
    dest << "DEL" << " " 
         << key   << " " 
         << "-"   << " "
         << "-";
    break;
  case CAS:
    dest << "CAS"          << " "
         << key            << " "
         << expected_value << " "
         << value;
    break;
  case CAD:
    dest << "CAD" << " " 
         << key   << " " 
         << value;
    break;
  }

  return dest.str();
}

std::string kvstore::response::to_string() const {
  std::ostringstream dest;

  dest << "Response "
    //       << client_id << " "
       << seqnum << " "
       << key << " "
       << (value == nullptr ? "-" : *value) << " "
       << (old_value == nullptr ? "-" : *old_value);

  return dest.str();
}


void kvstore::parse_request(const std::string& s, request& req) {
  size_t idx = 0;
  size_t next = s.find(' ', idx);
  assert(next != std::string::npos, "Unexpected end of string.");

  req.client_id = (unsigned int)std::stol(s.substr(idx, next - idx));
  idx = next;

  assert(idx < s.length(), "Unexpected end of string.");
  assert(s[idx] == ' ', std::string("parse_request: Unexpected character '") + s[idx] + "'");
  idx++;
  assert(idx < s.length(), "Unexpected end of string.");

  next = s.find(' ', idx);
  assert(next != std::string::npos, "Unexpected end of string.");
  req.seqnum = (unsigned int)std::stol(s.substr(idx, next - idx));
  idx = next;

  assert(idx < s.length(), "Unexpected end of string.");
  assert(s[idx] == ' ', std::string("parse_request: Unexpected character '") + s[idx] + "'");
  idx++;
  assert(idx < s.length(), "Unexpected end of string.");

  std::string cmd = s.substr(idx, 3);
  if (cmd == "GET") {
    req.tag = request::GET;
  } else if (cmd == "PUT") {
    req.tag = request::PUT;
  } else if (cmd == "DEL") {
    req.tag = request::DEL;
  } else if (cmd == "CAS") {
    req.tag = request::CAS;
  } else if (cmd == "CAD") {
    req.tag = request::CAD;
  } else {
    assert(false, std::string("Unknown tag ") + cmd);
  }

  idx += 3;
  assert(idx < s.length(), "Unexpected end of string.");
  assert(s[idx] == ' ', std::string("parse_request: Unexpected character '") + s[idx] + "'");
  idx++;
  assert(idx < s.length(), "Unexpected end of string.");

  next = s.find(' ', idx);
  assert(next != std::string::npos, "Unexpected end of string.");

  req.key = s.substr(idx, next - idx);

  idx = next;
  assert(idx < s.length(), "Unexpected end of string.");
  assert(s[idx] == ' ', std::string("parse_request: Unexpected character '") + s[idx] + "'");
  idx++;
  assert(idx < s.length(), "Unexpected end of string.");

  req.value = "";
  req.expected_value = optional_string(nullptr);

  if (req.tag != request::GET && req.tag != request::DEL) {
    next = s.find(' ', idx);
    assert(next != std::string::npos, "Unexpected end of string.");

    req.value = s.substr(idx, next - idx);

    idx = next;
    assert(idx < s.length(), "Unexpected end of string.");
    assert(s[idx] == ' ', std::string("parse_request: Unexpected character '") + s[idx] + "'");
    idx++;
    assert(idx < s.length(), "Unexpected end of string.");

    if (req.tag == request::CAS) {
      next = s.find(' ', idx);

      auto expected = s.substr(idx, next == std::string::npos ? next : next - idx);

      req.expected_value = optional_string (expected == "-" ? nullptr :
                                            new std::string(expected));
    }
  }
}

bool kvstore::execute_request(const request& req, response& resp) {
  const std::string& k = req.key;

  auto cached_response = m_response_cache.find(req.client_id);
  if (cached_response != m_response_cache.end() &&
      cached_response->second.seqnum >= req.seqnum) {

    if (cached_response->second.seqnum == req.seqnum) {
      resp = cached_response->second;
      return true;
    }
    // otherwise, the request is older than most recent; just drop it

    return false;
  }

  resp.client_id = req.client_id;
  resp.seqnum = req.seqnum;
  resp.key = k;

  if (m_kvstore.count(k) > 0) {
    resp.old_value = optional_string(new std::string(m_kvstore[k]));
  } else {
    resp.old_value = optional_string(nullptr);
  }

  switch (req.tag) {
  case request::GET:
    resp.value = resp.old_value;
    break;
  case request::PUT:
    m_kvstore[k] = req.value;
    resp.value = optional_string(new std::string(req.value));
    break;
  case request::DEL:
    m_kvstore.erase(k);
    resp.value = optional_string(nullptr);
    break;
  case request::CAS:
    if (resp.old_value == req.expected_value) {
      m_kvstore[k] = req.value;
      resp.value = optional_string(new std::string(m_kvstore[k]));
    } else {
      resp.value = resp.old_value;
    }
    break;
  case request::CAD:
    if (resp.old_value == req.expected_value) {
      m_kvstore.erase(k);
      resp.value = optional_string(nullptr);
    } else {
      resp.value = resp.old_value;
    }
    break;
  }

  m_response_cache[req.client_id] = resp;

  return true;
}

kvstore::client_id_t kvstore::parse_request_client_id(const std::string& s) {
  size_t idx;
  return std::stoi(s, &idx);
}
