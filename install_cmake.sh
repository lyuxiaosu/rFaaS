sudo apt-get remove cmake
sudo apt-get install -y build-essential 
sudo apt-get install -y libssl-dev
pushd ~/
wget https://github.com/Kitware/CMake/releases/download/v3.20.0/cmake-3.20.0.tar.gz
tar -xzvf cmake-3.20.0.tar.gz
cd cmake-3.20.0
./bootstrap
make -j
sudo make install
cmake --version
popd
