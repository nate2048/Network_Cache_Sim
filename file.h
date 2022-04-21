#pragma once

struct file{

    double size;
    int fileNum;

    bool inPQueue, inCache;

    double responseTime, queueTime;
    explicit file(int id);

};
