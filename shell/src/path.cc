#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include <errno.h>
#include <dirent.h>

#include "shell.hpp"

std::string Shell::get_path(std::string const &prog) const
{
  if (prog.find('/') != std::string::npos)
    return prog;

  std::string path(getenv("PATH"));

  if (!path.empty())
    {
      std::string searchdir;

      std::size_t pos, delim;
      pos = 0u;

      bool done = false;
      while (!done)
        {
          delim = path.find(':', pos + 1u);
          if (delim == std::string::npos)
            searchdir = path.substr(pos);
          else
            searchdir = path.substr(pos, delim - pos);

          if (delim == std::string::npos)
            done = true;
          else
            pos = delim + 1u;

          DIR *dir = opendir(searchdir.c_str());
          if (!dir)
            continue;

          for (;;)
            {
              struct dirent *dire = readdir(dir);
              if (!dire)
                break;

              if (strcmp(dire->d_name, prog.c_str()) == 0)
                {
                  std::string fullpath(searchdir);
                  if (fullpath.back() != '/')
                    fullpath += '/';
                  fullpath += dire->d_name;

                  return fullpath;
                }
            }
          closedir(dir);
        }
    }

  std::cerr << _name << ": " << prog << ": " << "Command not found\n";
  return "";
}
