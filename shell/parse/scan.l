%option nounput

%{
#include <string.h>

#include "parse.tab.h"

#define YY_NO_INPUT
#define YY_NO_UNPUT
%}

%%

"exit"                 { return EXIT; }
"&"                    { return BG; }
"|"                    { return PIPE;}
">"                    { return ORDIR; }
"<"                    { return IRDIR; }
[0-9a-zA-Z_/\-\*\.\=]+ { yylval.str = strdup(yytext); return WORD; }
.                      ;

%%

void parse(char const *str)
{
  YY_BUFFER_STATE buffer = yy_scan_string(str);
  yy_switch_to_buffer(buffer);
  yyparse();
  yy_flush_buffer(buffer);
  yy_delete_buffer(buffer);
}
