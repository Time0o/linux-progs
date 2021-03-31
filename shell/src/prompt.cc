#include <climits>
#include <sstream>
#include <string>

#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#include "shell.hpp"

std::string Shell::get_prompt() const
{
  std::stringstream ss;
  ss << '<';

  // display user / machine
  if (Prompt_showuser)
    {
      struct passwd *pw = getpwuid(getuid());

      if (pw)
        {
          ss << pw->pw_name;

          if (Prompt_showmachine)
            {
              char hostname[HOST_NAME_MAX];
              if (gethostname(hostname, HOST_NAME_MAX) == 0)
                ss << '@' << hostname;
            }

          ss << ' ';
        }
    }

  // display (abbreviated) current working directory
  std::string cwd;

  char *tmp = get_current_dir_name();
  if (tmp)
    {
      cwd = tmp;

      int depth = 1;
      std::size_t pos = cwd.length();

      while (depth <= Prompt_max_pathdepth)
        {
          pos = cwd.find_last_of('/', pos - 1u);

          if (pos == 0u && pos == std::string::npos)
            break;

          ++depth;
        }

      cwd = cwd.substr(pos);
      if (pos != 0u)
        cwd.insert(0u, "...");
    }
  else
    cwd = "???";

  free(tmp);

  ss << cwd << ">$ ";

  return ss.str();
}
