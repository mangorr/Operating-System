// process header
#ifndef __PROCESS__H__
#define __PROCESS__H__

class Process{
public:
    // The same name in instruction
    // Input variables
    int AT;
    int TC;
    int CB;
    int IO;
    int PID;

    // Output variables
    int IT = 0;
    int FT;
    int TT;
    int CW = 0;
    
    // Used in simulation()
    int static_prio;
    int dynamic_prio;
    int state_ts;
    int future_ts;

    int cpu_burst = 0;
    int io_burst = 0;
    int remain_time;

    Process(int at, int tc, int cb, int io, int prio, int pid) {
        AT = state_ts = at;
        TC = remain_time = tc;
        CB = cb;
        IO = io;
        PID = pid;
        static_prio = prio;
        dynamic_prio = static_prio-1;
    }

    void termiate(int currentTime) {
        TT = currentTime - AT;
        FT = currentTime;
    }
};

#endif