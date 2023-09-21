import pickle
import matplotlib.pyplot as plt
import numpy as np
import argparse

bottleneck_bw = "10Mbps" # requires changes
result = ""
termination_time = 0
total_bytes_sent = {} # the bytes sent by each flow
total_bytes_fq = {} # the bytes in fair curve of each flow

def process_file(input_file, sent_flag=False, fq_flag=False):
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
                        elif fq_flag:
                            total_bytes_fq[tag] = current_size[tag]
                    break
                else:
                    tag = int(parts[2].strip(','))
                    size = int(parts[4])
                    if tag not in tag_to_size_sum:
                        current_size[tag] = 0
                        previous_time[tag] = 0
                        tag_to_size_sum[tag] = np.array([0] * (termination_time + 1))
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
                elif fq_flag:
                    total_bytes_fq[tag] = current_size[tag]

    # structure of tag_to_size_sum:
    # tag0: 0 1 1 2 2 2 3
    # tag1: 0 1 3 7 9 9 9
    return tag_to_size_sum 

def plot_tag_data(tag_to_size_sum_sent, tag_to_size_sum_received, tag_to_size_sum_fq, update_lower_headroom, p_p):
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

        bytes_sent = sent[termination_time]

        # Sampling
        if p_p != {}:
            fq = fq[p_p[tag]]
            received = received[p_p[tag]]
            sent = sent[p_p[tag]]

        # Calculation for each flow
        deviation = sum(abs(fq - received))
        upper_head = abs(sent - fq)
        if update_lower_headroom:
            lower_head = fq - np.concatenate(([0], received[:len(received)-1])) # use the received value at the last sampling point as the subtraction 
        else:
            lower_head = fq
        max_head = [max(a, b) for a, b in zip(lower_head, upper_head)]
        max_headroom = sum(max_head)
        area_idx = deviation / max_headroom

        # Write the result in file
        file_path = "ns/" + result + "/area_idx.txt"
        data_for_idx.append(
            {"tag": tag, "area_idx": area_idx, "bytes_sent": bytes_sent}
        )
        if i == 0: # first time to write in file
            write_mode = "w"
        else:
            write_mode = "a"
        with open(file_path, write_mode) as file:
            file.write("The area index for Application " + str(tag) + " is " + str(area_idx) + ".\nApplication " + str(tag) + " has sent " + str(bytes_sent) + 
                       " bytes.\nThe deviation for Application " + str(tag) + " is " + str(deviation) + ".\nThe max headroom for Application " + str(tag) + " is " + 
                       str(max_headroom) + ".\n")

    # Weigh area_idx for each flow based on total bytes sent
    WAI_numerator = 0
    WAI_denominator = 0
    for data in data_for_idx:
        WAI_numerator = WAI_numerator + data["area_idx"] * data["bytes_sent"]
        WAI_denominator = WAI_denominator + data["bytes_sent"]
    WAI = WAI_numerator / WAI_denominator
    with open(file_path, "a") as file:
        file.write("WAI is " + str(WAI) + ".\n")

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
    point_position = {}
    total = sum(total_bytes_sent.values())
    for i, (tag, bytes) in enumerate(total_bytes_sent.items()):
        point_num = int(round((sampling_num*bytes)/total, 0)) # sum of point_num may not equal sampling_num
        threshold = []
        for j in range(1, point_num + 1):
            threshold.append(total_bytes_fq[tag]/j)
        th_count = 0
        point_position[tag] = np.array([0] * point_num)
        for j in range(termination_time+1):
            if tag_to_size_sum_fq[tag][j] >= threshold[th_count]:
                point_position[tag][th_count] = j
                th_count = th_count + 1
                if th_count >= point_num:
                    break    
        '''test'''
        print(tag)
        print(threshold)
        '''end test''' 
    '''test'''
    print(point_position)
    '''end test''' 
    return point_position 

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
    tag_to_size_sum_fq = process_file("ns/" + result + "/fq/received_ms.dat", False, True)

    if sampling_num != 0:
        p_p = sample(tag_to_size_sum_fq, sampling_num)
    else:
        p_p = {}

    plot_tag_data(tag_to_size_sum_sent, tag_to_size_sum_received, tag_to_size_sum_fq, upd_LHR, p_p)

if __name__ == "__main__":
    main()
