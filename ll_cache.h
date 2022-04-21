#pragma once
#include "file.h"

//  node in linked list
struct CF{

    CF* next;
    file nodeFile;

    explicit CF(const file& file) : nodeFile(-1){
        nodeFile = file;
        next = nullptr;
    }
};


struct ll_cache {

    float maxCapacity;
    double size;
    int files;
    CF* front;
    CF* rear;

    explicit ll_cache(float C);

    void enq(file& file);
    void deq();
    void removeNext(CF* prev);
    void moveNext(CF *base, file& newNext);
    void addFront(file& newFront);
    bool full() const;

};