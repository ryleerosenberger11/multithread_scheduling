Rylee Rosenberger
v01000941

CSC360 p2 - multi thread scheduling

TO RUN THIS PROGRAM:
make
./mts input.txt

mts has been fully implemented. The program successfully does the following:

Reads the input file, which is provided as a parameter when the
program is run, e.g. ./mts input.txt. It uses fscanf to read the
input file in the expected format.

The program sucessfully follows the simulation rules as outlined in 
3.2 of the p2 spec. It does this via use of 4 priority queues: low
priority east, high priority east, low priority west, and high
priority west. To prevent starvation, if an eastbound and westbound train of the same priority are both waiting, the train travelling
opposite to the last train that crossed gets to go.
*note: "To avoid starvation, if there are two trains in the same direction traveling through the main track back to back, the trains waiting in the opposite direction get a chance to dispatch one train if any." was not considered in the train signalling logic.

The program outputs to a file "output.txt" as expected, following the
format CLOCK, TRAIN ID, STATUS, DIRECTION.
This ouput log is appended at each status update for the train:
when ready, when crossing, and when cleared.

The program uses 4 mutexes, one for track access, one for queue
access, one for output file access, and one for broadcasting to load.
There is a broadcasting condvar associated with the broadcasting mutex,
and there is a local condvar for each train as part of the Train struct.

While testing mts, no deadlocks or race conditions were noticed. Each tested
input allowed for full completion of the program in less than a minute.
