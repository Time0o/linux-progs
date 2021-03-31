#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <errno.h>
#include <fcntl.h>
#include <pty.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "cmdline.hpp"
#include "shell.hpp"

pid_t Shell::execute_cmd(
  std::string const &cmd, int in, int out, int obsolete) const
{
  std::string prog, prog_path, interpreter_path;
  std::string args;

  bool is_builtin = false;
  bool is_script = false;

  // split command into program and arguments
  std::size_t pos;
  if ((pos = cmd.find(' ')) != std::string::npos)
    {
      prog = cmd.substr(0u, pos);
      args = cmd.substr(pos + 1u);
    }
  else
    prog = cmd;

  // split argument string into vector of arguments
  std::vector<std::string> argvec;

  if (!args.empty())
    {
      std::size_t pos = 0u;
      for (;;)
        {
          std::size_t nextspace = args.find(' ', pos);

          if (nextspace == std::string::npos)
            {
              argvec.push_back(args.substr(pos));
              break;
            }

          argvec.push_back(args.substr(pos, nextspace - pos));
          pos = nextspace + 1u;
        }
    }

  // expand aliases
  auto alias_it = _aliases.find(prog);
  if (alias_it != _aliases.end())
    prog = alias_it->second;

  // check if command is builtin
  auto builtin_it = _builtins.find(prog);
  if (builtin_it != _builtins.end())
    is_builtin = true;
  else
    {
      // otherwise find absolute path to executable
      prog_path = get_path(prog);

      if (prog_path.empty())
        return -1;

      struct stat prog_exists;
      if (stat(prog_path.c_str(), &prog_exists) == -1)
        {
          std::cerr << _name << ": " << prog_path
                    << ": No such file or directory\n";

          return -1;
        }

      // check if file starts with shebang
      std::ifstream progfile(prog_path);
      if (progfile.good())
        {
          std::string firstline;
          std::getline(progfile, firstline);

          if (firstline.length() >= 2u && firstline.substr(0u, 2u) == "#!")
            {
              is_script = true;

              std::string interpreter = firstline.substr(2u);
              interpreter_path = get_path(interpreter);

              struct stat interpreter_exists;
              if (stat(interpreter_path.c_str(), &interpreter_exists) == -1)
                {
                  std::cerr << _name << ": " << prog_path << ": "
                            << interpreter_path << ": Bad interpreter\n";

                  return -1;
                }
            }
        }
      else
        {
          std::cerr << _name << ": " << prog_path
                    << ": Failed to read file\n";
        }
    }

  // run program in a child process
  pid_t child;
  switch ((child = fork()))
    {
    case -1:
      {
        std::cerr << _name << ": " << strerror(errno) << '\n';
        return -1;
      }
    case 0:
      {
        if (in != -1 && in != STDIN_FILENO)
          {
            if (dup2(in, STDIN_FILENO) == -1 || close(in) == -1)
              {
                std::cerr << _name << ": " << strerror(errno) << '\n';
                _Exit(EXIT_FAILURE);
              }
          }
        else // connect child run in background to dummy pseudo terminal slave
          {
            int master;
            int slave;

            openpty(&master, &slave, nullptr, nullptr, nullptr);

            if (dup2(slave, STDIN_FILENO) == -1)
              {
                std::cerr << _name << ": " << strerror(errno) << '\n';
                _Exit(EXIT_FAILURE);
              }
          }

        if (out != -1 && out != STDOUT_FILENO) // redirect stdout
          {
            if (dup2(out, STDOUT_FILENO) == -1 || close(out) == -1)
              {
                std::cerr << _name << ": " << strerror(errno) << '\n';
                _Exit(EXIT_FAILURE);
              }
          }

        if (obsolete != -1) // close superfluous pipe end in child
          {
            if (close(obsolete) == -1)
              {
                std::cerr << _name << ": " << strerror(errno) << '\n';
                _Exit(EXIT_FAILURE);
              }
          }

        if (is_builtin) // execute builtin
          {
            builtin_it->second(argvec, false);
            _Exit(EXIT_SUCCESS);
          }
        else // construct argv array and execute script or program
          {
            char **argv = new char *[argvec.size() + 3u];

            // leave space in argv[0] for possible script interpreter
            argv[1] = new char[prog_path.length() + 1u];
            strcpy(argv[1], prog_path.c_str());

            for (auto i = 0u; i < argvec.size(); ++i)
              {
                argv[i + 2u] = new char[argvec[i].length() + 1u];
                strcpy(argv[i + 2u], argvec[i].c_str());
              }

            argv[argvec.size() + 2u] = nullptr;

            if (is_script) // execute script
              {
                argv[0] = new char [interpreter_path.size() + 1u];
                strcpy(argv[0], interpreter_path.c_str());

                if (execve(interpreter_path.c_str(), argv, environ) == -1)
                  _Exit(EXIT_FAILURE);
              }
            else // execute program
              {
                if (execve(prog_path.c_str(), &argv[1], environ) == -1)
                  _Exit(EXIT_FAILURE);
              }
          }
      }
    default:
      {
        if (builtin_it != _builtins.end())
          builtin_it->second(argvec, true);
      }
    }

  return child;
}

void Shell::execute_cmdline(Cmdline const &cmdline)
{
  int in_fd = -1;
  int out_fd = -1;

  std::vector<pid_t> jobs;
  struct timespec job_wait_timeout {0, 1000};

  // obtain input/output redirection file descriptors
  if (!cmdline.input_redirect.empty())
    {
      in_fd = open(cmdline.input_redirect.c_str(), O_RDONLY);

      if (in_fd == -1)
        {
          std::cerr << _name << ": " << strerror(errno) << '\n';
          goto cleanup;
        }
    }
  else
    in_fd = cmdline.bg ? -1 : STDIN_FILENO;

  if (!cmdline.output_redirect.empty())
    {
      out_fd = open(cmdline.output_redirect.c_str(),
                    O_WRONLY | O_CREAT | O_TRUNC, 0666);

      if (out_fd == -1)
        {
          std::cerr << _name << ": " << strerror(errno) << '\n';
          goto cleanup;
        }
    }
  else
    out_fd = STDOUT_FILENO;

  // start single process
  if (cmdline.pipeline.size() == 1u)
    {
      int proc = execute_cmd(cmdline.pipeline[0], in_fd, out_fd);
      if (proc == -1)
        return;

      if (!cmdline.bg)
        {
          _fg_job = proc;

          if (waitpid(_fg_job, nullptr, 0) == -1
              && errno != ECHILD && errno != EINTR)
            {
              std::cerr << _name << ": " << strerror(errno) << '\n';
            }
        }
      else
        std::cout << "Sent to background: " << proc << '\n';

      goto cleanup;
    }

  // start pipeline
  int pipefd[2];

  for (auto i = 0u; i < cmdline.pipeline.size(); ++i)
    {
      if (i == 0u)
        {
          pipe(pipefd);
          int proc = execute_cmd(
            cmdline.pipeline[i], in_fd, pipefd[1], pipefd[0]);

          if (close(pipefd[1]) == -1)
            std::cerr << _name << ": " << strerror(errno) << '\n';

          if (proc == -1)
            {
              if (close(pipefd[0]) == -1)
                std::cerr << _name << ": " << strerror(errno) << '\n';

              goto cleanup;
            }

          if (!cmdline.bg)
            jobs.push_back(proc);
        }
      else if (i == cmdline.pipeline.size() - 1u)
        {
          int proc = execute_cmd(cmdline.pipeline[i], pipefd[0], out_fd);

          if (close(pipefd[0]) == -1)
            std::cerr << _name << ": " << strerror(errno) << '\n';

          if (proc == -1)
            goto cleanup;

          if (!cmdline.bg)
            jobs.push_back(proc);
          else
            std::cout << "Sent to background: " << proc << '\n';
        }
      else
        {
          int pipe_read = pipefd[0];
          pipe(pipefd);

          int proc = execute_cmd(
            cmdline.pipeline[i], pipe_read, pipefd[1], pipefd[0]);

          if (close(pipe_read) == -1)
            std::cerr << _name << ": " << strerror(errno) << '\n';

          if (close(pipefd[1]) == -1)
            std::cerr << _name << ": " << strerror(errno) << '\n';

          if (proc == -1)
            goto cleanup;

          if (!cmdline.bg)
            jobs.push_back(proc);
        }
    }

  // wait for jobs to finish
  int status;
  bool done;

  while (!jobs.empty())
    {
      nanosleep(&job_wait_timeout, nullptr);

      for (pid_t job : jobs)
        {
          done = false;

          switch (waitpid(job, &status, WNOHANG))
            {
            case -1:
              if (errno == ECHILD)
                done = true;
              else
                std::cerr << _name << ": " << strerror(errno) << '\n';
              break;
            case 0:
              break;
            default:
              done = true;
            }

          if (done)
            jobs.erase(std::remove(jobs.begin(), jobs.end(), job), jobs.end());
        }
    }

cleanup:
  if (in_fd != -1 && in_fd != STDIN_FILENO && close(in_fd) == -1)
    std::cerr << _name << ": " << strerror(errno) << '\n';

  if (out_fd != -1 && out_fd != STDOUT_FILENO && close(out_fd) == -1)
    std::cerr << _name << ": " << strerror(errno) << '\n';
}
