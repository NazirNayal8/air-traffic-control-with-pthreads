#include <iostream>
#include <algorithm>
#include <queue>
#include <vector>
#include <cmath>
#include <chrono>
#include <ctime>
#include <cassert>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/time.h>

using namespace std;

// debugger
void __print(int x) {cerr << x;}
void __print(long x) {cerr << x;}
void __print(long long x) {cerr << x;}
void __print(unsigned x) {cerr << x;}
void __print(unsigned long x) {cerr << x;}
void __print(unsigned long long x) {cerr << x;}
void __print(float x) {cerr << x;}
void __print(double x) {cerr << x;}
void __print(long double x) {cerr << x;}
void __print(char x) {cerr << '\'' << x << '\'';}
void __print(const char *x) {cerr << '\"' << x << '\"';}
void __print(const string &x) {cerr << '\"' << x << '\"';}
void __print(bool x) {cerr << (x ? "true" : "false");}
template<typename T, typename V>
void __print(const pair<T, V> &x) {
  cerr << '{'; __print(x.first);
  cerr << ','; __print(x.second); cerr << '}';
}
template<typename T>
void __print(const T &x) {
  int f = 0; cerr << '{';
  for (auto &i : x)
  cerr << (f++ ? "," : ""), __print(i); cerr << "}";
}
void _print() {cerr << "]\n";}
template <typename T, typename... V>
void _print(T t, V... v) {
  __print(t);
  if (sizeof...(v))
    cerr << ", "; _print(v...);
}
#ifndef ONLINE_JUDGE
#define debug(x...) cerr << "[" << #x << "] = ["; _print(x)
#else
#define debug(x...)
#endif

// -----------------------------------------------------------------------------

#define LANDING 0
#define LEAVING 1
#define EMERGENCY 2
#define MAX_PLANES 1000000
const int t = 1;

struct Plane {
  int ID;
  string arrival_time;
  int type;

  Plane(int _ID, string _arrival_time, int _type){
    ID = _ID;
    arrival_time = _arrival_time;
    type = _type;
  }
};

queue<Plane> LandingQueue;
queue<Plane> LeavingQueue;

pthread_mutex_t LandingQueueMutex;
pthread_mutex_t LeavingQueueMutex;
pthread_mutex_t EmergencyQueueMutex;
pthread_mutex_t IDMutex;

vector<pthread_cond_t> Conditions(MAX_PLANES);
vector<pthread_mutex_t> Locks(MAX_PLANES);

int S;
int N;
double P;

std::chrono::_V2::system_clock::time_point start_time;

int pthread_sleep (int seconds) {
  pthread_mutex_t mutex;
  pthread_cond_t conditionvar;
  struct timespec timetoexpire;
  if(pthread_mutex_init(&mutex,NULL)) {
    return -1;
  }
  if(pthread_cond_init(&conditionvar,NULL)) {
    return -1;
  }
  struct timeval tp;
  // When to expire is an absolute time, so get the current time and add //it to our delay time
  gettimeofday(&tp, NULL);
  timetoexpire.tv_sec = tp.tv_sec + seconds;
  timetoexpire.tv_nsec = tp.tv_usec * 1000;

   pthread_mutex_lock (&mutex);
   int res =  pthread_cond_timedwait(&conditionvar, &mutex, &timetoexpire);
   pthread_mutex_unlock (&mutex);

   pthread_mutex_destroy(&mutex);
   pthread_cond_destroy(&conditionvar);
   //Upon successful completion, a value of zero shall be returned
   return res;
}


int LandingID = 2;
int LeavingID = 1;

int GenerateID(int type){

  assert(type == LANDING || type == LEAVING);
  int ID;

  pthread_mutex_lock(&IDMutex);
  ID = (type == LANDING) ? LandingID++ : LeavingID++;
  pthread_mutex_unlock(&IDMutex);

  pthread_mutex_init(&Locks[ID], NULL);
  pthread_cond_init(&Conditions[ID], NULL);

  return ID;
}

string GetCurrentTime(){
  auto current_time = chrono::system_clock::now();
  time_t Time = std::chrono::system_clock::to_time_t(current_time);
  return ctime(&Time);
}

bool DurationIsValid(){
  auto current_time = chrono::system_clock::now();
  chrono::duration<double> Duration = current_time - start_time;

  return Duration.count() <= S;
}

void InitMutex(){
  pthread_mutex_init(&LandingQueueMutex, NULL);
  pthread_mutex_init(&LeavingQueueMutex, NULL);
  pthread_mutex_init(&EmergencyQueueMutex, NULL);
  pthread_mutex_init(&IDMutex, NULL);
}

bool ProcessLanding() {
  bool process = false;
  pthread_mutex_lock(&LandingQueueMutex);
  if(!LandingQueue.empty()){ debug("Found a Landing Plane");
    process = true;
    debug(LandingQueue.size());
    Plane plane = LandingQueue.front();
    LandingQueue.pop();

    pthread_mutex_lock(&Locks[plane.ID]);
    pthread_cond_signal(&Conditions[plane.ID]);
    pthread_mutex_unlock(&Locks[plane.ID]);

    pthread_sleep(2 * t);
  }
  pthread_mutex_unlock(&LandingQueueMutex);

  return process;
}

void* ATC(void *ptr){

  debug("Start of ATC");

  while(DurationIsValid()) {
    if(ProcessLanding()){
      debug("Landing Happened");
      continue;
    }
  }

  debug("End of ATC");
  pthread_exit(NULL);
}

void AddLandingPlane(Plane &plane){

  pthread_mutex_lock(&LandingQueueMutex);
  LandingQueue.push(plane);
  pthread_mutex_unlock(&LandingQueueMutex);
}

void AddLeavingPlane(Plane &plane){

  pthread_mutex_lock(&LeavingQueueMutex);
  LeavingQueue.push(plane);
  pthread_mutex_unlock(&LeavingQueueMutex);
}

void* LeavingRequest(void *ptr){

  debug("start of leaving request");

  string cur_time = GetCurrentTime();

  int ID = GenerateID(LEAVING);

  Plane plane = Plane(ID, cur_time, LEAVING);

  AddLeavingPlane(plane);

}

void* LandingRequest(void *ptr){

  debug("start of landing request");

  string cur_time = GetCurrentTime();

  int ID = GenerateID(LANDING);

  Plane plane = Plane(ID, cur_time, LANDING);

  AddLandingPlane(plane);

  pthread_mutex_lock(&Locks[plane.ID]);
  pthread_cond_wait(&Conditions[plane.ID], &Locks[plane.ID]);
  pthread_mutex_unlock(&Locks[plane.ID]);

  debug("End of landing request");

  pthread_exit(NULL);
}


int main(int argc, char *argv[]) {

  S = atoi(argv[2]);
  P = atof(argv[4]);
  N = atoi(argv[6]);

  string _S = string(argv[1]);
  string _P = string(argv[3]);
  string _N = string(argv[5]);

  assert(_S == "-s");
  assert(_P == "-p");
  assert(_N == "-n");

  InitMutex();

  start_time = chrono::system_clock::now();

  pthread_t atc, land[MAX_PLANES], idx = 0;

  int dummy = 1, err;

  err = pthread_create(&atc, NULL, ATC, (void *) dummy);

  while(DurationIsValid()){

    err = pthread_create(&land[idx++], NULL, LandingRequest, (void*) dummy);

    pthread_sleep(t);
  }

  void* status;

  puts("Finished");
  return 0;
}
