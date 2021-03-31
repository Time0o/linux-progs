#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>

#include <readline/history.h>
#include <readline/readline.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <wordexp.h>

#include "shell.hpp"

// bison wrapper imported from flex generated sourcefile
extern "C" void parse(char const *str);

// global shell object
Shell shell;

// signal handling
static bool sigint = false;
static bool sigtstp = false;
static bool sigquit = false;

static void sighandle(int signo)
{
  pid_t fg_job = shell.get_foreground_job();
  if (fg_job != -1)
    kill(fg_job, signo);

  if (signo == SIGINT)
    sigint = true;
  else if (signo == SIGTSTP)
    sigtstp = true;
  else if (signo == SIGQUIT)
    sigquit = true;
}

int main(int argc, char **argv)
{
  // store program name
  shell.set_name(argv[0]);

  // register signal handlers
  int signals[] = { SIGINT, SIGTSTP, SIGQUIT };

  struct sigaction sa = {};
  sa.sa_handler = sighandle;

  for (int signal : signals)
    {
      if (sigaction(signal, &sa, nullptr) == -1)
        std::cerr << shell.get_name() << ": " << strerror(errno) << '\n';
    }

  // avoid zombiefication of child processes
  struct sigaction sa_ignore = {};
  sa_ignore.sa_handler = SIG_IGN;

  if (sigaction(SIGCHLD, &sa_ignore, nullptr) == -1)
    std::cerr << shell.get_name() << ": " << strerror(errno) << '\n';

  // enter readline loop
  char *buf;

  while ((buf = readline(shell.get_prompt().c_str())))
    {
      if (strlen(buf) > 0)
        {
          // add commandline to history
          add_history(buf);

          // expand wildcards
          std::string input(buf);
          std::string input_expanded;

          std::size_t pos = 0u;
          while (input[pos] == ' ')
            {
              if (++pos == input.length())
                continue;
            }

          bool expansion_done = false;
          bool expansion_error = false;
          while (!expansion_done)
            {
              std::size_t nextspace = input.find(' ', pos);

              std::string token;
              if (nextspace == std::string::npos)
                {
                  token = input.substr(pos);
                  expansion_done = true;
                }
              else
                {
                  token = input.substr(pos, nextspace - pos);

                  pos = nextspace;
                  while (input[pos] == ' ')
                    {
                      if (++pos == input.length())
                        expansion_done = true;
                    }
                }

              if (token.find_first_of("|&;<>(){}") == std::string::npos)
                {
                  wordexp_t exp;

                  if (wordexp(token.c_str(), &exp, WRDE_NOCMD) != 0)
                    {
                      std::cerr << shell.get_name() << ": Expansion error\n";
                      expansion_error = true;
                      break;
                    }

                  for (auto i = 0u; i < exp.we_wordc; ++i)
                    {
                      input_expanded += exp.we_wordv[i];

                      if (!expansion_done || i != exp.we_wordc - 1u)
                        input_expanded += ' ';
                    }

                  wordfree(&exp);
                }
              else
                {
                  input_expanded += token;

                  if (!expansion_done)
                    input_expanded += ' ';
                }
            }

          if (expansion_error)
            continue;

          parse(input_expanded.c_str());
        }

      free(buf);

      // cleanup after interrupting signals
      pid_t fg_job = shell.get_foreground_job();

      if (sigint)
        {
          sigint = false;

          if (fg_job != -1)
            std::cout << '\n';
        }
      else if (sigtstp)
        {
          sigtstp = false;

          if (fg_job != -1)
            std::cout << "Sent to background: " << fg_job << '\n';
        }
      else if (sigquit)
        {
          sigquit = false;

          if (fg_job != -1)
            std::cout << "Quit (core dumped)\n";
        }

      shell.set_foreground_job(-1);
    }
}
