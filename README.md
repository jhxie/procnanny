# procnanny
A simple process monitor using the client-server model.

# Build/Install
Type 'make' after you extract everything from the tarball distribution
and the 2 executables would be generated in the 'bin' folder.

# Usage
For the server, please remember to export the 2 environment variables like
the following before running the 'bin/procnanny.server' program:

```bash
export PROCNANNYSERVERINFO=./report
export PROCNANNYLOGS=./log
```

and then run it as 'bin/procnanny.server [CONFIGURATION FILE]'.

For the client, there is no need to export any environment variables,
just simply run it as 'bin/procnanny.client [HOST NAME] 9898',
where the '9898' is the predefined listening port number of the server.

To generate the documentation, you need to have doxygen installed; even though
the source code does not contain any doxygen formatting directives, you can
still use the cross-reference feature of it to assist code reading:
```bash
doxygen Doxyfile
```
the resulting documentation would reside in doc folder.

# Assumptions
1.The length of a line is no more than 1023 characters.
  The reason for using a statically allocated array rather than
  something like getline() is that memwatch cannot detect the memory
  allocated by getline().

2.The recycling nature of pid is IGNORED.
  Even though the checking is performed on errno after the
  process is killed, we still have a potential problem
  of killing some other processes end up with the
  pid same as the one before.

3.The configuration file is ALWAYS VALID: there cannot be redundant lines with
  identical information or a "re-definition" like "coredumpctl 8" and then
  there is a line "coredumpctl 10" appear in the configuration file at the same
  time; however, "re-definition" is allowed if the file is re-read (caught a
  SIGHUP signal).

4.The length of the program name to be monitored is no more than 1023
  characters as well(based on ASSUMPTION #1).

5.The size of deletion queue is assumed to be 1024 at maximum; this queue is
  used for storing information about deletion of the pw\_pid\_info structure
  upon the success/failure of a monitored process.

6.The server and clients cannot crash or be killed.

7.The server and all its clients have the same endianness; in other words,
  the data sent through the socket is not serialized.

8.Both SIGINT and SIGHUP signals cannot be sent to the server process before
  the handlers are successfully set up.

# License
Copyright &copy; 2015 - 2016 Jiahui Xie
Licensed under the [BSD 2-Clause License][BSD2].
Distributed under the [BSD 2-Clause License][BSD2].

[BSD2]: https://opensource.org/licenses/BSD-2-Clause
