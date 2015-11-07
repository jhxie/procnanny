# procnanny
Simple process monitor.

# Personal Info
Name:                Jiahui Xie

Student Number:      1372777

Unix ID:             jxie2

Lecture Section:     A1

Instructor Name:     Paul Lu

Lab Section:         D05

TA Name:             Luke Nitish Kumar

# ASSUMPTIONS
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
