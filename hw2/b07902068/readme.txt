1. Execution
Put bidding_system.c host.c player.c in same directory.
Compile by command "make".
Execute by "./bidding_system [host_num] [player_num]".
If there is any existing FIFO having the same pathname as the FIFO that the bidding_system is going to make,such as "Host.FIFO" or "Host1.FIFO" ,the bidding_sytem will output error message and exit.

2.Description
Reference of listing C(N, 8): https://www.geeksforgeeks.org/print-all-possible-combinations-of-r-elements-in-a-given-array-of-size-n/

I developed most part of my program, except for listing C(N, 8). 
When I had some questions about how some functions behave or encountered a bug like fork error, which is due to my not "wait" for child process, I sought for help from B07902084.
While working on this assignment, I realize that checking the return value of function is an important step to avoid possible error, like unable to fork.
The most impressive thing I learned is that do not ever write an infinite loop after an fork error, since all process id will be occupied and make me cannot command "ps", "kill" or re-login on linux work station.
Although the spec says that FIFO and pipe should be fsync after a write, I found that fsync return error on FIFO.