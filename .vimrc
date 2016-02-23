set cindent
":0 make the case statement align with the switch above
"N-s no indentation in namespace declaration
"(0 auto line up function parameters when there are new lines
set cinoptions=:0N-s(0
autocmd Filetype c      setlocal expandtab tabstop=8 shiftwidth=8
autocmd Filetype cpp    setlocal expandtab tabstop=8 shiftwidth=8
autocmd Filetype make   setlocal           tabstop=8 shiftwidth=8
autocmd Filetype python setlocal expandtab tabstop=4 shiftwidth=4
set textwidth=79
