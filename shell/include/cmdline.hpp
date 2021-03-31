#ifndef _GUARD_CMDLINE_H
#define _GUARD_CMDLINE_H

#include <ostream>
#include <string>
#include <vector>

struct Cmdline
{
  std::vector<std::string> pipeline;
  std::string input_redirect;
  std::string output_redirect;
  bool bg;
};

std::ostream& operator<<(std::ostream &stream, Cmdline const &cmdline);

#endif // _GUARD_CMDLINE_H
