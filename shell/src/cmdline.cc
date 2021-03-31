#include <ostream>

#include "cmdline.hpp"

std::ostream& operator<<(std::ostream &stream, const Cmdline &cmdline)
{
  auto pl = cmdline.pipeline;

  stream << pl[0];

  if (!cmdline.input_redirect.empty())
    stream << " < " << cmdline.input_redirect;

  for (auto i = 1u; i < pl.size(); ++i)
    stream << " | " << pl[i];

  if (!cmdline.output_redirect.empty())
    stream << " > " << cmdline.output_redirect;

  if (cmdline.bg)
    stream << " &";

  return stream;
}
