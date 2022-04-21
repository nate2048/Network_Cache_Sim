#include "event.h"
#include "file.h"
#include "ll_cache.h"
#include <iostream>
#include <gsl/gsl_randist.h>
#include <gsl/gsl_rng.h>
#include <vector>
#include <random>
#include <map>
#include <queue>
#include <numeric>
#include <fstream>
#include <iomanip>
using namespace std;

//  All times in Seconds; All sizes in MB

int N = 100000;  // # files in Origin Servers
double globalTime = 0;
double maxTime = 1000;

//  Default Parameters
double lambda = 100;  // # file requests / second
float R_a = 2;  // in-bound bandwidth
float R_c = 125;  // institution network bandwidth
float aF_s = 1.01;  // shape Param for F_s
float shapeF_p = 1.1; // used for Pareto shape Param for F_p
float minF_p = 0.005;  // used for Pareto min Param for F_p
float C = 500; // storage capacity
double meanDelay = 0.5;  // used for Params for log-normal D
double sdDelay = 0.4;  // used for Params for log-normal D

gsl_rng* randGen;

//  Data Structures
map<int, file*> fileInfo; // file information with key being "name"
queue<int> accessQueue;
priority_queue<event*, vector<event*>, priorityTime> eventQueue;
vector<double> responseTimes;
ll_cache* cache;

//  Main Loop
double Run_Sim(int replPolicy);

//  Events: 1 - new request,  2 - arrive at queue,  3 - depart queue,  4 - file received
void New_Event(int type, int fileNum, double time);
void Event_1(int fileNum, discrete_distribution<double>& p_i);
void Event_2(int fileNum);
void Event_3(int fileNum, int type);
void Event_4(int fileNum);

//  Cache Types
void LPF_Cache(file& f);
void Large_Cache(file& f);
void FIFO(file& f);

//  Construct Random Variates
double New_Req_Time();
discrete_distribution<double> Construct_File_Selector();
int New_Request(discrete_distribution<double>& p_i);
double Gen_S_i();
double Gen_LogNorm_D();

//  Input Methods
void Single_Run();
void Sample_Run(ofstream& output);
void Parameter_List(bool select);
void Display_Result(double avg, int type);
void reset();


int main() {

    ofstream output("graph_data/C2.csv");

    int selection;
    cout << "\nWelcome to the Network Cache Simulator!\n\nHow would you like to proceed - " << endl;
    cout << "1. Single Run to Test Parameters" << endl;
    cout << "2. Run a Sample (Response Time vs Parameter)\n" << endl;
    cout << "Selection: ";
    cin >> selection;

    if(selection == 1){ Single_Run(); }
    if(selection == 2){ Sample_Run(output); }

}



double Run_Sim(int replPolicy){

    //  For Random Number Generation
    randGen = gsl_rng_alloc(gsl_rng_default); //  initializing randGen
    discrete_distribution<double> p_i = Construct_File_Selector(); //  for p_i values to aid file selection

    cache = new ll_cache(C); //  initializing cache

    New_Event(1, New_Request(p_i), 0); //  starts program (user request webpage)

    int fileNum, eventType;
    int numEvents = 0;
    while(globalTime < maxTime){

        event* curEvent = eventQueue.top();
        fileNum = curEvent->fileNum;
        eventType = curEvent->type;

        globalTime = curEvent->time;

        if(eventType == 1){ Event_1(fileNum, p_i); }
        if(eventType == 2){ Event_2(fileNum); }
        //  replPolicy:  1 - Least Popular first  2 - Largest files first  3 - FIFO
        if(eventType == 3){ Event_3(fileNum, replPolicy); }
        if(eventType == 4){ Event_4(fileNum); }

        eventQueue.pop();
        ++numEvents;
    }

    unsigned int numFilesRec = responseTimes.size();
    double avg = accumulate(responseTimes.begin(), responseTimes.end(), 0.0) / numFilesRec;

    return avg;
}



/*-------------------------------------------------EVENTS--------------------------------------------------*/

void New_Event(int type, int fileNum, double eventTime){

    double queueTime = globalTime + eventTime; //  event time is time required for one event
    auto* newEvent = new event(type, fileNum, queueTime);
    eventQueue.push(newEvent); //  sorted by queue time
}

//  New Request file Event
void Event_1(int fileNum, discrete_distribution<double>& p_i){

    double time;
    if(fileInfo.count(fileNum) == 0){
        file* newFile = new file(fileNum);
        fileInfo[fileNum] = newFile;
        newFile->size = Gen_S_i();
    }

    file* curFile = fileInfo[fileNum];
    curFile->inPQueue = true;

    if(curFile->inCache){
        time = curFile->size/R_c;
        New_Event(4, fileNum, time);
    }
    else{
        time = Gen_LogNorm_D();
        New_Event(2, fileNum, time);
    }

    curFile->responseTime = time; //  Begins to account for response time of new request

    //  Prevents a file currently being process to be requested again
    int fileNotInPQ;
    while(true){
        fileNotInPQ = New_Request(p_i);
        if(fileInfo.count(fileNotInPQ) == 0 || (!fileInfo[fileNotInPQ]->inPQueue)){
            break;
        }
    }

    New_Event(1, fileNotInPQ, New_Req_Time());
}

//  Arrive at Queue Event
void Event_2(int fileNum){

    file* curFile = fileInfo[fileNum];
    accessQueue.push(fileNum);
    curFile->queueTime = globalTime; //  Starts "clock" to calculate time in accessQueue

    //  Case where queue is empty, (current file only file in queue)
    //  Queue build up when multiple Event 2s occur before an Event 3
    if(accessQueue.size() == 1){
        double time = curFile->size/R_a;
        New_Event(3, fileNum, time);
        curFile->responseTime += time;
    }
}

//  Depart-Queue Event
void Event_3(int fileNum, int type){

    double time;
    file* curFile = fileInfo[fileNum];

    if(type == 1){ LPF_Cache(*curFile); }
    if(type == 2){ Large_Cache(*curFile); }
    if(type == 3){ FIFO(*curFile); }

    time = curFile->size/R_c;
    New_Event(4, fileNum, time);
    curFile->responseTime += time;
    accessQueue.pop();

    //  After a file is remove from queue, a new event is created to continue to dequeue more files
    if(!accessQueue.empty()){

        int nextDeq = accessQueue.front();
        file* deqFile = fileInfo[nextDeq];

        double accessTime = deqFile->size/R_a;
        New_Event(3, nextDeq, accessTime);

        //  Accounts for time spent in the access queue
        double delayTime = (globalTime - deqFile->queueTime);
        deqFile->responseTime += (accessTime + delayTime);
    }
}

//  File Received Event
void Event_4(int fileNum){

    file* curFile = fileInfo[fileNum];
    responseTimes.push_back(curFile->responseTime);
    curFile->inPQueue = false; //  now same file can be requested again, when 'true', not possible
}



/*-----------------------------------------------CACHE TYPES-----------------------------------------------*/

//  least popular first to be replaced
void LPF_Cache(file& f){

    //  no need to add file, just reorder cache
    if(f.inCache){

        //  front element goes to rear
        if(cache->front->nodeFile.fileNum == f.fileNum){
            cache->deq();
            cache->enq(f);
        }
        //  if file is currently in the middle of the LL, previous file
        //  points over current file and current file added to rear
        else{
            for(CF* i = cache->front; i; i = i->next){
                if (i->next && i->next->nodeFile.fileNum == f.fileNum){
                    cache->removeNext(i);
                    cache->enq(f);
                    break;
                }
            }
        }

    }
    else{
        cache->enq(f);
        while(cache->full()){
            fileInfo[cache->front->nodeFile.fileNum]->inCache = false;
            cache->deq();
        }
        f.inCache = true;
    }

}

//  largest files first to be replaced
void Large_Cache(file& f){


    if(cache->size == 0){
        cache->enq(f);
        f.inCache = true;
    }
    //  if f is already in cache, previous insertions reorganized cache
    //  appropriately so only have to manage when f isn't in cache
    else if(!f.inCache){

        double largestFile = cache->front->nodeFile.size;
        double smallestFile = cache->rear->nodeFile.size;

        if(f.size < smallestFile){
            cache->enq(f);
        }
        else if(f.size > largestFile){
            if(cache->full())
                return;
            else
                cache->addFront(f);
        }
        else{
            for(CF* i = cache->front; i; i = i->next){

                file cacheFile = i->next->nodeFile;

                if (f.size > cacheFile.size){
                    //  let n = i->next; changes i->n to i->f->n
                    cache->moveNext(i, f);
                    break;
                }
            }
        }

        f.inCache = true;
        while(cache->full()){
            fileInfo[cache->front->nodeFile.fileNum]->inCache = false;
            cache->deq();
        }
    }
}

//  First in First out
void FIFO(file& f){

    cache->enq(f);
    while(cache->full()){
        fileInfo[cache->front->nodeFile.fileNum]->inCache = false;
        cache->deq();
    }
    f.inCache = true;
}



/*--------------------------------------------RANDOM GENERATORS--------------------------------------------*/

double New_Req_Time(){

    double mu = 1/lambda;
    return gsl_ran_exponential (randGen, mu);
}

//  returns (p_1, p_2,..., p_N) used when selecting a file
//  dependent on N draws from pareto(2, 0.005)
discrete_distribution<double> Construct_File_Selector(){

    vector<double> q_j;
    double q_i;
    for(int i = 0; i < N; i++){
        q_i = gsl_ran_pareto(randGen, shapeF_p, minF_p);
        q_j.push_back(q_i);
    }

    //  computes q_i / sum of all qs for each i in {1, ... , N}
    discrete_distribution<double> d(q_j.begin(),q_j.end());

    return d;
}

//  returns file 'name' i from {1,2,...,N} dependent on p_i
int New_Request(discrete_distribution<double>& p_i){

    random_device rd;
    mt19937 gen(rd());  //  for randomly drawing from a custom discrete dist.
    return (int) p_i(gen);
}

//  The mean file size is predetermined as 1 MB. The shape (aF_s) is
//  given as a parameter. The minimum file size (bF_s) is obtained from
//  the equation mu = 1 = (a*b)/(a - 1). Hence, b = (a - 1) / a
double Gen_S_i(){

    double bF_s = (aF_s-1) / aF_s;
    return gsl_ran_pareto(randGen, aF_s, bF_s);
}

//  Let m and s be the mean and standard deviation of D ~ log-norm(a, b)
//  We know m = exp{a + (b^2 / 2)} and s = m * sqrt{ exp(b^2) -1}
//  Solving for a and b we get a = (log(m) - (b^2 / 2)) and b = sqrt{log(s/m)^2 + 1}
double Gen_LogNorm_D(){

    double m = meanDelay;
    double s = sdDelay;

    double b = sqrt(log(pow(s/m, 2)  + 1));
    double a = log(m) - (pow(b, 2) / 2);
    return gsl_ran_lognormal(randGen, a, b);
}



/*-----------------------------------------------INPUT VALUES-----------------------------------------------*/

void Single_Run(){

    cout << "\nWhat parameters would you like to change?\n" << endl;
    Parameter_List(true);

    int i, type;
    float newParam;
    cout << "\nEnter Parameter Number to Change or 0 to run";
    while(true) {
        cout << "\nParameter: ";
        cin >> i;
        if(i == 0){ break; }
        cout << "Change to: ";
        cin >> newParam;
        if(i == 1){ lambda = newParam; }
        if(i == 2){ R_a = newParam/8; }
        if(i == 3){ R_c = newParam/8; }
        if(i == 4){ aF_s = newParam; }
        if(i == 5){ shapeF_p = newParam; }
        if(i == 6){ minF_p = newParam; }
        if(i == 7){ C = newParam; }
        if(i == 8){ meanDelay = newParam; }
        if(i == 9){ sdDelay = newParam; }
    }

    cout << "\nWhat Cache Replacement Method" << endl;
    cout << "0. No Cache\n1. Least Popular First\n2. Largest First\n3. FIFO" << endl;
    cout << "Selection: ";
    cin >> type;

    Display_Result(Run_Sim(type), type);
}

void Sample_Run(ofstream& output){

    int testParam;
    cout << "\nWhat parameters would you like to test?\n" << endl;
    cout << "1. File Request / Second" << endl;
    cout << "2. Shape of F_s" << endl;
    cout << "3. Cache Storage Capacity" << endl;
    cout << "4. Network Bandwidth" << endl;
    cout << "5. shape of F_p" << endl;
    cout << "\nSelection: ";
    cin >> testParam;

    if(output.is_open()) {
        maxTime = 500;

        int replacement;
        if(testParam == 1){ lambda = 10; }
        if(testParam == 2){ aF_s = 1.01; }
        if(testParam == 3){ C = 20; }
        if(testParam == 4){ R_c = 20; }
        if(testParam == 5){ R_c = 1.01; }


        for (int i = 0; i < 20; i += 1) {

            if(testParam == 1){ output << lambda; }
            if(testParam == 2){ output << aF_s; }
            if(testParam == 3){ output << C; }
            if(testParam == 4){ output << R_c; }

            replacement = 1;
            do {
                output << ", ";
                output << Run_Sim(replacement);
                reset(); //  reinitialize variables
                ++replacement;
            } while (replacement < 4);

            output << "\n";

            if (testParam == 1) { lambda += 6; }
            if (testParam == 2) { aF_s += 0.05; }
            if(testParam == 3){ C += 20; }
            if(testParam == 4){ R_c += 15; }
            if(testParam == 5){ R_c += .07; }



        }

    }
}

void Parameter_List(bool select){
    vector<string> num;
    for(int i = 0; i < 9; i++){
        if(select){ num.push_back(to_string(i+ 1) + ".");}
        else {num.emplace_back("");}
    }
    cout << num[0] << setw(27) << right << "File Request / Second = " << lambda << endl;
    cout << num[1] << setw(27) << right  << "In Bound Bandwidth = " << R_a * 8 << " Mbps" << endl;
    cout << num[2] << setw(27) << right  << "Network Bandwidth = " << R_c * 8 << " Mbps" << endl;
    cout << num[3] << setw(27) << right  << "Shape of F_s = " << aF_s << endl;
    cout << num[4] << setw(27) << right << "Shape of F_p = " << shapeF_p << endl;
    cout << num[5] << setw(27) << right << "Minimum of F_p = " << minF_p << endl;
    cout << num[6] << setw(27) << right << "Cache Storage Capacity = "   << C << " MB"<< endl;
    cout << num[7] << setw(27) << right << "Mean Round Trip Time = " << meanDelay << " s" <<endl;
    cout << num[8] << setw(27) << right << "SD Round Trip Time =  " << sdDelay << endl;
}

void Display_Result(double avg, int type){

    unsigned int numFile =  responseTimes.size();
    string h;
    if(type == 0){h = " No Cache";}
    if(type == 1){h = " Least Popular First";}
    if(type == 2){h = " Largest First";}
    if(type == 3){h = " FIFO";}
    cout << "\nUnder the cache replacement policy:" << h << endl;
    cout << "Average response time for " << numFile << " files is " << avg << endl;

    cout << "\nParameter List:" << endl;
    cout << setw(27) << right << "Files (N) = " << N << endl;
    cout << setw(27) << right << "Max Time = " << maxTime << " s" << endl;
    Parameter_List(false);
}

void reset(){
    globalTime = 0;
    fileInfo.clear();
    responseTimes.clear();
    eventQueue = {};
    accessQueue = {};
}