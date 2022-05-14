#include <string>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include "IO.h"

using namespace std;

// server----------------------------------------
// ssh nyuID@access.cims.nyu.edu
// password: *******
// ssh nyuID@linserv1.cims.nyu.edu
// scp lab4_assign.zip nyuID@access.cims.nyu.edu:/home/nyuID/os/lab
// unzip -o -d /home/nyuID/os/lab4 lab4_assign.zip
// ./runit.sh /home/nyuID/os/lab4/testOutput /home/nyuID/os/lab4/iosched
// ./gradeit.sh /home/nyuID/os/lab4/testOutput /home/nyuID/os/lab4/lab4_assign

// local----------------------------------------
// ./iosched -v -f -q -ss "/Users/yachiru/Documents/nyu/class/OS/lab4/lab4_assign/input1"
// ./runit.sh /Users/yachiru/Documents/nyu/class/OS/lab4/testOutput /Users/yachiru/Documents/nyu/class/OS/lab4/iosched
// ./gradeit.sh /Users/yachiru/Documents/nyu/class/OS/lab4/testOutput /Users/yachiru/Documents/nyu/class/OS/lab4/lab4_assign

string algo;
string inputfile;
int CURRENT_TIME = 0;
int requestCount = 0;

int max_waittime = INT_MIN;
int totalTurnAround = 0;
int totalWaittime = 0;

vector<Inst*> instructions;
IOSched* scheduler;

void readInputfile () {
    ifstream input;
    input.open(inputfile);

    if (!input.is_open()) {
        cout<<"Cannot open input file. Exit!"<<endl;
        std::exit(1);
    }

    string line;
    int countIns = 0;

    while (getline(input, line)) {
        if (line[0] != '#') {
            stringstream linestream(line);
            int arriveTime, trackNo;
            linestream >> arriveTime  >> trackNo;
            Inst* ins = new Inst(arriveTime, trackNo);
            ins->id = countIns++;
            instructions.push_back(ins);
        }
    }

    input.close();
}

void Sum () {
    for (auto it = instructions.begin(); it != instructions.end(); it++) {
        printf("%5ld: %5d %5d %5d\n",distance(instructions.begin(), it), (*it)->issuedTime, (*it)->startTime, (*it)->endTime);
    }

    int tot_movement = scheduler->moveTime;
    int size = static_cast<int> (instructions.size());
    double avg_turnaround = 1.0 * totalTurnAround / size;
    double avg_waittime = 1.0 * totalWaittime / size;

    printf("SUM: %d %d %.2lf %.2lf %d\n",
            CURRENT_TIME, tot_movement, avg_turnaround, avg_waittime, max_waittime);
}

void simulation () {
    while (true) {
        // cout<<"----- CURRENT_TIME: "<<CURRENT_TIME<<endl;
        // if a new I/O arrived to the system at this current time → add request to IO-queue
        if (requestCount < instructions.size() && instructions[requestCount]->issuedTime == CURRENT_TIME) {
            scheduler->addRequest(instructions[requestCount]);
            ++requestCount;
        }

        // if an IO is active and completed at this time
        if (active_request && active_request->trackNo == head) { // head == trackNo
            // Compute relevant info and store in IO request for final summary
            active_request->endTime = CURRENT_TIME; // end

            int curWaitTime = active_request->startTime - active_request->issuedTime;
            if (curWaitTime > max_waittime) {
                max_waittime = curWaitTime;
            }

            totalWaittime += curWaitTime;
            totalTurnAround += active_request->endTime - active_request->issuedTime;

            active_request = nullptr; // reset
        }

        // if no IO request active now and requests are pending
        if (!active_request && scheduler->isPending()) {
            scheduler->getNextProcess();
            active_request->startTime = CURRENT_TIME;

            while (active_request->trackNo == head) { // Check whether there are finished requests
                // Compute relevant info and store in IO request for final summary
                active_request->endTime = CURRENT_TIME;

                int curWaitTime = active_request->startTime - active_request->issuedTime;
                if (curWaitTime > max_waittime) {
                    max_waittime = curWaitTime;
                }

                totalWaittime += curWaitTime;
                totalTurnAround += active_request->endTime - active_request->issuedTime;

                active_request = nullptr;

                if (scheduler->isPending()){
                    scheduler->getNextProcess();
                    active_request->startTime = CURRENT_TIME;
                } else break;
            }

            if(active_request == nullptr && requestCount == instructions.size()) {
                break;
            }
        } else if (!active_request && !(scheduler->isPending()) && requestCount >= instructions.size()) {
            // if all IO from input file processed
            break;
        }

        // if an IO is active
        if (active_request) {
            scheduler->simulateSeek(); // Move the head by one unit in the direction its going (to simulate seek)
        }

        ++CURRENT_TIME;   //Increment time by 1
    }

    Sum();
}

int main (int argc, char** argv) {
    // Refrence : https://www.gnu.org/software/libc/manual/html_node/Example-of-Getopt.html
    int c;
    while ((c = getopt(argc, argv, "vqfs:")) != -1) {
        switch (c)
        {
        case 'v':
            // elective : log
            break;
        case 'q':
            break;
        case 'f':
            break;
        case 's':
            algo = string(optarg);
            break;
        case '?':
            if (optopt == 's')
                fprintf (stderr, "Option -%c requires an argument.\n", optopt);
            else if (isprint (optopt))
                fprintf (stderr, "Unknown option `-%c'.\n", optopt);
            else
                fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
            std::exit(1);
        default:
            abort();
        }
    }
    
    char* parameter = argv[argc-1];
    inputfile = string(parameter);
    readInputfile();

    if (algo == "i") scheduler = new FIFO();
    else if (algo == "j") scheduler = new SSTF();
    else if (algo == "s") scheduler = new LOOK();
    else if (algo == "c") scheduler = new CLOOK();
    else if (algo == "f") scheduler = new FLOOK();
    else {
        cout<<"Wrong strategy. Exit!"<<endl;
        std::exit(1);
    }

    simulation();

    return 0;
}
