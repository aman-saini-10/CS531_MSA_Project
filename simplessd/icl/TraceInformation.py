total_bytes_accessed = 0
total_operations = 0
total_write_operations = 0
# Read data from file
with open('/home/waqar/Downloads/MSR-Cambridge/MsrTraceFinal/MsrTRace2/hm_0.csv', 'r') as file:
    for line in file:
        fields = line.strip().split(',')
        if len(fields) == 7 and fields[3] == 'Write':
            bytes_accessed = int(fields[6])
            total_bytes_accessed += bytes_accessed
            total_operations += 1
            total_write_operations += 1
if total_operations > 0:
    average_access_length = total_bytes_accessed / total_write_operations
    print(f"The average access length is: {average_access_length:.2f} bytes")
else:
    print("No valid operations found in the trace data.")
