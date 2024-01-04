#!/bin/bash

# 进入 ns 目录
cd ns

# 运行第一个命令
./waf --run "cebinae/dcn1 --re=fifo"

# 运行第二个命令
./waf --run "cebinae/dcn1 --re=fq"

# 返回用户主目录
cd

# 进入 Fairness 目录
cd Fairness

# 运行 Python 脚本
python plot.py tmp_index/dcn1 40000 not_upd_LHR -1