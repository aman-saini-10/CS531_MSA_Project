import os
import pandas as pd

# Define the folder path containing your text files
folder_path = "/home/waqar/MappingTableWork/mappingWrokStats/buckets/"

# Define the order in which files should be appended
file_order = ['LRUusr_1.txt', 'predicted_usr_1.txt', 
              'LRUhm_0.txt', 'LRUproj_3.txt', 'LRUstg_0.txt', 
              'LRUprn_1.txt', 'LRUprxy_1.txt', 'LRUts_0.txt', 'LRUweb_0.txt',
              'predicted_hm_0.txt', 'predicted_proj_3.txt', 'predicted_stg_0.txt', 
              'predicted_prn_1.txt', 'predicted_prxy_1.txt', 'predicted_ts_0.txt', 
              'predicted_web_0.txt']

# Initialize an empty DataFrame to store the combined data
combined_data = pd.DataFrame()

# Iterate through files in the defined order
for filename in file_order:
    file_path = os.path.join(folder_path, filename)
    
    if os.path.exists(file_path):
        # Read the data from the text file into a DataFrame
        df = pd.read_csv(file_path, dtype=int)

        # Add a MultiIndex header with the filename
        df.columns = pd.MultiIndex.from_product([[filename], df.columns])

        # Concatenate the data horizontally
        combined_data = pd.concat([combined_data, df], axis=1)

# Define the output CSV file path
output_csv_path = 'BucketoutputDeadOnArrival.csv'

# Write the combined data to a CSV file
combined_data.to_csv(output_csv_path, index=False)

print(f"Combined data saved to '{output_csv_path}'")
