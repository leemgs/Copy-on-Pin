#include "common.h"
#include <iostream>

void assert(bool b, const std::string& msg) {
  if (!b) {
    std::cout << "failed assertion: " << msg << std::endl;
    throw failed_assertion(msg);
  }
}
