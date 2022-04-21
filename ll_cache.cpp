#include "ll_cache.h"

ll_cache::ll_cache(float C) {

    front = nullptr;
    rear = nullptr;
    maxCapacity = C;
    size = 0;
    files = 0;
}

void ll_cache::enq(file& file) {

    CF* addFile = new CF(file);
    size += file.size;

    //for adding first element
    if(rear == nullptr || front == nullptr){
        front = addFile;
        rear = addFile;
        return;
    }

    rear->next = addFile;
    rear = addFile;
    ++files;
}

void ll_cache::deq() {

    if(front == nullptr)
        return;

    size -= front->nodeFile.size;

    CF* temp = front;
    front = front->next;

    delete(temp);
    --files;
}

void ll_cache::removeNext(CF *prev) {

    CF* duplicate = prev->next;
    if(duplicate == rear)
        rear = prev;

    //removes node by changing pointer to point "over" deleted node
    prev->next = prev->next->next;

    size -= duplicate->nodeFile.size;

    delete(duplicate);

    //add nodes
    if(files == 1){
        front = prev;
        rear = prev;
    }
    --files;
}

void ll_cache::moveNext(CF *base, file& newNext){

    CF* newFile = new CF(newNext);

    CF* duplicate = base->next;

    base->next = newFile;
    newFile->next = duplicate;

    size += newNext.size;
    ++files;
}

void ll_cache::addFront(file& newFront){

    CF* newFile = new CF(newFront);

    size += newFront.size;

    newFile->next = front;
    front = newFile;
    ++files;
}

bool ll_cache::full() const {
    if(size > maxCapacity)
        return true;
    else
        return false;
}