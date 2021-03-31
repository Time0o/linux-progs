#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#include "shell.hpp"

// Note that all builtins perform `local' and `non-local' operations, the former
// are performed in the process that runs the shell and the latter are performed
// in a forked child process so that both modifying shell state and easy
// command redirection become possible.

void Shell::_cd(std::vector<std::string> args, bool local) const
{
  std::string target;
  if (args.size() == 0u)
    {
      char *home = getenv("HOME");
      if (home)
        target = home;
      else if (!local)
        std::cerr << "cd: HOME not set\n";
    }
  else if (args.size() == 1u)
    target = args[0];
  else if (!local)
    std::cerr << "cd: Too many arguments\n";

  if (chdir(target.c_str()) == -1 && !local)
    std::cerr << "cd: " << strerror(errno) << '\n';
}

void Shell::_pwd(std::vector<std::string> args, bool local) const
{
  if (local)
    return;

  if (args.size() > 0u)
    {
      std::cerr << "pwd: Too many arguments\n";
      return;
    }

  char *cwd = get_current_dir_name();
  if (cwd)
    std::cout << cwd << '\n';
  else
    std::cerr << "pwd: " << strerror(errno) << '\n';

  free(cwd);
}

void Shell::_kill(std::vector<std::string> args, bool local) const
{
  if (local)
    return;

  auto usage = []() { std::cerr << "kill: Usage: kill [signo] pid\n"; };

  if (args.size() == 0u || args.size() > 2u)
    usage();

  pid_t pid;
  int signo;
  try
    {
      if (args.size() == 1u)
        {
          signo = SIGTERM;
          pid = static_cast<pid_t>(std::stoi(args[0]));
        }
      else
        {
          signo = std::stoi(args[0]);
          pid = static_cast<pid_t>(std::stoi(args[1]));
        }
    }
  catch (std::invalid_argument const &e)
    {
      usage();
    }

  if (kill(pid, signo) == -1)
    std::cerr << "kill: " << strerror(errno) << '\n';
}

void Shell::_alias(std::vector<std::string> args, bool local)
{
  if (!local && args.empty())
    {
      if (_aliases.empty())
        return;

      for (auto const &a : _aliases)
        std::cout << std::get<0>(a) << "='" << std::get<1>(a) << "'\n";
    }

  for (auto const &arg : args)
    {
      std::size_t eq = arg.find('=');
      if (eq != std::string::npos && eq > 0u)
        _aliases[arg.substr(0u, eq)] = arg.substr(eq + 1u);
      else if (!local)
        {
          auto it = _aliases.find(arg);
          if (it == _aliases.end())
             std::cerr << "alias: " << arg << ": Not found\n";
          else
            std::cout << it->first << "='" << it->second << "'\n";
        }
    }
}

void Shell::_unalias(std::vector<std::string> args, bool local)
{
  if (args.empty())
    {
      if (!local)
        std::cerr << "unalias: Usage: unalias name [name ...]\n";

      return;
    }

  for (auto const &arg : args)
    {
      auto it = _aliases.find(arg);
      if (it == _aliases.end())
        {
          if (!local)
            std::cerr << "unalias: " << arg << ": Not found\n";
        }
      else
        _aliases.erase(it);
    }
}
