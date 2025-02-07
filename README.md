Rylee Rosenberger

multi thread scheduling

TO RUN THIS PROGRAM:
make
./mts input.txt

This program does the following:

Reads the input file, which is provided as a parameter when the
program is run, e.g. ./mts input.txt. It uses fscanf to read the
input file in the expected format.

The program sucessfully follows the simulation rules via use of 4 priority queues: low
priority east, high priority east, low priority west, and high
priority west. The rules enforced by the automated control system are:

Only one train is on the main track at any given time.
Only loaded trains can cross the main track.
If there are multiple loaded trains, the one with the high priority crosses.
If two loaded trains have the same priority, then:

If they are both traveling in the same direction, the train which finished loading first gets the clearance to cross first. 
If they finished loading at the same time, the one that appeared first in the input file gets the clearance to cross first.
If they are traveling in opposite directions, pick the train which will travel in the direction opposite of which the last 
train to cross the main track traveled. If no trains have crossed the main track yet, the Westbound train has the priority.

To avoid starvation, if there are two trains in the same direction traveling through the main track back to back, the trains 
waiting in the opposite direction get a chance to dispatch one train if any.
To prevent starvation, if an eastbound and westbound train of the same priority are both waiting, the train travelling
opposite to the last train that crossed gets to go.

The program outputs to a file "output.txt", following the
format CLOCK, TRAIN ID, STATUS, DIRECTION.
This output log is appended at each status update for the train:
when ready, when crossing, and when cleared.

The program uses 4 mutexes, one for track access, one for queue
access, one for output file access, and one for broadcasting to load.
There is a broadcasting condvar associated with the broadcasting mutex,
and there is a local condvar for each train as part of the Train struct.

While testing mts, no deadlocks or race conditions were noticed. Each tested
input allowed for full completion of the program in less than a minute.
