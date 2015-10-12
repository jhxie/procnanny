set print pretty on
set environment PROCNANNYLOGS=./log

# macro definition, show all user defined macros with "show user"
define pstrlen
printf "The length of the string is %d\n", $(arg0)
end

tbreak src/procwatch.c:procwatch
