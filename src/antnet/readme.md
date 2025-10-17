Clone the repo
``` bash
git clone https://github.com/zihanpan8378/ns-3-dev.git
cd ns-3-dev
```

Make an venv for ns3 and install dependencies
```bash
python3 -m venv .venv && source .venv/bin/activate
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
./ns3 run antnet-csma-chain
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
    antnet-wifi-adhoc.cc            # Ad-hoc Wi-Fi multihop demo that exercises AntNet routing.
    antnet-csma-chain.cc            # Wired CSMA chain demo showing multi-subnet, multi-hop learning.
    antnet-csma-mesh.cc             # Wired CSMA 3×3 mesh demo with a slow link to visualize path choice.
```