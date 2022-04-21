#include "file.h"

file::file(int id) {

    fileNum = id;
    size = -1;
    inPQueue = false;
    inCache = false;
    responseTime = 0;
    queueTime = 0;

}