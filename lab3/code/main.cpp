#include "handler.h"

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

using namespace std;

// server------------------------------------------------------------
// ssh nyuID@access.cims.nyu.edu
// password: *******
// ssh nyuID@linserv1.cims.nyu.edu
// scp lab3_assign.zip nyuID@access.cims.nyu.edu:/home/nyuID/os/lab3
// unzip -o -d /home/nyuID/os/lab3 submit_lab3.zip

// ./runit.sh /home/nyuID/os/lab3/lab3_assign/inputs /home/nyuID/os/lab3/outputdir /home/nyuID/os/lab3/submit/mmu
// ./gradeit.sh /home/nyuID/os/lab3/outputdir /home/nyuID/os/lab3/lab3_assign/refout

// local------------------------------------------------------------
// ./runit.sh /Users/yachiru/Documents/nyu/class/OS/lab3/lab3_assign/inputs /Users/yachiru/Documents/nyu/class/OS/lab3/testoutput /Users/yachiru/Documents/nyu/class/OS/lab3/submit/mmu
// ./gradeit.sh /Users/yachiru/Documents/nyu/class/OS/lab3/testoutput /Users/yachiru/Documents/nyu/class/OS/lab3/lab3_assign/refout

// ./mmu -oOPFS -f16 -af "/Users/yachiru/Documents/nyu/class/OS/lab3/lab3_assign/inputs/in1" "/Users/yachiru/Documents/nyu/class/OS/lab3/lab3_assign/inputs/rfile" > tmp

string inputfile;
string randfile;

int MAX_FRAMES = 128;
int TOTAL_RAND;
vector<int> randvals;

struct instruction {
    char instType;
    int instVal;
};

vector<instruction> instructions;
vector<Process> processes;
string method;
string options = "";

Process* CURRENT_PROCESS = nullptr;
MMU* mmu;

bool oooohOption = false;
bool pagetableOption = false;
bool frameTableOption = false;
bool processStatistics = false;

unsigned long long cost = 0;
unsigned long ctx_switches = 0;
unsigned long process_exits = 0;

char operation;
int vpage;
unsigned int countInst = 0;

void parseFlag (string options) {
    if (!options.empty()) {
        if (options.find('O') != string::npos) {
            oooohOption  = true;
        }
        if (options.find('P') != string::npos) {
            pagetableOption = true;
        }
        if (options.find('F') != string::npos) {
            frameTableOption = true;
        }
        if (options.find('S') != string::npos) {
            processStatistics = true;
        }
    } else return;
}

void Summary () {
    for(int i = 0; i<processes.size(); i++) {
        proc_stats* pstats = &processes[i].pstats;
        printf("PROC[%d]: U=%lu M=%lu I=%lu O=%lu FI=%lu FO=%lu Z=%lu SV=%lu SP=%lu\n",
                     processes[i].pid,
                     pstats->unmaps, pstats->maps, pstats->ins, pstats->outs,
                     pstats->fins, pstats->fouts, pstats->zeros,
                     pstats->segv, pstats->segprot);
        cost += pstats->maps*300 + pstats->unmaps*400 + pstats->ins*3100 + pstats->outs*2700 + 
                    pstats->fins*2800 + pstats->fouts*2400 + pstats->zeros*140 + pstats->segv*340 + pstats->segprot*420;
    }
    printf("TOTALCOST %lu %lu %lu %llu %lu\n",
              instructions.size(), ctx_switches, process_exits, cost, sizeof(pte_t));
}

void read_inputfile () {
    ifstream input;
    input.open(inputfile);

    if (!input.is_open()) {
        cout << "Cannot open input file. Exit!" << endl;
        std::exit(1);
    }

    string line;
    string inputType = "crew";
    int processNum = -1;
    int vmaNum = -1;
    int countProcess = -1;

    while (getline(input, line)) {
        if (line[0] != '#') {
            stringstream linestream(line);
            if (inputType.find(line[0]) != std::string::npos) { // eg. c 0, r 14
                instruction inst;
                linestream >> inst.instType >> inst.instVal;
                instructions.push_back(inst);
            } else if (processNum == -1) { // record number of processes first
                linestream >> processNum;
            } else if (vmaNum == -1) { // record each line of vma
                linestream >> vmaNum;

                Process process;
                process.pid = ++countProcess;
 
                string vmaString;
                for (int time = 0; time < vmaNum; time++) {
                    getline(input, vmaString);
                    VMA vma;
                    stringstream vmaStream(vmaString);
                    vmaStream >> vma.start_vpage >> vma.end_vpage >> vma.write_protected >> vma.file_mapped;
                    process.vmaList.push_back(vma);
                }
                
                processes.push_back(process);
                vmaNum = -1; // reset vmaNum
            } else {
                cerr<<"Unknown line. Exit!"<<line<<endl;
                std::exit(1);
            }
        } else continue; // Start with '#'. Ignore
    }

    input.close();
}

void read_rfile () { // the same with lab2
    ifstream read_rfile;
    read_rfile.open(randfile);

    if (!read_rfile.is_open()) {
        cout<<"Warning: cannot open "<<randfile<<". Exit!"<<endl;
        std::exit(1);
    }

    string number;
    getline(read_rfile, number);
    int totalNum = stoi(number);

    for (int i = 0; i < totalNum; i++) {
        getline(read_rfile, number);
        randvals.push_back(stoi(number));
    }

    read_rfile.close();
}

bool get_next_instruction (char &operation, int &vpage) {
    if (countInst < instructions.size()) {
        operation = instructions[countInst].instType; // c,r,w...
        vpage = instructions[countInst].instVal; // PID || vpage 
        return true;
    } else return false;
}

void simulation() {
    while (get_next_instruction(operation, vpage)) {
        mmu->instNO = countInst;

        if (operation == 'c') {
            // Overall calculating
            cost += 130;
            ++ctx_switches;

            // Log
            if (oooohOption ) {
                cout<<countInst<<": ==> "<<operation<<" "<<vpage<<endl;
            }

            // Set current process and initiate its page table
            CURRENT_PROCESS = &processes[vpage];
            mmu->contextSwitch(CURRENT_PROCESS);
        } else if (operation == 'r' || operation == 'w') {
            ++cost; // read/write (load/store) instructions count as 1

            if (oooohOption) {
                cout<<countInst<<": ==> "<<operation<<" "<<vpage<<endl;
            }

            pte_t *currentPte = &CURRENT_PROCESS->page_table[vpage];
            
            if (!currentPte->PRESENT) {
                if (!currentPte->VALID) { // If not present and valid → pagefault
                    cout<<" SEGV"<<endl;
                    CURRENT_PROCESS->pstats.segv++;
                    ++countInst;
                    continue; // move on to the next instruction
                }

                frame_t* victimFrame = mmu->get_frame(); // select a victim frame to replace
                // Unmap its current user
                if (victimFrame->mapped) {
                    pte_t *replacedPte = &(processes[victimFrame->pid].page_table[victimFrame->vpage]);
                    mmu->UNMAP(replacedPte);
                    processes[victimFrame->pid].pstats.unmaps++;

                    if (oooohOption) {
                        cout<<" UNMAP "<<victimFrame->pid<<":"<<victimFrame->vpage<<endl;
                    }
                    
                    if (replacedPte->MODIFIED && replacedPte->FILE_MAPPED) { // FILE_MAPPED recorded by input
                        processes[victimFrame->pid].pstats.fouts++;
                        if(oooohOption ) cout<<" FOUT"<<endl;
                    } else if (replacedPte->MODIFIED && !replacedPte->FILE_MAPPED) {
                        replacedPte->PAGEDOUT = true;
                        processes[victimFrame->pid].pstats.outs++;
                        if(oooohOption ) cout<<" OUT"<<endl;
                    } else {}
                }

                // Fill frame with proper content of current instruction’s address space (IN, FIN,ZERO)
                if (currentPte->FILE_MAPPED) {
                    CURRENT_PROCESS->pstats.fins++;
                    if(oooohOption ) cout<<" FIN"<<endl;
                } else {
                    if (currentPte->PAGEDOUT) {
                        if(oooohOption ) cout<<" IN"<<endl;
                        CURRENT_PROCESS->pstats.ins++;
                    } else {
                        if(oooohOption ) cout<<" ZERO"<<endl;
                        CURRENT_PROCESS->pstats.zeros++;
                    }
                }
                // Map its new user (MAP)
                mmu->MAP(victimFrame, currentPte, CURRENT_PROCESS->pid, vpage, countInst);
                // Its now ready for use and instruction 
                if (oooohOption )
                    cout<<" MAP "<<currentPte->FRAME_NUMBER<<endl;
                CURRENT_PROCESS->pstats.maps++;
                currentPte->MODIFIED = false; // reset
            }

            currentPte->REFERENCED = true;
            if (operation == 'w') {
                if (currentPte->WRITE_PROTECT) {
                    CURRENT_PROCESS->pstats.segprot++;
                    cout<<" SEGPROT"<<endl;
                } else {
                    currentPte->MODIFIED = true;
                }
            }
        } else if (operation == 'e') {
            cost += 1250;
            ++process_exits;

            if (oooohOption) {
                cout<<countInst<<": ==> "<<operation<<" "<<vpage<<endl;
                cout<<"EXIT current process "<<vpage<<endl;
            }

            Process* exProcess = &processes[vpage];
            mmu->exit(exProcess, oooohOption , vpage);  
        } else {
            cout<<"Wrong instructions. Exit!"<<endl;
            exit(1);
        }

        ++countInst;
    }
}

int main (int argc, char** argv) {
    // Parse arguments. Reference : https://www.gnu.org/software/libc/manual/html_node/Example-of-Getopt.html
    int c;
    while ((c = getopt(argc, argv, "f::a::o::")) != -1) {
        switch (c)
        {
        case 'f':
            MAX_FRAMES = stoi(optarg);
            break;
        case 'a':
            method = optarg;
            break;
        case 'o':
            options = optarg;
            break;
        case '?':
            if (optopt == 'f' || optopt == 'a' || optopt == 'o')
                fprintf (stderr, "Option -%c requires an argument.\n", optopt);
            else if (isprint (optopt))
                fprintf (stderr, "Unknown option `-%c'.\n", optopt);
            else
                fprintf (stderr,
                        "Unknown option character `\\x%x'.\n",
                        optopt);
            std::exit(1);
        default:
            abort();
        }
    }

    parseFlag(options);

    inputfile = string(argv[argc-2]);
    randfile = string(argv[argc-1]);

    read_inputfile();
    read_rfile();

    mmu = new MMU(MAX_FRAMES);
    mmu->initPager(method, randvals, &processes);

    simulation();

    // Log
    mmu->printPageTable(pagetableOption, processes);
    mmu->printFrameTable(frameTableOption);
    if (processStatistics) {
        Summary();
    }

    return 0;
}
