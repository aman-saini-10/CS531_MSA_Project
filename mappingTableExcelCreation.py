import os
import pandas as pd

def extract_data_from_file(file_path):
    with open(file_path, 'r') as file:
        data = file.readlines()

    file_data = {
        'Filename': os.path.basename(file_path)  # Store filename as the first column
    }

    metrics_to_extract = [
        'CMTSize',
        'cmtReadRequests',
        'cmtReadRequestsHits',
        'cmtWriteRequests',
        'cmtWriteRequestsHits',
        'cmtEntriesEvictedAgainRequested',
        'deadOnArrivalCounter',
        'EvictionsFromCmt',
        'EntriesEvictedWithOneAccessCount'
    ]

    for line in data:
        line = line.strip()
        for metric in metrics_to_extract:
            if metric in line:
                # Check for an exact match to avoid partial matching
                if line.startswith(f'{metric}:') or line.startswith(f'{metric} '):
                    file_data[metric] = line.split()[-1]
                else:
                    # Handle partial match by updating the value if it doesn't exist
                    file_data.setdefault(metric, line.split()[-1])

    return file_data

def main():
    root_folder = '/home/waqar/MappingTableWork/mappingWrokStats/hcftl/WithDifferentCmtSizes/'
    output_file = '/home/waqar/MappingTableWork/mappingWrokStats/hcftl/WithDifferentCmtSizes/hcFTLWithAllCMT.csv'

    all_file_data = []

    # Loop through subfolders in the root folder
    for foldername in os.listdir(root_folder):
        folder_path = os.path.join(root_folder, foldername)
        if os.path.isdir(folder_path):
            # Process files in the current subfolder
            for filename in os.listdir(folder_path):
                if filename.endswith('.txt'):
                    file_path = os.path.join(folder_path, filename)
                    file_data = extract_data_from_file(file_path)
                    all_file_data.append(file_data)

    # Create a DataFrame using all_file_data
    df = pd.DataFrame(all_file_data)

    # Save the DataFrame to the CSV file
    df.to_csv(output_file, index=False)

if __name__ == "__main__":
    main()
