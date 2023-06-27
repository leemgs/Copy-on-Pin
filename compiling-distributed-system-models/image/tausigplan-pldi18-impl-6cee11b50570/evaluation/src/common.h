#include <string>
#include <memory>

#include <sys/types.h>

#ifndef _COMMON_H
#define _COMMON_H

typedef std::shared_ptr<std::string> optional_string;

class failed_assertion : std::exception {
protected:
  const std::string& msg;

public:
  failed_assertion(const std::string& msg) :
    msg(msg)
  {}

  virtual const char* what() const throw() {
    return msg.c_str();
  }
};

void assert(bool b, const std::string& msg = "failed assertion");

class socket_set {
public:
  int max;
  fd_set the_fd_set;

  socket_set() :
    max(-1)
  {
    FD_ZERO(&the_fd_set);
  }

  void add(int fd) {
     FD_SET(fd, &the_fd_set);
     if (fd > max) {
       max = fd;
     }
  }

  void remove(int fd) {
     FD_CLR(fd, &the_fd_set);
  }

  void clear() {
    FD_ZERO(&the_fd_set);
  }
};

template <typename T> T max(T a, T b) {
  return a >= b ? a : b;
}

#endif  // _COMMON_H
