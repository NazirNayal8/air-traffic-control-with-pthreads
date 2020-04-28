#include <iostream>
#include <algorithm>
#include <queue>
#include <vector>
#include <cmath>
#include <deque>
#include <chrono>
#include <ctime>
#include <random>
#include <cassert>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/time.h>

using namespace std;

/*
  This code snippet serves as a debugger it can take an arbitrary number of
   arguments in one call, and print to standard error in the format
   [name_of_variable] = [value_of_variable]
   which is quite useful and lightweight for debugging
*/
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
#define MAX_CHAR 10000
#define LANDING_WAIT_THRESHOLD 5
#define LEAVING_WAIT_THRESHOLD 5
#define NORMAL_POLICY 0
#define STARVATION_POLICY 1
#define CROWDED_POLICY 2
#define EMERGENCY_POLICY 3
#define time_point chrono::_V2::system_clock::time_point
#define time_now chrono::system_clock::now
#define to_time_t chrono::system_clock::to_time_t
#define duration chrono::duration

const int t = 1;

time_point start_time; // stores start time of simulation
int S; // stores duration of simulation in seconds
int N; // stores number of seconds after which to start logging waiting planes
double P; // stores probability of Landing Plane arrival

/*
  Returns current time as a string
*/
string GetCurrentTime() {
  auto current_time = time_now();
  time_t Time = to_time_t(current_time);
  return ctime(&Time);
}

/*
  Returns current time as a time_point instance
*/
time_point GetCurrentTimePoint() {
  auto current_time = time_now();
  return current_time;
}

/*
  Returns elapsed time since beginning of simulations
*/
double GetElapsedTime() {
  auto cur_time = time_now();
  duration<double> Duration = cur_time - start_time;
  return Duration.count();
}

/*
  Returns true if simulation duration has not ended yet, or returns false
  otherwise
*/
bool DurationIsValid() {
  auto current_time = time_now();
  duration<double> Duration = current_time - start_time;

  return Duration.count() <= S;
}

/*
  This type represents a plane, it stores the ID of the plane, its type, and
  its arrival, runway, and turnaround time.
*/
struct Plane {
  int ID;
  int type;
  double arrival_time;
  double runway_time;
  double turnaround_time;

  Plane() {}
  Plane(int _ID, double _arrival_time, int _type) {
    ID = _ID;
    arrival_time = _arrival_time;
    type = _type;

    assert(0 <= arrival_time);
    assert(type == LANDING || type == LEAVING || type == EMERGENCY);
  }

  // This function is typically called when the plane has successfully
  // used the runway, it stores the runway time and calculates the
  // turnaround time
  void AppendRunwayTime() {
    runway_time = GetElapsedTime();
    turnaround_time = runway_time - arrival_time;
  }
};

// Every type of planes has its own queue
// I have used a deque instead of a normal queue because I can iterate
// over the elements of a queue more easily
deque<Plane> LandingQueue;
deque<Plane> LeavingQueue;
deque<Plane> EmergencyQueue;

// Every queue has its own mutex for safe access
pthread_mutex_t LandingQueueMutex;
pthread_mutex_t LeavingQueueMutex;
pthread_mutex_t EmergencyQueueMutex;

// This mutex is used to protect variables used to generate IDs
pthread_mutex_t IDMutex;

// Mutexes and condition variables of planes are available to access globally
// because C++ does not support copy by reference easily, so this is to
// guarantee that ATC modifies the same mutexes that are used by the planes.
// The mutex or condition variable belonging to a certain plane can be accessed
// using its  unique ID. For this a maximum number of possible planes per
// simulation is assumed. Of course each mutex or condition variable will
// be initialized when a new ID for a plane is generated.
vector<pthread_cond_t> Conditions(MAX_PLANES);
vector<pthread_mutex_t> Locks(MAX_PLANES);

// this variable will keep the logs and will be printed at the end
string Logs = "";

/*
  Initializes the globally necessary mutexes
*/
void InitMutex() {
  pthread_mutex_init(&LandingQueueMutex, NULL);
  pthread_mutex_init(&LeavingQueueMutex, NULL);
  pthread_mutex_init(&EmergencyQueueMutex, NULL);
  pthread_mutex_init(&IDMutex, NULL);
}

/*
  Initializes the logs variable to define the log columns
*/
void InitLog() {

  char entry1[] = "Plane ID";
  char entry2[] = "Status";
  char entry3[] = "Request Time";
  char entry4[] = "Runway Time";
  char entry5[] = "Turnaround Time";
  char out[MAX_CHAR];
  sprintf(out, "%10s %7s %15s %15s %15s\n", entry1, entry2, entry3, entry4, entry5);
  Logs += string(out);
  sprintf(out, "---------------------------------------------------------------------\n");
  Logs += string(out);
}

/*
  This function takes a plane that has successfully achieved its Request
   and adds its information to logging variable
*/
void Log(Plane plane) {

  int ID = plane.ID;
  char status ;
  if (plane.type == LANDING)
    status = 'L';
  else if (plane.type == LEAVING)
    status = 'D';
  else
    status = 'E';

  double arr_time = plane.arrival_time;
  double run_time = plane.runway_time;
  double trn_time = plane.turnaround_time;
  char out[MAX_CHAR];

  sprintf(out, "%10d %7c %15lf %15lf %15lf\n", ID, status, arr_time, run_time, trn_time);
  Logs += string(out);
}

/*
  This function sleeps a thread for a certain number of seconds
*/
int pthread_sleep (int seconds) {
  pthread_mutex_t mutex;
  pthread_cond_t conditionvar;
  struct timespec timetoexpire;
  if (pthread_mutex_init(&mutex, NULL)) {
    return -1;
  }
  if (pthread_cond_init(&conditionvar, NULL)) {
    return -1;
  }
  struct timeval tp;
  // When to expire is an absolute time, so get the current time and add
  //it to our delay time
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

// these variables are used to assign IDs for planes
int LandingID = 0;
int LeavingID = -1;

/*
  This function takes the type of a newly created plane and assigns an
  ID for it
*/
int GenerateID(int type) {

  // make sure the given type is valid
  assert(type == LANDING || type == LEAVING || type == EMERGENCY);
  int ID;

  pthread_mutex_lock(&IDMutex);
  ID = (type == LANDING || type == EMERGENCY) ? (LandingID += 2) : (LeavingID += 2);
  pthread_mutex_unlock(&IDMutex);

  // initialize the lock  and condition variable of this plane
  pthread_mutex_init(&Locks[ID], NULL);
  pthread_cond_init(&Conditions[ID], NULL);
  return ID;
}

/*
  This function takes the ID of a plane, waits for 2 * t time and then signals
   the condition variable of this plane to terminate it waiting
*/
void Serve(int planeID){

    pthread_sleep(2 * t);
    pthread_mutex_lock(&Locks[planeID]);
    pthread_cond_signal(&Conditions[planeID]);
    pthread_mutex_unlock(&Locks[planeID]);
}

/*
  This function checks if a Landing plane is waiting. If a plane is waiting,
  it is served by removing it from the queue and signalling its condition
  variable
*/
bool HandleLanding() {

  // this value is true if a waiting plane was found
  bool process = false;
  Plane plane = Plane();

  pthread_mutex_lock(&LandingQueueMutex);
  if (!LandingQueue.empty()) {
    process = true;
    plane = LandingQueue.back();
    LandingQueue.pop_back();
  }
  pthread_mutex_unlock(&LandingQueueMutex);

  if(process) Serve(plane.ID);

  return process;
}

/*
  This function checks if a departing or leaving plane is waiting. If a plane
  is waiting, it is served by removing it from the queue and signalling its
  condition variable
*/
bool HandleLeaving() {

  // this value is true if a waiting plane was found
  bool process = false;
  Plane plane = Plane();

  pthread_mutex_lock(&LeavingQueueMutex);
  if (!LeavingQueue.empty()) {
    process = true;
    plane = LeavingQueue.back();
    LeavingQueue.pop_back();
  }
  pthread_mutex_unlock(&LeavingQueueMutex);

  if (process) Serve(plane.ID);

  return process;
}

/*
  This function checks if an emergency plane is waiting. If a plane is waiting,
  it is served by removing it from the queue and signalling its condition
  variable
*/
bool HandleEmergency() {

  // this value is true if a waiting plane was found
  bool process = false;
  Plane plane = Plane();

  pthread_mutex_lock(&EmergencyQueueMutex);
  if (!EmergencyQueue.empty()) {
    process = true;
    plane = EmergencyQueue.back();
    EmergencyQueue.pop_back();
  }
  pthread_mutex_unlock(&EmergencyQueueMutex);

  if (process) Serve(plane.ID);

  return process;
}

/*
  This function checks the number of waiting planes on each of the landing
  and leaving queues and according to these values returns the necessary
  policy for handling them:

  EMERGENCY_POLICY: This policy gives extreme priority to emergency planes. If
  an emergency plane is waiting it is served immediately

  NORMAL_POLICY: This policy simply favors the landing planes, so whenever
  there exists landing plane, it is served first regardless of other queues.

  CROWDED_POLICY: This policy takes place when the number of waiting planes
  of both types is greater than a threshold. In this case, in order to avoid
  any type of starvation, as long as this policy remains valid, a leaving plane
  is served for every two landing planes. This slight priority is given to the
  landing planes per the economic value suggested in the prompt.

  STARVATION_POLICY: This policy takes place when the number of leaving planes
  that are waiting crosses a certain threshold, with the landing plane still,
  within their threshold limits. The leaving planes are given priority as long
  as the number of waiting planes is beyond the threshold, or if the
  CROWDED_POLICY takes place.

*/
int GetPolicy() {

  // first check emergency queue
  int urgent = 0;
  pthread_mutex_lock(&EmergencyQueueMutex);
  urgent = EmergencyQueue.size();
  pthread_mutex_unlock(&EmergencyQueueMutex);

  if(urgent) return EMERGENCY_POLICY;

  // count number of waiting planes in landing and leaving queues
  int LandWait = 0;
  int LeavWait = 0;

  pthread_mutex_lock(&LandingQueueMutex);
  LandWait = LandingQueue.size();
  pthread_mutex_unlock(&LandingQueueMutex);

  pthread_mutex_lock(&LandingQueueMutex);
  LeavWait = LeavingQueue.size();
  pthread_mutex_unlock(&LandingQueueMutex);

  if (LEAVING_WAIT_THRESHOLD <= LeavWait && LEAVING_WAIT_THRESHOLD <= LandWait)
    return CROWDED_POLICY;

  if (LEAVING_WAIT_THRESHOLD <= LeavWait)
    return STARVATION_POLICY;

  return NORMAL_POLICY;
}

// this variable is used to help in executing the CROWDED_POLICY
int crowded_state = 0;

/*
  crowded state variable is resetted when a policy other than the crowded
  policy is executed
*/
void ResetCrowded() {
  crowded_state = 0;
}

/*
  This function handles the crowded policy. It serves either leaving or
  landing planes according to the value of the state.

  crowded_state = 0 -> serve landing
  crowded_state = 1 -> serve landing
  crowded_state = 2 -> serve leaving

  Every time it is incremented we take modulo 3 to ensure this executes
  cyclically.
*/
void HandleCrowded() {
  if (crowded_state == 2) {
    HandleLeaving();
  } else {
    HandleLanding();
  }
  crowded_state = (crowded_state + 1) % 3;
}

/*
  This function represent the ATC thread, it keeps handling plane requests
  as long as the duration of the simulation has not finished yet.
*/
void* ATC(void *ptr) {

  while (DurationIsValid()) {

    int policy = GetPolicy();

    if (policy == CROWDED_POLICY) {
      HandleCrowded();
      continue;
    } else {
      ResetCrowded();
    }

    if (policy == EMERGENCY_POLICY) {
      HandleEmergency();
      continue;
    }

    if (policy == STARVATION_POLICY) {
      HandleLeaving();
      continue;
    }

    // execute normal policy
    if (HandleLanding()) {
      debug("Landing Happened");
      continue;
    }
    if (HandleLeaving()) {
      debug("Leaving Happened");
      continue;
    }
  }

  pthread_exit(NULL);
}

/*
  This function adds a plane to the leaving queue
*/
void EnqueueLeavingPlane(Plane &plane) {

  pthread_mutex_lock(&LeavingQueueMutex);
  LeavingQueue.push_front(plane);
  pthread_mutex_unlock(&LeavingQueueMutex);
}

/*
  This function adds a plane to the landing queue
*/
void EnqueueLandingPlane(Plane &plane) {

  pthread_mutex_lock(&LandingQueueMutex);
  LandingQueue.push_front(plane);
  pthread_mutex_unlock(&LandingQueueMutex);
}

/*
  This function adds a plane to the emergency queue
*/
void EnqueueEmergencyPlane(Plane &plane) {

  pthread_mutex_lock(&EmergencyQueueMutex);
  EmergencyQueue.push_front(plane);
  pthread_mutex_unlock(&EmergencyQueueMutex);
}

/*
  This function allows a plane to wait on the condition variable, and after
  the condition variable is signalled, its time variables' values are
  finalized, and its information is logged
*/
void AwaitResponse(Plane &plane) {

  pthread_mutex_lock(&Locks[plane.ID]);
  pthread_cond_wait(&Conditions[plane.ID], &Locks[plane.ID]);
  pthread_mutex_unlock(&Locks[plane.ID]);

  plane.AppendRunwayTime();
  Log(plane);
}

/*
  This function represents a leaving plane thread, it initializes the plane,
  adds it to the queue, and then waits to be served
*/
void* LeavingRequest(void *ptr) {

  double arrival_time = GetElapsedTime();

  int ID = GenerateID(LEAVING);

  Plane plane = Plane(ID, arrival_time, LEAVING);

  EnqueueLeavingPlane(plane);

  AwaitResponse(plane);

  pthread_mutex_destroy(&Locks[plane.ID]);
  pthread_cond_destroy(&Conditions[plane.ID]);

  pthread_exit(NULL);
}

/*
  This function represents a landing plane thread, it initializes the plane,
  adds it to the queue, and then waits to be served
*/
void* LandingRequest(void *ptr) {

  double arrival_time = GetElapsedTime();

  int ID = GenerateID(LANDING);

  Plane plane = Plane(ID, arrival_time, LANDING);

  EnqueueLandingPlane(plane);

  AwaitResponse(plane);

  pthread_mutex_destroy(&Locks[plane.ID]);
  pthread_cond_destroy(&Conditions[plane.ID]);

  pthread_exit(NULL);
}

/*
  This function represents an emergency plane thread, it initializes the plane,
  adds it to the queue, and then waits to be served
*/
void* EmergencyRequest(void *ptr) {

  double arrival_time = GetElapsedTime();

  int ID = GenerateID(EMERGENCY);

  Plane plane = Plane(ID, arrival_time, EMERGENCY);

  EnqueueEmergencyPlane(plane);

  AwaitResponse(plane);

  pthread_mutex_destroy(&Locks[plane.ID]);
  pthread_cond_destroy(&Conditions[plane.ID]);

  pthread_exit(NULL);
}

/*
  This function represents the thread that is responsible for creating
  emergency planes. It starts at the beginning of the simulation and creates
  an emergency plane every 40t seconds
*/
void* EmergencyFlights(void *ptr) {

  pthread_sleep(40 * t);

  while (DurationIsValid()) {

    pthread_t thread;
    int dummy = 1;
    int err = pthread_create(&thread, NULL, EmergencyRequest, (void*) dummy);
    pthread_sleep(40 * t);
  }

  pthread_exit(NULL);
}

/*
  This function returns a list of the IDs of the landing planes that are
  currently waiting
*/
vector<int> GetLandingWait() {

  vector<int> Wait;
  pthread_mutex_lock(&LandingQueueMutex);
  for (auto &plane : LandingQueue) {
    Wait.push_back(plane.ID);
  }
  pthread_mutex_unlock(&LandingQueueMutex);

  return Wait;
}

/*
  This function returns a list of the IDs of the leaving planes that are
  currently waiting
*/
vector<int> GetLeavingWait() {

  vector<int> Wait;
  pthread_mutex_lock(&LeavingQueueMutex);
  for (auto &plane : LeavingQueue) {
    Wait.push_back(plane.ID);
  }
  pthread_mutex_unlock(&LeavingQueueMutex);

  return Wait;
}

/*
  This function represents the thread that is responsible for printing
  snapshots of waiting planes every second starting from the nth second,
  where n is a paremter that is passed by command line to the program
*/
void* WaitingSnapshot(void *ptr) {

  while (DurationIsValid()) {

    vector<int> LandingWait = GetLandingWait();
    vector<int> LeavingWait = GetLeavingWait();

    printf("Waiting Snapshot at time %lf:\n", GetElapsedTime());

    printf("\t Planes Waiting to Land: \n");
    for (auto &plane : LandingWait) printf("\t\t- ID : %d\n", plane);

    if (LandingWait.empty()) {
      printf("\t\t- NONE\n");
    }

    printf("\t Planes Waiting to Leave: \n");
    for (auto &plane : LeavingWait) printf("\t\t- ID : %d\n", plane);

    if (LeavingWait.empty()) {
      printf("\t\t- NONE\n");
    }

    pthread_sleep(t);
  }

  pthread_exit(NULL);
}


int main(int argc, char *argv[]) {

  // parse commands line arguments
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

  // store beginning time point of the simulation
  start_time = time_now();

  // define necessary threads
  pthread_t atc, plane[MAX_PLANES], snapshot, urgent;
  int dummy = 1, err, idx = 0;

  // create atc thread and emergency threads
  err = pthread_create(&atc, NULL, ATC, (void *) dummy);
  err = pthread_create(&urgent, NULL, EmergencyFlights, (void*) dummy);

  // this is the random number generator
  // use chrono::system_clock::now().time_since_epoch().count(); for random seed
  const unsigned int seed = 1;
  mt19937_64 rng(seed);
  uniform_real_distribution<double> unif(0.0, 1.0);

  // turns true when the the snapshot thread is unleashed
  bool snapshot_started = false;

  while (DurationIsValid()) {

    double p = unif(rng);
    debug(p);
    if (p < P)
      err = pthread_create(&plane[idx++], NULL, LandingRequest, (void*) dummy);
    else
      err = pthread_create(&plane[idx++], NULL, LeavingRequest, (void*) dummy);

    // when time for snapshot output arrives create the thread
    if (!snapshot_started && GetElapsedTime() > N) {
      snapshot_started = true;
      err = pthread_create(&snapshot, NULL, WaitingSnapshot, (void*) dummy);
    }

    pthread_sleep(t);
  }

  void* status;

  pthread_join(atc, &status);

  cout << Logs << endl;

  exit(0);
  return 0;
}
