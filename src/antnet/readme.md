## How to build and run
Clone the repo
``` bash
git clone https://github.com/zihanpan8378/ns-3-dev.git
cd ns-3-dev
```

Build the ns3 simulator
``` bash
./ns3 clean
./ns3 configure --enable-python-bindings --enable-examples --enable-tests --build-profile=debug
./ns3 build
```

Check how to set different parameters for running experienment
``` bash
./ns3 run antnet-simple-example -- --PrintHelp
```

Run antnet examples
``` bash
./ns3 run antnet-simple-example -- --option=val

./ns3 run antnet-topology-nxy-example -- --n=6 --x=4 --y=3
```

## Milestone 1
Basic AntNet implementation with point to point channels
- The AntNet paper abstracts the routing problem as nodes and links, so we implemented with a point to point topology.

## Milestone 2
CSMA and Wifi support for AntNet

Fault tolerance add-on #1: active failure detection and pheromone evaporation
- Periodic beacon ant broadcasting
- Sliding window for failure detection
- Pheromone evaporation
- This feature is turned off by default, can be turned on with attribute "ns3::Ipv4AntNetRouting::UseBeaconWindow"

## Milestone 3
Fault tolerance add-on #2: failure info propagation
- Evaporation effectiveness calculation
- Failure info propagation
- This feature is turned off by default, can be turned on with attribute "ns3::Ipv4AntNetRouting::UseFailureMessagePropagation"

## Model Overview
```
src/antnet/
    model/
        ipv4-antnet-routing.h/cc                        # AntNet routing protocol implementation
        ipv4-antnet-routing-table-entry.h/cc            # Routing table entry
        ipv4-antnet-local-traffic-statistics-entry.h/cc # Local traffic statistics table entry
        ant-header.h/cc                                 # Ant packet header definition
        failure-message.h/cc                            # Failure info message definition (used for add-on #2 only)
    helper/
        ipv4-antnet-routing-helper.h                    # Wrapper helper for AntNet routing protocol to be used in example scripts
    examples/
        antnet-topology-nxy-example.cc                  # Script for runing the experiment
        antnet-simple-example.cc                        # Simple example script demonstrating AntNet usage
        antnet-simple-wifi-example.cc                   # Example script demonstrating AntNet usage with CSMA and Wifi nodes
        antnet-paper-example.cc                         # Example script replicating the AntNet paper's setup
        example-helpers.cc                              # Some common functions used in the example scripts
    tests/
        antnet-fowrarding-test.cc                       # Unit test for AntNet packet sending and receiving
        antnet-pheromone-test.cc                        # Unit test for AntNet routing table updates
    log/log_file
        pheromones/node_i.csv                           # Pheromone Table of Node i
    script
        plot_clean.py                                   # Plot script
    visualization
        basic_visualize.py                              # Visual example script
    test                                                # Unit Tests to verify the program
        antnet-forward-ant-test.cc                      
        antnet-pheronmone-test.cc
    data
        at_most_one_node_fail_latency.json              # Aggregated data for one node fail and recover
        latency_vs_size_aggregated.json                 # Aggregated data for latency vs different beacon window size
        latency_vs_threshold_aggregated.json            # Aggregated data for latency vs different beacon notificaiton threshold
        loss_vs_interval_aggregated.json                # Aggregated data for loss rate vs different beacon send interval
        loss_vs_size_aggregated.json                    # Aggregated data for loss rate vs different beacon window size
        mobile_latency.json                             # Aggregated data for mobility vs different beacon window size
        loss_vs_threshold_aggregated.json               # Aggregated data for loss rate vs different beacon notificaiton threshold
        stable_loss_rate.json                           # Aggregated data for latency at a fixed loss rate

```
