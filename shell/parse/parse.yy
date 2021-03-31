%{
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "shell.hpp"

extern "C" {
  extern int yylex();
  extern int yyparse();
  extern char *yytext;
}

// declared in shell.cc
extern Shell shell;

// commandline structure filled by the parser and passed to the shell
static Cmdline cmdline;

void yyerror(std::string const &s)
{
  std::cerr << shell.get_name() << ": Syntax error near unexpected token `"
            << yytext << "'\n";

  cmdline = Cmdline();
}
%}

%union { char *str; }

%start ROOT

%token BG
%token EXIT
%token PIPE
%token IRDIR
%token ORDIR
%token <str> WORD

%type <str> args

%%

/* Note that the following rules purposefully use only C-style heap memory
   management in order to more easily mix with the scanner which is compiled
   as a C program. The parser only really uses C++ features where absolutely
   necessary in order to not have deal with Yacc/Bison's terrible C++
   interface. */

ROOT:
  EXIT {
    exit(EXIT_SUCCESS);
  }
| cmd irdir pipeline ordir bg {
    shell.execute_cmdline(cmdline);
    cmdline = Cmdline();
  }
;

cmd:
  WORD args {
    if (!$2)
      cmdline.pipeline.push_back($1);
    else
      {
        char *cmd = (char *)malloc(strlen($1) + strlen($2) + 2);
        sprintf(cmd, "%s %s", $1, $2);
        cmdline.pipeline.push_back(cmd);

        free(cmd);
      }

    free($1);
    free($2);
  }
;

args:
  /* empty */ {
    $$ = nullptr;
  }
| args WORD {
    if (!$1)
      $$ = $2;
    else
      {
        $$ = (char *)malloc(strlen($1) + strlen($2) + 2);
        sprintf($$, "%s %s", $1, $2);

        free($1);
        free($2);
      }
  }
;

pipeline:
  /* empty */ {}
| PIPE cmd pipeline {}
;

irdir:
  /* empty */ {}
| IRDIR WORD {
    cmdline.input_redirect = $2;
    free($2);
  }
;

ordir:
  /* empty */ {}
| ORDIR WORD {
    cmdline.output_redirect = $2;
    free($2);
  }
;

bg:
  /* empty */ {}
| BG {
    cmdline.bg = true;
  }
;
