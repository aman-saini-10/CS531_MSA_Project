import os
import csv

# Specify the folder containing log files
folder_path = '/home/waqar/MappingTableWork/mappingWrokStats/hcftl/DeadLRUVsLRU/Comparison3/lruOnly_65568/'

# Specify the CSV file to write the extracted data
csv_file_path = 'outputDeadPredictoroff.csv'

# Open the CSV file for writing
with open(csv_file_path, 'w', newline='') as csv_file:
    csv_writer = csv.writer(csv_file)

    # Write the header row
    csv_writer.writerow(['Filename1', 'DeadPagePercentage Values'])

    # Iterate through each log file in the folder and its subfolders
    for root, dirs, files in os.walk(folder_path):
        for filename in files:
            if filename.endswith(".txt"):
                log_file_path = os.path.join(root, filename)

                with open(log_file_path, 'r') as log_file:
                    dead_page_percentage_values = []
                    read_next_line = False

                    for line in log_file:
                        if read_next_line:
                            dead_page_percentage_values = line.split()
                            break
                        elif line.startswith('DeadPagePercentage'):
                            read_next_line = True

                    csv_writer.writerow([filename] + dead_page_percentage_values)
