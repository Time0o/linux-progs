#ifndef _GUARD_SHELL_H
#define _GUARD_SHELL_H

#include <functional>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <sys/types.h>

#include "cmdline.hpp"

class Shell
{
public:
  // command execution
  pid_t execute_cmd(
    std::string const &cmd, int in, int out, int obsolete = -1) const;

  void execute_cmdline(Cmdline const &cmdline);

  // accessors
  std::string get_name() const { return _name; }
  std::string get_prompt() const;
  std::string get_path(std::string const &prog) const;
  pid_t get_foreground_job() const { return _fg_job; };

  // modifiers
  void set_name(std::string const &name) { _name = name; }
  void set_foreground_job(pid_t job) { _fg_job = job; };

private:
  // prompt settings
  enum {Prompt_showuser = 1, Prompt_showmachine = 1, Prompt_max_pathdepth = 2};

  // program name
  std::string _name;

  // pid of current foreground job (or -1 if there is none)
  pid_t _fg_job = -1;

  // alias table
  std::map<std::string, std::string> _aliases;

  // builtins
  std::map<std::string,
           std::function<void(std::vector<std::string>, bool)>> _builtins
    {
      {"cd", [&](std::vector<std::string> args, bool local)
        {this->_cd(args, local); }},
      {"pwd", [&](std::vector<std::string> args, bool local)
        { this->_pwd(args, local); }},
      {"kill", [&](std::vector<std::string> args, bool local)
        { this->_kill(args, local); }},
      {"alias", [&](std::vector<std::string> args, bool local)
        { this->_alias(args, local); }},
      {"unalias", [&](std::vector<std::string> args, bool local)
        { this->_unalias(args, local); }}
    };

  void _cd(std::vector<std::string>, bool local) const;
  void _pwd(std::vector<std::string>, bool local) const;
  void _kill(std::vector<std::string>, bool local) const;
  void _alias(std::vector<std::string>, bool local);
  void _unalias(std::vector<std::string>, bool local);
};

#endif // _GUARD_SHELL_H
