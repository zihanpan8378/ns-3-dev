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