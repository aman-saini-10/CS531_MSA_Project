import os
import pandas as pd

# Define the folder path containing your text files
folder_path = "/home/waqar/MappingTableWork/mappingWrokStats/buckets/"

# Initialize an empty DataFrame to store the combined data
combined_data = pd.DataFrame()

# Iterate through all text files in the folder
for filename in os.listdir(folder_path):
    if filename.endswith(".txt"):
        file_path = os.path.join(folder_path, filename)

        # Read the data from the text file into a DataFrame
        df = pd.read_csv(file_path,dtype=int)
        #df = df.astype(int)

        # Add a MultiIndex header with the filename
        df.columns = pd.MultiIndex.from_product([[filename], df.columns])

        # Concatenate the data horizontally
        df[''] = ''
        combined_data = pd.concat([combined_data, df], axis=1)

# Define the output CSV file path
output_csv_path = 'BucketoutputDeadOnArrival.csv'

# Write the combined data to a CSV file
combined_data.to_csv(output_csv_path, index=False)

print(f"Combined data saved to '{output_csv_path}'")
