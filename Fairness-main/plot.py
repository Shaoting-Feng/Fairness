import pickle
import matplotlib.pyplot as plt
import numpy as np
import argparse

bottleneck_bw = "10Mbps" # requires changes
result = ""
termination_time = 0
total_bytes_sent = {} # the bytes sent by each flow

'''
tag_to_max_send_t = {}
JFI_metric = {}
'''

'''
def find_max_time(receive_file, fq_file):
    global max_time
    with open(receive_file, 'r') as f:
        lines = list(reversed(f.readlines()))
        for line in lines:
            parts = line.split()
            if len(parts) >= 5 and parts[1] == 'SourceIDTag:':
                time = int(parts[0].split('[')[1].strip(']'))
                if time > max_time:
                    max_time = time
            break
    with open(fq_file, 'r') as f:
        lines = list(reversed(f.readlines()))
        for line in lines:
            parts = line.split()
            if len(parts) >= 5 and parts[1] == 'SourceIDTag:':
                time = int(parts[0].split('[')[1].strip(']'))
                if time > max_time:
                    max_time = time
            break
'''

def process_file(input_file, sent_flag=False):
    tag_to_size_sum = {}
    current_size = {}
    previous_time = {}
    time = 0
    with open(input_file, 'r') as f:
        lines = list(f.readlines())
        for line in lines:
            parts = line.split()
            if len(parts) >= 5 and parts[1] == 'SourceIDTag:':
                time = int(parts[0].split('[')[1].strip(']'))
                if time > termination_time:
                    for tag in tag_to_size_sum:
                        for t in range(previous_time[tag] + 1, termination_time + 1):
                            tag_to_size_sum[tag][t] = current_size[tag]
                        if sent_flag:
                            total_bytes_sent[tag] = current_size[tag]
                    break
                else:
                    tag = int(parts[2].strip(','))
                    size = int(parts[4])
                    if tag not in tag_to_size_sum:
                        current_size[tag] = 0
                        previous_time[tag] = 0
                    for t in range(previous_time[tag] + 1, time):
                        tag_to_size_sum[tag][t] = current_size[tag]
                    current_size[tag] += size
                    tag_to_size_sum[tag][time] = current_size[tag]
                    previous_time[tag] = time
    if time <= termination_time:
        for tag in tag_to_size_sum:
            for t in range(previous_time[tag] + 1, termination_time + 1):
                tag_to_size_sum[tag][t] = current_size[tag]
                if sent_flag:
                    total_bytes_sent[tag] = current_size[tag]

    # structure of tag_to_size_sum:
    # tag0: 0 1 1 2 2 2 3
    # tag1: 0 1 3 7 9 9 9
    return tag_to_size_sum 

def plot_tag_data(tag_to_size_sum_sent, tag_to_size_sum_received, tag_to_size_sum_fq, update_lower_headroom, sampling_num):
    fig_num = len(tag_to_size_sum_received)
    axs = plt.subplots(nrows=fig_num, ncols=1, figsize=(8, 3*fig_num))[1]
    data_for_idx = [] # to calculate weighted average

    # sending curve
    for i, (tag, sent) in enumerate(tag_to_size_sum_sent.items()):
        axs[tag].plot(sent, label=f"Tag {tag}" + "_sent", color='black')
        
        # receiving curve
        if tag in tag_to_size_sum_received:
            received = tag_to_size_sum_received[tag]
            axs[tag].plot(received, label=f"Tag {tag}" + "_received", color='orange')
        
        # fq curve
        if tag in tag_to_size_sum_fq:
            fq = tag_to_size_sum_fq[tag]
            axs[tag].plot(fq, label=f"Tag {tag}" + "_fq", color='blue')
        
        axs[tag].set_xlabel("Time (ms)")
        axs[tag].set_ylabel("Byte")
        axs[tag].legend()

        # Calculation for each flow
        deviation = sum(abs(fq - received))
        upper_head = abs(sent - fq)
        if update_lower_headroom:
            lower_head = fq - np.concatenate(([0], received[:termination_time-1]))
        else:
            lower_head = fq
        max_head = [max(a, b) for a, b in zip(lower_head, upper_head)]
        max_headroom = sum(max_head)
        area_idx = deviation / max_headroom

        # Write the result in file
        file_path = "ns/" + result + "/area_idx.txt"
        data_for_idx.append(
            {"tag": tag, "area_idx": area_idx, "bytes_sent": sent[termination_time]}
        )
        if i == 0: # first time to write in file
            write_mode = "w"
        else:
            write_mode = "a"
        with open(file_path, write_mode) as file:
            file.write("The area index for Application " + str(tag) + " is " + str(area_idx) + ".\nApplication " + str(tag) + " has sent " + str(sent[termination_time]) + 
                       " bytes.\nThe deviation for Application " + str(tag) + " is " + str(deviation) + ".\nThe max headroom for Application " + str(tag) + " is " + 
                       str(max_headroom) + ".\n")
            
        '''
        # added on 08.22 to calculate JFI metric for each app
        # throughput for JFI is calculated when the sending behavior is terminated
        JFI_metric[tag1] = cumulative_sum2[tag_to_max_send_t[tag1]] / cumulative_sum1[tag_to_max_send_t[tag1]]
        with open(file_path, "a") as file:
            file.write("Normalised throughput of Application " + str(tag1) + " in the sending period is " + str(JFI_metric[tag1]) + ".\n")
        '''

    # Weigh area_idx for each flow based on total bytes sent
    WAI_numerator = 0
    WAI_denominator = 0
    for data in data_for_idx:
        WAI_numerator = WAI_numerator + data["area_idx"] * data["bytes_sent"]
        WAI_denominator = WAI_denominator + data["bytes_sent"]
    WAI = WAI_numerator / WAI_denominator
    with open(file_path, "a") as file:
        file.write("WAI is " + str(WAI) + ".\n")

    '''
    # added on 08.31 to calculate normalised area index
    NAI_numerator = 0
    NAI_denominator = 0
    for data in data_for_idx:
        NAI_numerator = NAI_numerator + data["deviation"]
        NAI_denominator = NAI_denominator + data["max_headroom"]
    NAI = NAI_numerator / NAI_denominator
    with open(file_path, "a") as file:
        file.write("NAI is " + str(NAI) + ".\n")
    '''

    '''
    # added on 08.22 to calculate JFI
    N = len(JFI_metric)
    sum_xi = sum(JFI_metric.values())
    sum_xi_squared = sum(x**2 for x in JFI_metric.values())
    JFI = (sum_xi**2) / (N * sum_xi_squared)
    with open(file_path, "a") as file:
        file.write("JFI is " + str(JFI) + ".\n")
    '''

    plt.tight_layout()
    plt.savefig("ns/" + result + "/output_chart.svg", format="svg")
    plt.savefig("ns/" + result + "/output_chart.png", format="png")
    plt.show()

def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("result", type=str, help="Path to the result directory")
    parser.add_argument("termination_time", type=str, help="Termination time in ms")
    parser.add_argument("upd_LHR", type=str, help="Whether to update lower_headroom")
    parser.add_argument("sampling_num", type=str, help="How many points to sample for all the flows")
    return parser.parse_args()

def sample(tag_to_size_sum_fq, sampling_num):
    point_num = {}

    total = sum(total_bytes_sent.values())
    for i, (tag, bytes) in enumerate(total_bytes_sent.items()):
        point_num[tag] = (sampling_num * bytes) // total
        

def main():
    global result
    global termination_time
    args = parse_args()
    result = args.result
    termination_time = int(args.termination_time)
    if args.upd_LHR == "upd_LHR":
        upd_LHR = True
    else:
        upd_LHR = False
    sampling_num = int(args.sampling_num)

    tag_to_size_sum_received = process_file("ns/" + result + "/fifo/received_ms.dat")
    tag_to_size_sum_sent = process_file("ns/" + result + "/fifo/sent_ms.dat", True)
    tag_to_size_sum_fq = process_file("ns/" + result + "/fq/received_ms.dat")

    if sampling_num != 0:
        sample(tag_to_size_sum_fq, sampling_num)

    plot_tag_data(tag_to_size_sum_sent, tag_to_size_sum_received, tag_to_size_sum_fq, upd_LHR, sampling_num)

if __name__ == "__main__":
    main()
