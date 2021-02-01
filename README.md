# MyShell
 Command line interpreter coded in C, with support for both interactive and batchfile mode.
 It supports the following capabilities:
 - All Linux commands called through exec()
 - Execution of multiple commands in a single line, delimited with && or ;
 - Input redirection with <
 - Output redirection with >
 - Redirection of one command to another through pipes, with symbol |. Consecutive pipes are supported
 - Custom command 'Quit' to exit the program
 
 Future improvements include implementing commands like cd, jobs, history, etc. which are not executed through exec()
