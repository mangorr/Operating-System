#include <iostream>
#include <climits>
#include <vector>
#include <queue>

typedef struct {
    unsigned int PRESENT :1; // ‘1’ if the values in this entry is valid, otherwise translation is invalid and a pagefault exception will be raised
    unsigned int  REFERENCED :1; // every time the page is accessed (load or store), the reference bit is set
    unsigned int  MODIFIED :1; // every time the page is written to (store), the modified bit is set
    unsigned int  WRITE_PROTECT :1; // If writeprotect bit is set the page can only be read (load instruction). If attempted write (store instruction), a write protection exception is raised
    unsigned int  PAGEDOUT :1;
    unsigned int  FRAME_NUMBER :7; //  the maximum number of frames is 128, which equals 7 bits
    unsigned int  VALID :1;
    unsigned int  FILE_MAPPED :1;
} pte_t;

typedef struct {
    int pid;
    int vpage;
    int index;
    bool mapped;
    unsigned int age;
    unsigned int lastAccess;
} frame_t;

typedef struct {
    // The same order with print
    unsigned long unmaps;
    unsigned long maps;
    unsigned long ins;
    unsigned long outs;
    unsigned long fins;
    unsigned long fouts;
    unsigned long zeros;
    unsigned long segv;
    unsigned long segprot;
} proc_stats;

struct VMA {
    unsigned int start_vpage;
    unsigned int end_vpage;
    bool write_protected;
    bool file_mapped;
};

const int MAX_VPAGES = 64;
std::vector<frame_t> frame_table;

class Process {
public:
    int pid;
    proc_stats pstats = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    std::vector<VMA> vmaList;
    std::vector<pte_t> page_table{MAX_VPAGES}; // represents the translations from virtual pages to physical frames for that process
};

class Pager {
public:
    virtual frame_t* select_victim_frame(int inst_no) = 0;
};

class FIFOPAGE: public Pager {
public:
    // Actually don't need any parameter. But pure virtual function cannot be overloaded. 
    // So remain inst_no but don't use it.
    frame_t* select_victim_frame(int useless) {
        frame_t* victim =  &frame_table[wrapIndex];
        ++wrapIndex;
        wrapIndex %= frame_table.size();
        return victim;
    }
private:
    int wrapIndex = 0;
};

class RANDOMPAGE: public Pager {
public:
    RANDOMPAGE (std::vector<int> randvals) {
        randoms = randvals;
    }

    int generateRandom (int size) {
        index %= randoms.size();
        ++index;
        return (randoms[index-1] % size); 
    }

    frame_t* select_victim_frame (int inst_no) {
        int randNum = generateRandom(frame_table.size());
        return &frame_table[randNum];
    }

    frame_t* select_victim_frame () {
        int randNum = generateRandom(frame_table.size());
        return &frame_table[randNum];
    }
private:
    int index = 0;
    std::vector<int> randoms;
};

class CLOCKPAGE: public Pager {
public:
    CLOCKPAGE(std::vector<Process>* processesPtr_) {
        processesPtr = processesPtr_;
    }

    frame_t* select_victim_frame (int useless) {
        frame_t* frame = &frame_table[clock];
        int pid = frame->pid;
        int vpage = frame->vpage;
        pte_t* pte = &(processesPtr->at(pid).page_table[vpage]);

        while (pte->REFERENCED) {
            pte->REFERENCED = 0; // Reset
            ++clock;
            clock %= frame_table.size();

            frame = &frame_table[clock];

            pid = frame->pid;
            vpage = frame->vpage;

            pte = &(processesPtr->at(pid).page_table[vpage]); // Update pte
        }

        ++clock;
        clock %= frame_table.size();
        return frame;
    }
private: 
    int clock = 0;
    std::vector<Process>* processesPtr;
};

class NRUPAGE: public Pager {
public:
    NRUPAGE (std::vector<Process>* processesPtr_) {
        processesPtr = processesPtr_;
    }

    frame_t* select_victim_frame (int inst_no);
private:
    int hand = 0;
    int lastAccess = -1;
    std::vector<Process>* processesPtr;
};

frame_t* NRUPAGE::select_victim_frame (int inst_no) {
    std::vector<frame_t*> pagePrio(4, nullptr);
    frame_t* victimFrame = nullptr;

    frame_t* frame;
    pte_t* pte;
    int prioPos;

    for(int i=0; i<frame_table.size(); i++){
        frame = &(frame_table[(i+hand) % frame_table.size()]);
        pte = &(processesPtr->at(frame->pid).page_table[frame->vpage]);

        // Definition of calss
        prioPos = pte->REFERENCED*2 + pte->MODIFIED;

        if (victimFrame==nullptr && prioPos==0) {
            victimFrame = frame;
            pagePrio[0] = frame;
        } 
        
        if (pagePrio[prioPos] == nullptr) pagePrio[prioPos] = frame;
    }

    // Check whether need to update pte
    if (inst_no - lastAccess >= 50) {
        for(unsigned int i=0; i<frame_table.size();i++) {   
            frame_t* frame = &frame_table[i];
            pte_t* pte = &(processesPtr->at(frame->pid).page_table[frame->vpage]);
            pte->REFERENCED=false;
        }    

        lastAccess = inst_no;     
    }

    if (victimFrame == nullptr) {
        for (int i=0; i<4; i++) {
            if (pagePrio[i]) {
                victimFrame = pagePrio[i];
                break;
            }
        }
    }

    // Wrap around
    hand = (victimFrame->index + 1) % frame_table.size();
    return victimFrame;
}

class AGEPAGE: public Pager {
public:
    AGEPAGE (std::vector<Process>* processesPtr_) {
        processesPtr = processesPtr_;
    }
    frame_t* select_victim_frame(int uesless);
private:
    int hand = 0;
    std::vector<Process>* processesPtr;
};

frame_t* AGEPAGE::select_victim_frame (int uesless) {
    unsigned int minAge = UINT_MAX;
    frame_t* victimFrame = nullptr;

    frame_t* frame;
    pte_t* pte;

    for (int i=0; i<frame_table.size(); i++) {
        frame = &frame_table[(i+hand)%frame_table.size()];
        pte = &(processesPtr->at(frame->pid).page_table[frame->vpage]);

        frame->age = frame->age >> 1;

        if (pte->REFERENCED) {
            frame->age = frame->age | 0x80000000;
            pte->REFERENCED = false; // Update
        }

        if (frame->age < minAge) {
            victimFrame = frame;
            minAge = frame->age;
        }
    }

    hand = (victimFrame->index + 1) % frame_table.size();
    return victimFrame;
}

class WSPAGE: public Pager {
public:
    WSPAGE (std::vector<Process>* processesPtr_) {
        processesPtr = processesPtr_;
    }

    frame_t* select_victim_frame(int inst_no);
private:
    unsigned int hand = 0;
    int TAU = 49;
    std::vector<Process>* processesPtr;
};

frame_t* WSPAGE::select_victim_frame (int ints) {
    frame_t* victim_frame = &frame_table[hand % frame_table.size()];
    int oldest_used = ints;
    int maxDiff = INT_MIN;

    frame_t* frame;
    pte_t* pte;

    for (int i=0; i<frame_table.size(); i++) {
        frame = &frame_table[(i+hand)%frame_table.size()];
        pte = &processesPtr->at(frame->pid).page_table[frame->vpage];

        if (pte->REFERENCED) {
            frame->lastAccess = ints;
            pte->REFERENCED = 0;
        } else {
            bool check = (ints - frame->lastAccess) > TAU;

            if (check) {
                victim_frame = frame;
                break;
            } else {
                if (frame->lastAccess < oldest_used) {
                    oldest_used = frame->lastAccess;
                    victim_frame = frame;
                }
            }
        }
    }
    
    hand = (victim_frame->index + 1) % frame_table.size();
    return victim_frame;
}