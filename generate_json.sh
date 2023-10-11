#!/bin/bash

#!/bin/bash

# 指定保存JSON文件的目录
output_dir="ns/configs/scale"

# 创建目录（如果不存在）
mkdir -p "$output_dir"

# 循环生成168个JSON文件
for i in {1..168}
do
  # 构建JSON文件名
  json_file="${output_dir}/case${i}.json"

  # 构建result_dir中的目录
  result_dir="tmp_index/"
  result_dir_fifq="/fifo/"
  result_dir_fq="/fq/"

  # flow,TCP
  if (( i <= 20 )); then
    transport_prot0="TcpNewReno"
    transport_prot1="TcpVegas"
  elif (( i <= 40 )); then
    transport_prot0="TcpNewReno"
    transport_prot1="TcpWestwood"
  elif (( i <= 60 )); then
    transport_prot0="TcpNewReno"
    transport_prot1="TcpCubic"
  elif (( i <= 80 )); then
    transport_prot0="TcpVegas"
    transport_prot1="TcpWestwood"
  elif (( i <= 100 )); then
    transport_prot0="TcpVegas"
    transport_prot1="TcpCubic"
  elif (( i <= 120 )); then
    transport_prot0="TcpWestwood"
    transport_prot1="TcpCubic"
  else
    transport_prot0="TcpNewReno"
    transport_prot1="TcpVegas"
    transport_prot2="TcpWestwood"
    transport_prot3="TcpCubic"
  fi

  # 构建JSON内容
  json_content=$(cat <<EOF
{
  "instance_type": "dumbbell_long",
  "batch_params": [
      "queuedisc_type",
      "result_dir"
  ],
  "result_dir": [
      "${result_dir}case${i}${result_dir_fifq}",
      "${result_dir}case${i}${result_dir_fq}"
  ],
  "batch_size": 2,
  "enable_debug": 1,
  "logtcp": 1,
  "enable_stdout": 0,
  "printprogress": 1,    
  "seed": 2022,
  "run": 1205,
  "sim_seconds": 61,
  "app_seconds_start": 0,
  "app_seconds_end": 60,
  "tracing_period_us": 10000000,
  "progress_interval_ms": 1000,
  "delackcount": 1,
  "app_packet_size": 1440,
  "bottleneck_bw": "10Mbps",
  "bottleneck_delay": "1ms",
  "switch_netdev_size": "1p",
  "switch_total_bufsize": "2000p",
  "pool": 1,
  "vdt": "1ns",
  "dt": "134.217728ms",
  "l": "100000ns",
  "p": 1,
  "tau": 0.01,
  "delta_port": 0.01,
  "delta_flow": 0.01,
  "queuedisc_type": [
      "FifoQueueDisc",
      "FqCoDelQueueDisc"   
  ],
  "transport_prot0": "${transport_prot0}",
  "transport_prot1": "${transport_prot1}",     
  "leaf_bw0": "10000Mbps",
  "leaf_bw1": "10000Mbps",
  "leaf_delay0": "1ms",
  "leaf_delay1": "1ms",
  "num_cca": 2,
  "num_cca0": 1,
  "num_cca1": 1,
  "app_seconds_change0": "0,10",
  "app_seconds_change1": "0,10",
  "app_bw0": "10Mbps",
  "app_bw1": "10Mbps",
  "termination_time_ms": 50000,
  "sampling_num": 12

EOF
)

  # flow,TCP 
  if ((i > 120)); then
    json_content+="  \"transport_prot2\": \"TcpNewReno\",\n"
    json_content+="  \"transport_prot3\": \"TcpVegas\",\n"
    json_content+="  \"leaf_bw2\": \"10000Mbps\",\n"
    json_content+="  \"leaf_bw3\": \"10000Mbps\",\n"
    json_content+="  \"leaf_delay2\": \"1ms\",\n"
    json_content+="  \"leaf_delay3\": \"1ms\",\n"
    json_content+="  \"num_cca2\": 1,\n"
    json_content+="  \"num_cca3\": 1,\n"
    json_content+="  \"app_seconds_change2\": \"30,40\",\n"
    json_content+="  \"app_seconds_change3\": \"30,40\",\n"
    json_content+="  \"app_bw2\": \"10Mbps\",\n"
    json_content+="  \"app_bw3\": \"10Mbps\"\n"
  fi

  # 完成JSON内容
  json_content+=$'\n'
  json_content+="}"
  json_content+=$'\n'

  # 将JSON内容写入文件
  echo "$json_content" > "$json_file"

  echo "生成文件: $json_file"
done