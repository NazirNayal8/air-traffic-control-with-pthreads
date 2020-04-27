#include <iostream>
#include <algorithm>
#include <queue>
#include <vector>
#include <cmath>
#include <chrono>
#include <ctime>
#include <random>
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

#define LANDING 0
#define LEAVING 1
#define EMERGENCY 2
#define MAX_PLANES 1000000
const int t = 1;

#define time_point chrono::_V2::system_clock::time_point
#define time_now chrono::system_clock::now
#define to_time_t chrono::system_clock::to_time_t
#define duration chrono::duration

time_point start_time;
int S;
int N;
double P;

string GetCurrentTime(){
  auto current_time = time_now();
  time_t Time = to_time_t(current_time);
  return ctime(&Time);
}

time_point GetCurrentTimePoint(){
  auto current_time = time_now();
  return current_time;
}

double GetElapsedTime() {
  auto cur_time = time_now();
  duration<double> Duration = cur_time - start_time;
  return Duration.count();
}

bool DurationIsValid(){
  auto current_time = time_now();
  duration<double> Duration = current_time - start_time;

  return Duration.count() < S;
}

struct Plane {
  int ID;
  int type;
  double arrival_time;
  double runway_time;
  double turnaround_time;

  Plane(){}
  Plane(int _ID, double _arrival_time, int _type){
    ID = _ID;
    arrival_time = _arrival_time;
    type = _type;

    assert(0 <= arrival_time);
    assert(type == LANDING || type == LEAVING);
  }

  void AppendRunwayTime(){
    runway_time = GetElapsedTime();
    turnaround_time = runway_time - arrival_time;
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


void InitMutex(){
  pthread_mutex_init(&LandingQueueMutex, NULL);
  pthread_mutex_init(&LeavingQueueMutex, NULL);
  pthread_mutex_init(&EmergencyQueueMutex, NULL);
  pthread_mutex_init(&IDMutex, NULL);
}

void InitLog(){

  char entry1[] = "PlaneID";
  char entry2[] = "Status";
  char entry3[] = "Request Time";
  char entry4[] = "Runway Time";
  char entry5[] = "Turnaround Time";

  printf("%10s %7s %15s %15s %15s\n", entry1, entry2, entry3, entry4, entry5);
  puts("---------------------------------------------------------------------");
}

void Log(Plane plane){

  int ID = plane.ID;
  char status = (plane.type == LANDING) ? 'L' : 'D' ;
  double arr_time = plane.arrival_time;
  double run_time = plane.runway_time;
  double trn_time = plane.turnaround_time;

  printf("%10d %7c %15f %15f %15f\n", ID, status, arr_time, run_time, trn_time);
}


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

int LandingID = 0;
int LeavingID = -1;

int GenerateID(int type){

  assert(type == LANDING || type == LEAVING);
  int ID;

  pthread_mutex_lock(&IDMutex);
  ID = (type == LANDING) ? (LandingID += 2) : (LeavingID += 2);
  pthread_mutex_unlock(&IDMutex);

  pthread_mutex_init(&Locks[ID], NULL);
  pthread_cond_init(&Conditions[ID], NULL);

  return ID;
}


bool ProcessLanding() {

  bool process = false;
  Plane plane = Plane();

  pthread_mutex_lock(&LandingQueueMutex);
  if(!LandingQueue.empty()){
    process = true;
    plane = LandingQueue.front();
    LandingQueue.pop();
  }
  pthread_mutex_unlock(&LandingQueueMutex);

  if(process){
    pthread_sleep(2 * t);

    pthread_mutex_lock(&Locks[plane.ID]);
    pthread_cond_signal(&Conditions[plane.ID]);
    pthread_mutex_unlock(&Locks[plane.ID]);
  }

  return process;
}

bool ProcessLeaving() {

  bool process = false;
  Plane plane = Plane();

  pthread_mutex_lock(&LeavingQueueMutex);
  if(!LeavingQueue.empty()){
    process = true;
    plane = LeavingQueue.front();
    LeavingQueue.pop();
  }
  pthread_mutex_unlock(&LeavingQueueMutex);

  if(process){
    pthread_sleep(2 * t);

    pthread_mutex_lock(&Locks[plane.ID]);
    pthread_cond_signal(&Conditions[plane.ID]);
    pthread_mutex_unlock(&Locks[plane.ID]);
  }

  return process;
}

void* ATC(void *ptr){

  while(DurationIsValid()) {
    if(ProcessLanding()){
      debug("Landing Happened");
      continue;
    }

    if(ProcessLeaving()){
      debug("Leaving Happened");
      continue;
    }
  }

  pthread_exit(NULL);
}

void EnqueueLandingPlane(Plane &plane){

  pthread_mutex_lock(&LandingQueueMutex);
  LandingQueue.push(plane);
  pthread_mutex_unlock(&LandingQueueMutex);
}

void EnqueueLeavingPlane(Plane &plane){

  pthread_mutex_lock(&LeavingQueueMutex);
  LeavingQueue.push(plane);
  pthread_mutex_unlock(&LeavingQueueMutex);
}

void* LeavingRequest(void *ptr){

    double arrival_time = GetElapsedTime();

    int ID = GenerateID(LEAVING);

    Plane plane = Plane(ID, arrival_time, LEAVING);

    EnqueueLeavingPlane(plane);

    pthread_mutex_lock(&Locks[plane.ID]);
    pthread_cond_wait(&Conditions[plane.ID], &Locks[plane.ID]);
    pthread_mutex_unlock(&Locks[plane.ID]);

    plane.AppendRunwayTime();

    Log(plane);

    pthread_mutex_destroy(&Locks[plane.ID]);
    pthread_cond_destroy(&Conditions[plane.ID]);

    pthread_exit(NULL);
}

void* LandingRequest(void *ptr){

  double arrival_time = GetElapsedTime();

  int ID = GenerateID(LANDING);

  Plane plane = Plane(ID, arrival_time, LANDING);

  EnqueueLandingPlane(plane);

  pthread_mutex_lock(&Locks[plane.ID]);
  pthread_cond_wait(&Conditions[plane.ID], &Locks[plane.ID]);
  pthread_mutex_unlock(&Locks[plane.ID]);

  plane.AppendRunwayTime();

  Log(plane);

  pthread_mutex_destroy(&Locks[plane.ID]);
  pthread_cond_destroy(&Conditions[plane.ID]);

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
  InitLog();

  start_time = time_now();

  pthread_t atc, plane[MAX_PLANES], idx = 0;

  int dummy = 1, err;

  err = pthread_create(&atc, NULL, ATC, (void *) dummy);

  // use chrono::system_clock::now().time_since_epoch().count(); for random seed
  const unsigned int seed = 1;
  mt19937_64 rng(seed);
  uniform_real_distribution<double> unif(0.0, 1.0);

  while(DurationIsValid()){

    double p = unif(rng);
    debug(p);
    if(p < P)
      err = pthread_create(&plane[idx++], NULL, LandingRequest, (void*) dummy);
    else
      err = pthread_create(&plane[idx++], NULL, LeavingRequest, (void*) dummy);

    pthread_sleep(t);
  }

  void* status;

  pthread_join(atc, &status);

  puts("Finished");
  exit(0);
  return 0;
}
