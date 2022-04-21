#pragma once
#include "file.h"
#include <string>
using namespace std;

struct event {

    double time;
    // 1. new request  2. arrive at queue 3. depart queue 4. file received
    int type;
    int fileNum;

    event(int type, int fileNum, double time);

};

//  used to sort events according to queue time for priority queue structure
struct priorityTime : public binary_function<event*, event*, bool>{
    bool operator()(const event* lhs, const event* rhs) const {
        return lhs->time > rhs->time;
    }
};