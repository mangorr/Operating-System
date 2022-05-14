#include "scheduler.h"

using namespace std;

// Implement of schedulers

// FCFS
void FCFS::add_process(Process *p) {
    RUN_QUEUE.push(p);
}

Process* FCFS::get_next_process() {
    if(RUN_QUEUE.empty()){
        return nullptr;
    }
    Process *proc = RUN_QUEUE.front();
    RUN_QUEUE.pop();
    return proc;
}

bool FCFS::test_preempt(Process *p, int curtime ) {
    return false;
}

void FCFS::printSched () {
    queue<Process*> print_queue = RUN_QUEUE;
    cout<<"------------------------"<<endl;
    if (print_queue.empty()) cout<<"SCHEDULER is empty"<<endl;
    while (!print_queue.empty()) {
        Process* tmp = print_queue.front();
        cout<<"PID: "<<tmp->PID<<": AT, CB, IO: "<<tmp->AT<<" "<<tmp->CB<<" "<<tmp->IO<<endl;
        cout<<"cpu_burst, io_busrt: "<<tmp->cpu_burst<<" "<<tmp->io_burst<<endl;
        cout<<"state_ts, future_ts: "<<tmp->state_ts<<" "<<tmp->future_ts<<endl;
        print_queue.pop();
    }
    cout<<"------------------------"<<endl;
}

// LCFS
void LCFS::add_process(Process *proc){
    RUN_QUEUE.push(proc);
}

Process* LCFS::get_next_process(){
    if(RUN_QUEUE.empty()){
        return nullptr;
    }
    Process *proc = RUN_QUEUE.top();
    RUN_QUEUE.pop();
    return proc;
}

bool LCFS::test_preempt(Process *p, int curtime ) {
    return false;
}

void LCFS::printSched () {}

// SRTF
void SRTF::add_process(Process *proc){
    if (RUN_QUEUE.empty()){
        RUN_QUEUE.insert(RUN_QUEUE.begin(), proc);
    }
    else {
        auto it = RUN_QUEUE.begin();
        for (; it!=RUN_QUEUE.end(); it++){
            if ((*it)->remain_time > proc->remain_time) break;
        }
        RUN_QUEUE.insert(it, proc); //insert according to the remaining time
    }
}

Process* SRTF::get_next_process(){
    if(RUN_QUEUE.empty()){
        return nullptr;
    }

    Process *proc = RUN_QUEUE.front();
    RUN_QUEUE.pop_front();
    return proc;
}

bool SRTF::test_preempt(Process *p, int curtime ) {
    return false;
}

void SRTF::printSched () {}

// ROUNDROBIN
void ROUNDROBIN::add_process(Process *proc){
    RUN_QUEUE.push(proc);
}

Process* ROUNDROBIN::get_next_process(){
    if(RUN_QUEUE.empty()){
        return nullptr;
    }
    Process *proc = RUN_QUEUE.front();
    RUN_QUEUE.pop();
    return proc;
}

bool ROUNDROBIN::test_preempt(Process *p, int curtime ) {
    return false;
}

void ROUNDROBIN::printSched () {}

// PRIO_X
void PRIO_X::add_process(Process* proc){
    if (proc->dynamic_prio<0){ // Insert into expiredQ
        proc->dynamic_prio = proc->static_prio - 1;
        expiredQ[proc->dynamic_prio].push(proc);
    } else { // Insert into activeQ
        activeQ[proc->dynamic_prio].push(proc);
    }

    // cout<<"function origin:"<<countQueue().first<<" "<<countQueue().second<<endl;
}

Process* PRIO_X::get_next_process() {
    int countActiveQ_ = countQueue().first;
    int countExpiredQ_ = countQueue().second;

    if(countActiveQ_ == 0){
        swap(activeQ, expiredQ);
        swap(countActiveQ_, countExpiredQ_);
    }
    Process *proc = nullptr;
    if (countActiveQ_ > 0){
        for(int i = maxprios - 1; i >= 0; i--){
            if (!activeQ[i].empty()){
                proc = activeQ[i].front();
                activeQ[i].pop();
                break;
            }
        }
    }
    return proc;
}

bool PRIO_X::test_preempt(Process *proc, int curtime ) {
    if (proc->state_ts == curtime) return true;
    else return false;
}

void PRIO_X::printSched () {}
