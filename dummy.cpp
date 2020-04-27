#include <iostream>
#include <stdio.h>
#include <vector>
#include <string>
#include <pthread.h>
using namespace std;


int main(){

  int x = 2;

  int y = (x += 2);

  cout << y << endl;

}
