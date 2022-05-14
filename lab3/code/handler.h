#include "pager.h"

#include <iostream>
#include <vector>
#include <queue>
#include <string>

class MMU { // handler
public:
    unsigned long instNO = 0;
    int max_frame_size;
    std::queue<int> freeFrames;
    Pager* THE_PAGER;

    MMU (int maxSize) {
        max_frame_size = maxSize;
    }

    void initPager (std::string method, std::vector<int>& random_numbers, std::vector<Process>* processes_ptr) {
        if (method=="a") {
            THE_PAGER = new AGEPAGE(processes_ptr);
        } else if (method=="c") {
            THE_PAGER = new CLOCKPAGE(processes_ptr);
        } else if (method=="e") {
            THE_PAGER = new NRUPAGE(processes_ptr);
        } else if (method=="f") {
            THE_PAGER = new FIFOPAGE();
        } else if (method=="r") {
            THE_PAGER = new RANDOMPAGE(random_numbers);
        } else if (method=="w") {
            THE_PAGER = new WSPAGE(processes_ptr);
        } else {
            std::cout << "Wrong type of Pager. Exit!" << std::endl;
            std::exit(1);
        }
    }

    void initPageTable (Process* newProcess) { // Initiate page_table
        for (int i = 0; i < MAX_VPAGES; i++) {
            pte_t PTE = {0, 0, 0, 0, 0, 0, 0, 0};
            newProcess->page_table.push_back(PTE);
        }
    }

    void contextSwitch (Process* newProcess) {
        if (newProcess->page_table.size() == 0) {
            initPageTable(newProcess);
        }

        // Initiate page table according to input vmas
        for (int i = 0; i < newProcess->vmaList.size(); i++) {
            for (int j = 0; j < MAX_VPAGES; j++) {
                if (j >= newProcess->vmaList[i].start_vpage && j <= newProcess->vmaList[i].end_vpage) {
                    newProcess->page_table[j].VALID = true;

                    if (newProcess->vmaList[i].write_protected) {
                        newProcess->page_table[j].WRITE_PROTECT = 1;
                    } else {
                        newProcess->page_table[j].WRITE_PROTECT = 0;
                    }

                    if (newProcess->vmaList[i].file_mapped) {
                        newProcess->page_table[j].FILE_MAPPED = 1;
                    } else {
                        newProcess->page_table[j].FILE_MAPPED = 0;
                    }
                }
            }
        }
    }

    void UNMAP (pte_t *pte) {
        pte->PRESENT = false;
        frame_table[pte->FRAME_NUMBER].mapped = false;
        pte->FRAME_NUMBER = 0;
    }

    void MAP (frame_t* frame_, pte_t* pte_, int pid, int vpage, unsigned int insID) {
        frame_->pid = pid;
        frame_->vpage = vpage;
        frame_->lastAccess = insID;
        frame_->age = 0;
        frame_->mapped = true;

        pte_->PRESENT = true;  
        pte_->FRAME_NUMBER = frame_->index;      
    }

    frame_t* allocate_frame_from_free_list () {
        if (frame_table.size() >= max_frame_size && freeFrames.empty()) { // Illegal
            return nullptr;
        } else if (frame_table.size() < max_frame_size) { // Create new frame item in traditional frame table
            // frame_t insertFrame = {.mapped = false, .age = 0, .index = (int)frame_table.size()};
            // frame_t insertFrame = {mapped : false, age : 0, index : (int)frame_table.size()};
            frame_t insertFrame;
            insertFrame.mapped = false;
            insertFrame.age = 0;
            insertFrame.index = (int)frame_table.size();
            frame_table.push_back(insertFrame);
            return &frame_table.back();
        } else if (freeFrames.size() > 0) { // Get from free frame queue
            int frameNo = freeFrames.front();
            freeFrames.pop();
            return &frame_table[frameNo];
        } else return nullptr;
    }

    frame_t *get_frame () {
        frame_t *frame = allocate_frame_from_free_list();
        if (frame == nullptr) frame = THE_PAGER->select_victim_frame(instNO);
        return frame;
    }

    void exit (Process* process_, bool print_detail, int vpage) {
        for (unsigned int i = 0; i < process_->page_table.size(); i++) {
            pte_t* pte = &process_->page_table[i];

            if (pte->PRESENT) {
                // Update freeFrames
                freeFrames.push(pte->FRAME_NUMBER);
                // UNMAP the pte
                UNMAP(pte);

                if (print_detail) {
                    std::cout<<" UNMAP "<<vpage<<":"<<i<<std::endl;
                }

                process_->pstats.unmaps++;

                if (pte->FILE_MAPPED && pte->MODIFIED) {
                    process_->pstats.fouts++;
                    if(print_detail) std::cout<<" FOUT"<<std::endl;
                }

                pte->MODIFIED = 0;
            }

            pte->PAGEDOUT = 0;
        }
    }

    // Log
    void printPageTable (bool pagetableOption, std::vector<Process> processes) {
        if (pagetableOption) {
            for (int i = 0; i<processes.size(); i++) {
                std::cout<<"PT["<<processes[i].pid<<"]:";

                for(int j=0; j<processes[i].page_table.size(); j++){
                    if (processes[i].page_table[j].PRESENT) {
                        std::cout<<" "<<j<<":";

                        if(processes[i].page_table[j].REFERENCED) std::cout<<"R";
                        else std::cout<<"-";

                        if(processes[i].page_table[j].MODIFIED) std::cout<<"M";
                        else std::cout<<"-";

                        if(processes[i].page_table[j].PAGEDOUT) std::cout<<"S";
                        else std::cout<<"-";
                    } else {
                        if(processes[i].page_table[j].PAGEDOUT)
                            std::cout<<" #";
                        else std::cout<<" *";
                    }
                }
                std::cout<<std::endl;
            }
        }
    }

    void printFrameTable (bool frameTableOption) {
        if (frameTableOption) {
            std::cout<<"FT:";
            /* Doesn't work on service. Overflow
            for (int i = 0; i < max_frame_size; i++) { // max_frame_size frame items need to be printed
                if(frame_table[i].mapped){
                    std::cout<<" "<<frame_table[i].pid<<":"<<frame_table[i].vpage;
                } else std::cout<<" *";
            }
            */
           for (int i = 0; i < frame_table.size(); i++) { // max_frame_size frame items need to be printed
                if(frame_table[i].mapped){
                    std::cout<<" "<<frame_table[i].pid<<":"<<frame_table[i].vpage;
                } else std::cout<<" *";
            }

            if (max_frame_size > frame_table.size()) {
                int count = max_frame_size - frame_table.size();
                while (count) {
                    std::cout<<" *";
                    --count;
                }
            }

            std::cout<<std::endl;
        }
    }
};