#include "process.h"
#include "scheduler.h"
#include "des.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

// server------------------------------------------------------------
// ssh ym2360@access.cims.nyu.edu
// password: *******
// ssh ym2360@linserv1.cims.nyu.edu
// scp lab2_assign.zip ym2360@access.cims.nyu.edu:/home/ym2360/os/lab2
// scp lab3_assign.zip ym2360@access.cims.nyu.edu:/home/ym2360/os/lab3
// unzip -o -d /home/ym2360/os/lab2 submit_lab2.zip
// ./runit.sh /home/ym2360/os/lab2/outputdir /home/ym2360/os/lab2/submit_lab2/sched
// ./runit.sh /home/ym2360/os/lab3/outputdir /home/ym2360/os/lab3/mmu
// ./gradeit.sh /home/ym2360/os/lab2/lab2_assign/refout /home/ym2360/os/lab2/outputdir

// local--------------------------------------------------------------
// test. Run in lab2
// ./runit.sh /Users/yachiru/Documents/nyu/class/OS/lab2/outputdir /Users/yachiru/Documents/nyu/class/OS/lab2/sched
// ./gradeit.sh /Users/yachiru/Documents/nyu/class/OS/lab2/lab2_assign/refout /Users/yachiru/Documents/nyu/class/OS/lab2/outputdir

// RR: ./sched -v -s R2 "/Users/yachiru/Documents/nyu/class/OS/lab2/lab2_assign/input2" "/Users/yachiru/Documents/nyu/class/OS/lab2/rfile"  >tmp
// PREPRIO: ./sched -v -s E2:5 "/Users/yachiru/Documents/nyu/class/OS/lab2/lab2_assign/input3" "/Users/yachiru/Documents/nyu/class/OS/lab2/rfile"  >tmp

int quantum=10000;
int maxprios=4;
int CURRENT_TIME=0;

bool vTrack=false;
string svalue;
string inputfile;
string rfile;
string schedulerName;

DES* des;
vector<Process*> total_processes;
int num_processes = 0;
vector<int> randvals;
SCHEDULER* THE_SCHEDULER;

int ofs=0;
int randCount;

int time_cpubusy=0;
int time_iobusy=0;
int num_IO_duty=0; //keep track of how many process are doing IO, it is concurrent
int time_io_start=0;
int finishtime = 0;

void read_rfile () {
    ifstream read_rfile;
    read_rfile.open(rfile);

    if (!read_rfile.is_open()){
        cout<<"Warning: cannot open "<<rfile<<". Exit!"<<endl;
        exit(1);
    }

    string number;
    getline(read_rfile, number);
    randCount = stoi(number);
    for(int i = 0; i < randCount; i++){
        getline(read_rfile, number);
        randvals.push_back(stoi(number));
    }

    read_rfile.close();
}

int myrandom(int burst) {
    ofs %= randCount-1;
    int ans = 1 + (randvals[ofs] % burst);
    ++ofs;
    return ans; 
}

void read_input() {
    ifstream input;
    input.open(inputfile);
    if(!input.is_open()){
        cout<<"Warning: cannot open "<<inputfile<<". Exit!"<<endl;
        exit(1);
    }

    string linetext;
    int countPid = 0;

    int AT_, TC_, CB_, IO_;
    char buff[100];
    while (input.getline(buff, sizeof(buff))) {
        sscanf(buff, "%d %d %d %d", &AT_, &TC_, &CB_, &IO_);
        int PRIO_ = myrandom(maxprios);
        int PID_ = countPid++;
        Process* process = new Process(AT_, TC_, CB_, IO_, PRIO_, PID_);
        total_processes.push_back(process);

        EVENT* evt = new EVENT(process, STATE_READY);
        evt->evtTimeStamp = AT_;
        evt->old_state = STATE_CREATED;
        des->put_event(evt);
    }

    num_processes = total_processes.size();
}

int get_cpu_burst(Process* proc) {
    if (proc->cpu_burst > 0){
        return proc->cpu_burst;
    }

    int tmp_burst = myrandom(proc->CB);

    if (tmp_burst >= proc->remain_time){
        return proc->remain_time;
    } else return tmp_burst;
}

void Summary() {
    cout<<schedulerName<<endl;
    for (Process* proc : total_processes) {
        printf("%04d: %4d %4d %4d %4d %1d | ", proc->PID, proc->AT, proc->TC, proc->CB, proc->IO, proc->static_prio);
        printf("%5d %5d %5d %5d\n", proc->FT, proc->TT, proc->IT, proc->CW);
    }

    int sumTT=0;
    int sumCW=0;
    for (Process* proc : total_processes) {
        sumTT += proc->TT;
        sumCW += proc->CW;
    }

    double avgTT = sumTT / (double)num_processes;
    double avgCW = sumCW / (double)num_processes;
    
    double cpu_util = 100.0 * (time_cpubusy / (double)finishtime);
    double io_util = 100.0 * (time_iobusy / (double)finishtime);
    double throughput = 100.0 * (num_processes / (double)finishtime );

    printf("SUM: %d %.2lf %.2lf %.2lf %.2lf %.3lf\n",
        finishtime, 
        cpu_util, 
        io_util, 
        avgTT, 
        avgCW, 
        throughput
    );
}

void simulation() {
    EVENT* evt;
    bool CALL_SCHEDULER = false;
    Process* CURRENT_RUNNING_PROCESS = nullptr;

    while ((evt = des->get_event()) != NULL) {
        Process *proc = evt->evtProcess;
        CURRENT_TIME = evt->evtTimeStamp;
        int transition = evt->new_state;
        int timeInPrevState = CURRENT_TIME - proc->state_ts;

        switch(transition) {
            case STATE_READY:
            {
                // must come from BLOCKED or from PREEMPTION
                // must add to run queue
                CALL_SCHEDULER = true; // conditional on whether something is run
                proc->state_ts = CURRENT_TIME; 

                if(vTrack){
                    if (evt->old_state == STATE_RUNNING) {
                        evt->baseTrack(CURRENT_TIME, timeInPrevState, CPU);
                    } else {
                        evt->baseTrack(CURRENT_TIME, timeInPrevState, NA);
                    }
                }

                if (evt->old_state==STATE_BLOCK) { // calculate IO time
                    proc->IT += timeInPrevState;

                    --num_IO_duty;
                    if(num_IO_duty==0)
                        time_iobusy += CURRENT_TIME - time_io_start; 
                } else if (evt->old_state==STATE_RUNNING && (svalue.at(0)=='P' || svalue.at(0)=='E')) { // update priority of PRIO/PREPRIO
                    (proc->dynamic_prio)--;
                } else {} // do nothing
                
                // proc->state_ts = CURRENT_TIME;             
                THE_SCHEDULER->add_process(proc);

                if (svalue.at(0) == 'E') {
                    if(evt->old_state == STATE_BLOCK || evt->old_state == STATE_CREATED){
                        bool isPreempt = false;
                        int flag = 0;

                        if (CURRENT_RUNNING_PROCESS != nullptr && proc->dynamic_prio > CURRENT_RUNNING_PROCESS->dynamic_prio) {
                            flag = 1;
                            if (des->checkPreemptTime(CURRENT_RUNNING_PROCESS, CURRENT_TIME) != true) {
                                isPreempt = true;
                            }
                        }

                        if(vTrack){
                            if (CURRENT_RUNNING_PROCESS != nullptr) {
                                cout<<"---> PRIO preemption "<<CURRENT_RUNNING_PROCESS->PID<<" by "<<proc->PID<<" ? "<<flag;
                                cout<<" TS="<<CURRENT_RUNNING_PROCESS->future_ts<<" now="<<CURRENT_TIME;

                                if (isPreempt) {
                                    cout<<") --> YES"<<endl;
                                } else {
                                    cout<<") --> NO"<<endl;
                                }
                            }

                            // Update track information if preempt successfully
                            if (isPreempt) {
                                if (evt->old_state == STATE_RUNNING) {
                                    evt->baseTrack(CURRENT_TIME, timeInPrevState, CPU);
                                } else {
                                    evt->baseTrack(CURRENT_TIME, timeInPrevState, NA);
                                }
                            }
                        }

                        if (isPreempt) {
                            int trsnaferTime = CURRENT_RUNNING_PROCESS->future_ts - CURRENT_TIME;

                            // Update information of current process
                            CURRENT_RUNNING_PROCESS->future_ts = CURRENT_TIME;
                            CURRENT_RUNNING_PROCESS->cpu_burst += trsnaferTime;
                            CURRENT_RUNNING_PROCESS->remain_time += trsnaferTime;

                            time_cpubusy -= trsnaferTime;

                            des->rm_event(CURRENT_RUNNING_PROCESS);

                            EVENT *preemptEvt = new EVENT(CURRENT_RUNNING_PROCESS, STATE_PREEMPT);
                            preemptEvt->evtTimeStamp = CURRENT_TIME;
                            des->put_event(preemptEvt);
                            CURRENT_RUNNING_PROCESS = nullptr;
                        }
                    }
                }

                if (CURRENT_RUNNING_PROCESS != nullptr && proc->PID == CURRENT_RUNNING_PROCESS->PID) {
                    CURRENT_RUNNING_PROCESS = nullptr;
                }
            }
            break;
            case STATE_RUNNING:
            {
                int createCB = get_cpu_burst(proc);
                proc->cpu_burst = createCB;
                int validCB; // Actual used cpu_burst
                
                // Compare createCB with quantum
                if (createCB >= quantum) 
                    validCB = quantum;
                else validCB = createCB;
                
                // summary
                proc->CW += CURRENT_TIME - proc->state_ts;
                proc->state_ts = CURRENT_TIME;

                if (vTrack) {
                    evt->baseTrack(CURRENT_TIME, timeInPrevState, CPU);
                }

                // Use validCB instead of createCB
                proc->future_ts = CURRENT_TIME + validCB;
                proc->cpu_burst -= validCB;
                proc->remain_time -= validCB;
                time_cpubusy += validCB;
                
                // Create event for either preemption or blocking
                EVENT* incomingEvt = new EVENT(CURRENT_TIME + validCB, proc);
                incomingEvt->old_state = STATE_RUNNING;

                // RUNNING -> READY : remain_time > 0, cpu_burst > 0
                // RUNNING -> BLOCK : remain_time > 0, cpu_burst = 0
                // RUNNING -> TREMINATE : remain_time = 0
                if (proc->remain_time > 0) { // Still needs to be dealt with
                    if (proc->cpu_burst != 0) { // Has cup_burst to run
                        incomingEvt->new_state = STATE_READY;
                    } else { // Must wait for new resources
                        incomingEvt->new_state = STATE_BLOCK;
                    }
                } else { // Done
                    incomingEvt->new_state = STATE_TERMINATE;
                }

                des->put_event(incomingEvt);
            }
            break;
            case STATE_BLOCK:
            {
                // create event for process becomes ready again
                CALL_SCHEDULER = true;
                CURRENT_RUNNING_PROCESS = nullptr;

                proc->state_ts = CURRENT_TIME;
                // Calculate io_burst. No need to compare with other parameters
                int io_burst = myrandom(proc->IO);
                proc->io_burst = io_burst;

                if(vTrack){
                    evt->baseTrack(CURRENT_TIME, timeInPrevState, IO);
                }

                // BLOCK -> READY
                EVENT* incomingEvt = new EVENT(proc, STATE_READY);
                incomingEvt->evtTimeStamp = CURRENT_TIME + io_burst;
                incomingEvt->old_state = STATE_BLOCK;
                des->put_event(incomingEvt);
                
                // When a process returns from I/O its dynamic priority is reset to (static_priority-1)
                proc->dynamic_prio = proc->static_prio - 1;

                // Calculate IO busy
                if(num_IO_duty==0){
                    time_io_start = CURRENT_TIME;
                }
                ++num_IO_duty;
            }
            break;
            case STATE_PREEMPT:
            {   // add to runqueue (no event is generated)
                CURRENT_RUNNING_PROCESS = nullptr;
                CALL_SCHEDULER = true;
                
                proc->state_ts = CURRENT_TIME;
                --(proc->dynamic_prio);
                THE_SCHEDULER->add_process(proc);
            }
            break;
            case STATE_TERMINATE:
            {
                if(vTrack){
                    evt->terminateTrack(CURRENT_TIME, timeInPrevState);
                }
                CURRENT_RUNNING_PROCESS=nullptr;
                CALL_SCHEDULER=true;
                proc->termiate(CURRENT_TIME);
            }
            default:
                break;
        }

        if (CALL_SCHEDULER) {
            if (des->get_next_event_time() == CURRENT_TIME){
                continue; // process next event from Event queue
            }

            CALL_SCHEDULER = false; // reset global flag

            if(CURRENT_RUNNING_PROCESS == nullptr){
                // THE_SCHEDULER->printSched();
                CURRENT_RUNNING_PROCESS = THE_SCHEDULER->get_next_process();
                // THE_SCHEDULER->printSched();
                if (CURRENT_RUNNING_PROCESS==nullptr){
                    continue;
                }

                // create event to make this process runnable for same time
                EVENT* incomingEvt = new EVENT(CURRENT_RUNNING_PROCESS, STATE_RUNNING);
                incomingEvt->evtTimeStamp = CURRENT_TIME;
                incomingEvt->old_state = STATE_READY;
                des->put_event(incomingEvt);
            }
            
        }
        delete evt;
        evt = nullptr;
    }

    finishtime = CURRENT_TIME;
}

int main(int argc, char** argv){
    // reference : https://www.gnu.org/software/libc/manual/html_node/Example-of-Getopt.html
    int c;
    while((c=getopt(argc, argv, "vs:")) != -1){
        switch (c)
        {
        case 'v':
            vTrack=true;
            break;
        case 's':
            svalue = string(optarg);
            if (svalue=="F"){
                    schedulerName = "FCFS";
                    THE_SCHEDULER = new FCFS();
                }
                else if (svalue=="L"){
                    schedulerName = "LCFS";
                    THE_SCHEDULER = new LCFS();
                }
                else if (svalue=="S"){
                    schedulerName = "SRTF";
                    THE_SCHEDULER = new SRTF();
                }
                else if (svalue.at(0)=='R'){
                    schedulerName = "RR ";
                    sscanf(svalue.c_str(), "R%d", &quantum);
                    schedulerName += to_string(quantum);
                    THE_SCHEDULER = new ROUNDROBIN(quantum);
                }
                else if (svalue.at(0)=='P'){
                    if (svalue.find(":") == string::npos){
                        sscanf(svalue.c_str(), "P%d", &quantum);
                    }
                    else{
                        sscanf(svalue.c_str(), "P%d:%d", &quantum, &maxprios);
                    }
                    schedulerName = "PRIO " + to_string(quantum);                   
                    THE_SCHEDULER = new PRIO_X(quantum, maxprios);
                }
                else if (svalue.at(0)=='E'){
                    if (svalue.find(":") == string::npos){
                        sscanf(svalue.c_str(), "E%d", &quantum);
                    }
                    else{
                        sscanf(svalue.c_str(), "E%d:%d", &quantum, &maxprios);
                    }
                    schedulerName = "PREPRIO " + to_string(quantum); 
                    THE_SCHEDULER = new PRIO_X(quantum, maxprios);
                }
                else{
                    cout<<"Unknown scheduler. Exit!"<<endl;
                    exit(-1);
                }
            break;
        case '?':
            if (optopt == 's')
                fprintf (stderr, "Option -%c requires an argument.\n", optopt);
            else if (isprint (optopt))
                fprintf (stderr, "Unknown option `-%c'.\n", optopt);
            else
                fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
            exit(1);
        default:
            abort();
        }
    }

    des = new DES();
    char* inputfile_tmp = argv[argc-2];
    char* rfile_tmp = argv[argc-1];
    inputfile = string(inputfile_tmp);
    rfile = string(rfile_tmp);

    read_rfile();
    read_input();
    simulation();
    Summary();
    return 0;
}