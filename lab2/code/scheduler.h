#ifndef __SCHEDULER__H__
#define __SCHEDULER__H__
#include "process.h"
#include <iostream>
#include <string>
#include <stack>
#include <queue>
#include <list>

using namespace std;

class SCHEDULER{
public:
    virtual void add_process(Process *p) = 0;
    virtual Process* get_next_process() = 0;
    virtual bool test_preempt(Process *p, int curtime ) = 0; // false but for ‘E’
    virtual void printSched () = 0; // Debug
};

class FCFS: public SCHEDULER{
public:
    queue<Process*> RUN_QUEUE;

    void add_process(Process *p);
    Process* get_next_process();
    bool test_preempt(Process *p, int curtime );
    void printSched ();
};

class LCFS: public SCHEDULER{
public:
    stack<Process*> RUN_QUEUE;

    void add_process(Process *proc);
    Process* get_next_process();
    bool test_preempt(Process *p, int curtime );
    void printSched ();
};

class SRTF: public SCHEDULER{
public:
    list<Process*> RUN_QUEUE;

    void add_process(Process *proc);
    Process* get_next_process();
    bool test_preempt(Process *p, int curtime );
    void printSched ();
};

class ROUNDROBIN: public SCHEDULER{
public:
    queue<Process*> RUN_QUEUE;
    int quantum;

    ROUNDROBIN(int quantum_): SCHEDULER(){
        quantum = quantum_;
    }

    void add_process(Process *proc);
    Process* get_next_process();
    bool test_preempt(Process *p, int curtime );
    void printSched ();
};

// Class of PRIO and PREPRIO can be originated as the same object
// The difference will be dealt with in simulation()
class PRIO_X: public SCHEDULER{
public:
    int quantum;
    int maxprios;

    queue<Process*> *activeQ;
    queue<Process*> *expiredQ;

    PRIO_X(int quantum_, int maxprios_): SCHEDULER(){
        quantum = quantum_;
        maxprios = maxprios_;
        activeQ = new queue<Process*> [maxprios_];
        expiredQ = new queue<Process*> [maxprios_];
    }

    void add_process(Process*p);
    Process* get_next_process();
    bool test_preempt(Process *p, int curtime );
    void printSched ();

    // Since the sizes of two queues are determined after implement of construction function,
    // We need to calculate total number of process in each layer
    pair<int, int> countQueue () {
        int countactiveQ = 0;
        int countexpiredQ = 0;

        for (int i = 0; i < maxprios; i++) {
            countactiveQ += activeQ[i].size();
        }

        for (int i = 0; i < maxprios; i++) {
            countexpiredQ += expiredQ[i].size();
        }

        return make_pair(countactiveQ, countexpiredQ);
    }
};

#endif