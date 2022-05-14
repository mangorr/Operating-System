// DES
#ifndef __DES__H__
#define __DES__H__
#include "process.h"
#include <iostream>
#include <list>
#include <string>

using namespace std;

typedef enum {STATE_CREATED, STATE_READY, STATE_RUNNING, STATE_BLOCK, STATE_PREEMPT, STATE_TERMINATE} STATES;
typedef enum {CPU, IO, NA} INFOTYPE;

ostream &operator << ( ostream& strm, STATES tt ) { // In order to print enum
   const string stateType[] = {"CREATED", "READY", "RUNNG", "BLOCK", "PREEMPT", "TERMINATE"}; // RUNNG? Keep the same with refoutv
   return strm << stateType[tt];
}

class EVENT{
public:
    int evtTimeStamp;
    STATES new_state;
    STATES old_state;
    Process* evtProcess;

    EVENT() {}

    EVENT(Process* proc) {
        evtProcess = proc;
    }

    EVENT(Process* proc, STATES state) {
        new_state = state;
        evtProcess = proc;
    }

    EVENT(int evtTimeStamp_, Process* proc) {
        evtTimeStamp = evtTimeStamp_;
        evtProcess = proc;
    }

    void baseTrack (int CURRENT_TIME, int timeInPrevState, INFOTYPE type) {
        cout<<CURRENT_TIME<<" "<<evtProcess->PID<<" "<<timeInPrevState<<": ";
        cout<<old_state<<" -> "<<new_state;

        int info = type;
        switch(info){
            case CPU:
            {
                cout<<" cb="<<evtProcess->cpu_burst<<" rem="<<evtProcess->remain_time<<" prio="<<evtProcess->dynamic_prio<<endl;
            }
            break;
            case IO:
            {
                cout<<"  ib="<<evtProcess->io_burst<<" rem="<<evtProcess->remain_time<<endl;
            }
            break;
            default:
            {
                cout<<endl;
            }
            break;
        }
    }

    void terminateTrack (int CURRENT_TIME, int timeInPrevState) {
        cout<<CURRENT_TIME<<" "<<evtProcess->PID<<" "<<timeInPrevState<<": Done"<<endl;
    }
};

class DES{
public:
    friend class EVENT;
    list<EVENT*> event_queue;

    EVENT* get_event(){
        if (event_queue.empty()) {
            return nullptr;
        } else {
            auto current_event = event_queue.front();
            event_queue.pop_front();
            return current_event;
        }
    }

    void put_event(EVENT* evt){
        if (event_queue.empty()){
            event_queue.insert(event_queue.begin(), evt);
        } else {
            auto it = event_queue.begin();
            for (; it != event_queue.end(); ++it){
                if ((*it)->evtTimeStamp > evt->evtTimeStamp) { // Insert in chronological order
                    break;
                }
            }
            event_queue.insert(it, evt);
        }
    }

    void rm_event(Process *proc){
        // Delete according to unique PID
        auto it = event_queue.begin();
        for(; it!=event_queue.end();){
            if ((*it)->evtProcess->PID == proc->PID){
                it = event_queue.erase(it);
            }
            else it++;
        }
    }

    int get_next_event_time() {
        if(event_queue.empty()){
            return -1;
        } else {
            return event_queue.front()->evtTimeStamp;
        }
    }

    bool checkPreemptTime(Process* p, int curtime){
        if (event_queue.empty()) return false;
        else {
            auto it = event_queue.begin();
            for (; it!=event_queue.end(); it++){
                if ((*it)->evtTimeStamp == curtime && p->PID == (*it)->evtProcess->PID) 
                    return true;
            }
            return false;
        }
    }

    // debug
    void printEvent () {
        if (event_queue.empty()) cout<<"event_queue is empty."<<endl;
        else {
            for (auto it = event_queue.begin(); it != event_queue.end(); ++it) {
                cout<<"event->process: "<<(*it)->evtProcess->PID<<endl;
            }
        }
    }

};

#endif