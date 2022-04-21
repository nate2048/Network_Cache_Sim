#include "event.h"

event::event(int type, int fileNum, double time) {

    this->time = time;
    this->type = type;
    this->fileNum = fileNum;

}