#!/bin/bash

sudo apt update

sudo apt install python3-pip
pip install numpy
pip install scipy
pip install matplotlib

chmod +x ns/waf
python fairness.py ns validate -p optimized
