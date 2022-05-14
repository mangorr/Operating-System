#include <iostream>
#include <vector>
#include <queue>
#include <climits>

struct Inst {
    int id;
    int issuedTime;
    int trackNo;
    int startTime;
    int endTime;

    Inst (int time_step, int trackNo_) {
        issuedTime = time_step;
        trackNo = trackNo_;
    }
};

Inst* active_request = nullptr;
int head = 0;

class IOSched {
public:
    virtual bool isPending() = 0;
    virtual void getNextProcess() = 0;
    virtual void simulateSeek() = 0;
    virtual void addRequest (Inst* new_request) = 0;
    int moveTime = 0;
};

// 1. 
// Use queue to keep the order of requests
class FIFO: public IOSched {
public:
    void addRequest (Inst* new_request) {
        IOqueue.push(new_request);
    }

    void getNextProcess() {
        if (IOqueue.empty()) return;
        active_request = IOqueue.front();
        IOqueue.pop();
    }

    bool isPending () {
        if (!IOqueue.empty()) return true;
        else return false;
    }

    void simulateSeek () {
        ++moveTime;
        if(head < active_request->trackNo) ++head;
        else --head;
    }
private:
    std::queue<Inst*> IOqueue;
};

// 2.
// Keep IOqueue ascending and choose the closest one no matter directions
class SSTF: public IOSched {
public:
    void addRequest(Inst* new_request) {
        IOqueue.push_back(new_request);
    }

    void getNextProcess () {
        if (IOqueue.empty()) {
            std::cout << "Cannot get next process."<<std::endl;
            return;
        }

        auto closest = IOqueue.begin();
        int minDis = INT_MAX;
        int curDis = -1;

        for (auto it = IOqueue.begin(); it != IOqueue.end(); ++it) {
            curDis = abs((*it)->trackNo - head);
            if (curDis < minDis) {
                minDis = std::min(curDis, minDis);
                closest = it;
            }
        }

        active_request = (*closest);
        IOqueue.erase(closest);
    }

    bool isPending () {
        if (!IOqueue.empty()) return true;
        else return false;
    }

    void simulateSeek () {
        ++moveTime;
        if(head < active_request->trackNo) ++head;
        else --head;
    }
private:
    std::vector<Inst*> IOqueue;
};

// 3.
// Two directions. Proceed the direction of closest one.
class LOOK: public IOSched {
public:
    void addRequest (Inst* new_request) {
        int pos;
        for (pos = 0; pos < IOqueue.size(); pos++) {
            if (IOqueue[pos]->trackNo > new_request->trackNo) {
                IOqueue.insert(IOqueue.begin() + pos, new_request);
                break;
            }
        }

        if (pos == IOqueue.size()) {
            IOqueue.push_back(new_request);
        }
    }

    void getNextProcess () {
        if (greater) { // go right / bigger
            auto it = IOqueue.begin();
            auto closest = IOqueue.begin(); // In case need to change direction
            int dis = -1;
            int preDis = head;

            for (; it != IOqueue.end(); it++) {
                dis = head - (*it)->trackNo;

                if (dis > 0 && dis < preDis) { // keep order
                    preDis = dis;
                    closest = it;
                    active_request = (*closest);
                } else if (dis <= 0) {
                    active_request = (*it);
                    IOqueue.erase(it);
                    break;
                }
            }

            if (dis > 0) { // for-loop runs till the end
                // 20 30 40 50 50 50, head 60
                // 20 30 40 (50) 50 50
                // active_request is updated by if branch
                IOqueue.erase(closest);
                greater = false;
            }
        } else { // to left / smaller
            auto it = IOqueue.begin();
            auto closest = IOqueue.begin(); // In case need to change direction
            int dis = -1;;
            int preDis = head;

            if (head < IOqueue.front()->trackNo) { // change direction
                greater = true;
                active_request = IOqueue.front();
                IOqueue.erase(IOqueue.begin());
            } else {
                for (; it != IOqueue.end(); it++) {
                    dis = head - (*it)->trackNo;

                    if (dis >= 0 && dis < preDis) {
                        // insert in front of the first same numbers
                        // 30 40 40 40 : insert 35
                        // 30 35 (40) 40 40 
                        preDis = dis;
                        closest = it;
                    } else if (dis < 0){
                        break;
                    }
                }
                active_request = (*closest);
                IOqueue.erase(closest);
            }
        }
    }

    bool isPending () {
        if (!IOqueue.empty()) return true;
        else return false;
    }

    void simulateSeek () {
        ++moveTime;
        if (greater) ++head;
        else --head;
    }

private:
    // true for going bigger / right
    // false for going smaller / left
    bool greater = true;
    std::vector<Inst*> IOqueue;
};

// 4.
// bigger to bound and wrap around
class CLOOK: public IOSched {
public:
    void addRequest (Inst* new_request) {
        int pos;
        for (pos = 0; pos < IOqueue.size(); pos++) {
            if (IOqueue[pos]->trackNo > new_request->trackNo) {
                IOqueue.insert(IOqueue.begin() + pos, new_request);
                break;
            }
        }

        if (pos == IOqueue.size()) {
            IOqueue.push_back(new_request);
        }
    }

    void getNextProcess () {
        int dis = head - IOqueue.back()->trackNo;

        if (dis > 0) { // head is larger than end. wrap around to the begin
            active_request = IOqueue.front();
            IOqueue.erase(IOqueue.begin());
            greater = false;
        } else {
            int  i;
            for (i = 0; i < IOqueue.size(); i++) {
                dis = head - IOqueue[i]->trackNo;
                if (dis <= 0) { // closest satisfying iterator              
                    break;
                }
            }
            active_request = IOqueue[i];
            IOqueue.erase(IOqueue.begin() + i);
            greater = true;
        }
    }

    bool isPending () {
        if (!IOqueue.empty()) return true;
        else return false;
    }

    void simulateSeek () {
        ++moveTime;
        if (greater) ++head;
        else --head;
    }
private:
    // true for going bigger / right
    // false for going smaller / left
    bool greater = true;
    std::vector<Inst*> IOqueue;
};

// 5. Similar with 3 but two queues
class FLOOK: public IOSched {
public:
    void addRequest (Inst* new_request) {
        int pos;
        for (pos = 0; pos < (*addQ).size(); pos++) {
            if ((*addQ)[pos]->trackNo > new_request->trackNo) {
                (*addQ).insert((*addQ).begin() + pos, new_request);
                break;
            }
        }

        if (pos == (*addQ).size()) {
            (*addQ).push_back(new_request);
        }
    }

    void getNextProcess () {
        if ((*activeQ).empty()) {
            // swap active queue and addqueue by pointer
            swap(activeQ, addQ);
        }

        if (greater) { // go right / bigger
            auto it = (*activeQ).begin();
            auto closest = (*activeQ).begin(); // In case need to change direction
            int dis = -1;
            int preDis = head;

            for (; it!= (*activeQ).end(); it++) {
                dis = head - (*it)->trackNo;

                if (dis > 0 && dis < preDis) { // keep order
                    preDis = dis;
                    closest = it;
                    active_request = (*closest);
                } else if (dis <= 0) {
                    active_request = (*it);
                    (*activeQ).erase(it);
                    break;
                }
            }

            if (dis > 0) { // for-loop run till the end
                // 20 30 40 50 50 50, head 60
                // 20 30 40 (50) 50 50
                // active_request is updated by if branch
                (*activeQ).erase(closest);
                greater = false;
            }
        } else { // to left / smaller
            auto it = (*activeQ).begin();
            auto closest = (*activeQ).begin(); // In case need to change direction
            int dis = -1;;
            int preDis = head;

            if (head < (*activeQ).front()->trackNo) { // change direction
                greater = true;
                active_request = (*activeQ).front();
                (*activeQ).erase((*activeQ).begin());
            } else {
                for (; it != (*activeQ).end(); it++) {
                    dis = head - (*it)->trackNo;

                    if (dis >= 0 && dis < preDis) {
                        // insert in front of the first same numbers
                        // 30 40 40 40 : insert 35
                        // 30 35 (40) 40 40 
                        preDis = dis;
                        closest = it;
                    } else if (dis < 0){
                        break;
                    }
                }
                active_request = (*closest);
                (*activeQ).erase(closest);
            }
        }
    }

    bool isPending () {
        // pending if there are requests in activequeue or addqueue
        if(!(*activeQ).empty() || !(*addQ).empty()) return true;
        else return false;
    }

    void simulateSeek () {
        ++moveTime;
        if (greater) ++head;
        else --head;
    }

private:
    bool greater = true;
    std::vector<Inst*> *activeQ = &IOqueue1;
    std::vector<Inst*> *addQ = &IOqueue2;
    std::vector<Inst*> IOqueue1;
    std::vector<Inst*> IOqueue2;
};