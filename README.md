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

2.The recycling nature of pid_t is IGNORED.
  Even though the checking is performed before the
  process is killed, we still have a potential problem
  of killing some other processes end up with the
  pid_t same as the one before.
