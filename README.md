# air-traffic-control-with-pthreads

A simulation for air traffic control synchronized using pthreads API.


## Building

In order to build the code simply run:

```
make build
```

## Running

Here is an example that runs the code for a simulation duration of 10 seconds, probability of 0.5, and snapshots of waiting queues starting at second 2:

```
./run -s 10 -p 0.5 -n 2
```
