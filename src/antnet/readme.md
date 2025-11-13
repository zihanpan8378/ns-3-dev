Clone the repo
``` bash
git clone https://github.com/zihanpan8378/ns-3-dev.git
cd ns-3-dev
```

Make an venv for ns3 and install dependencies (deactivate conda's venv if exist)
```bash
conda deactivate
python3 -m venv .venv && source ~/.venv/bin/activate
pip install -U pip
pip install cppyy==3.1.2
pip install ns3
```

Build the ns3 simulator
``` bash
./ns3 clean
./ns3 configure --enable-python-bindings --enable-examples --enable-tests --build-profile=debug
./ns3 build
```

Run antnet examples
``` bash
./ns3 run antnet-csma-grid
```

AntNet is implemented in `ns-3-dev/src/antnet` with this scructure:
```
src/antnet/
  model/
    antnet-routing-protocol.h       # The AntNet IPv4 routing protocol class and its API.
    antnet-routing-protocol.cc
    ant-headers.h                   # A serializable ns-3 header for forward/backward ant packets.
    ant-headers.cc
    pheromone-table.h               # The pheromone table and delay-statistics interfaces.
    pheromone-table.cc
  helper/
    antnet-helper.h                 # A routing helper to install and configure AntNet on nodes.
    antnet-helper.cc
  examples/
    antnet-csma-chain.cc            # Wired CSMA chain demo .
    antnet-csma-grid.cc             # Wired CSMA 3×3 mesh demo .
```

# Milestones
## Milestone 1 deliverables:
- A structure of AntNet implementation in ns-3 simulator with basic functionalities of sending and receiving ants.
- An example of AntNet on wired chain topology and grid topology.
- (not deliverable) We had tried the existing AntNet implementation on ns-2.34 but it was hard to set up the environment and run the code since ns-2.34 only compiles on GCC 4. So we decided to re-implement AntNet in ns-3 from scratch based on the original paper.
## Milestone 2 deliverables:
- Complete AntNet implementation with features mentioned in the original paper:
  - Ant destination selection based on traffic load.
  - Pheromone decay mechanism.
  - Cycle detection and handling.
- Logging the pheromone table changes over time.
- A configuration file to implement any network topology provided in a configuration file.
- A formulation of the original Antnet algorithm and the two fault tolerance add-ons in the same set of notations (in report.pdf).
## Milestone 3 deliverables:
- Not implemented yet. Should have the implementation of the two fault tolerance add-ons and a wireless example of AntNet on Wi-Fi adhoc network.