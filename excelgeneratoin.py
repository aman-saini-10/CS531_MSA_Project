import os
import pandas as pd

def extract_data_from_file(file_path):
    with open(file_path, 'r') as file:
        data = file.readlines()

    file_data = {
        'Filename': os.path.basename(file_path),  # Store filename as the first column
        'ShadowCacheEviction': '',
        'DeadOnArrivalPredictior': '',
        'misPredictionCount': '',
        'CmtEntresLoadedInCmt': '',
        'CMTSize': '',
        'ShadowCacheSize': '',
        'cmtReadRequests': '',
        'cmtReadRequestsHits': '',
        'cmtWriteRequests': '',
        'cmtWriteRequestsHits': '',
        'cmtEntriesEvictedAgainRequested': '',
        'deadOnArrivalCounter': '',
        'EvictionsFromCmt': '',
        'EntriesEvictedWithOneAccessCount': '',
        'VictimBlockSelctionPolicy':'',
    }

    for line in data:
        line = line.strip()  # Remove leading/trailing whitespaces

        if line.startswith('cmtReadRequests '):
            file_data['cmtReadRequests'] = line.split()[-1]
        elif line.startswith('CMTSize'):
            file_data['CMTSize'] = line[len('CMTSize'):]  # Capture the entire line after 'CMTSize'
        elif line.startswith('ShadowCacheSize'):
            file_data['ShadowCacheSize'] = line[len('ShadowCacheSize'):]
        elif line.startswith('DeadOnArrivalPredictior'):
            file_data['DeadOnArrivalPredictior'] = line[len('DeadOnArrivalPredictior'):]
        elif line.startswith('misPredictionCount'):
            file_data['misPredictionCount'] = line[len('misPredictionCount'):]
        elif line.startswith('ShadowCacheEviction'):
            file_data['ShadowCacheEviction'] = line[len('ShadowCacheEviction'):]
        elif line.startswith('cmtReadRequestsHits '):
            file_data['cmtReadRequestsHits'] = line.split()[-1]
        elif line.startswith('cmtWriteRequests '):
            file_data['cmtWriteRequests'] = line.split()[-1]
        elif line.startswith('cmtWriteRequestsHits '):
            file_data['cmtWriteRequestsHits'] = line.split()[-1]
        elif line.startswith('cmtEntriesEvictedAgainRequested '):
            file_data['cmtEntriesEvictedAgainRequested'] = line.split()[-1]
        elif line.startswith('deadOnArrivalCounter '):
            file_data['deadOnArrivalCounter'] = line.split()[-1]
        elif line.startswith('EvictionsFromCmt '):
            file_data['EvictionsFromCmt'] = line.split()[-1]
        elif line.startswith('EntriesEvictedWithOneAccessCount '):
            file_data['EntriesEvictedWithOneAccessCount'] = line.split()[-1]
        elif line.startswith('CmtEntresLoadedInCmt '):
            file_data['CmtEntresLoadedInCmt'] = line.split()[-1]
        elif line.startswith('VictimBlockSelctionPolicy'):
            file_data['VictimBlockSelctionPolicy'] = line.split()[-1]

    return file_data

def main(working_directory):
    output_file = '/home/waqar/MappingTableWork/mappingWrokStats/hcftl/DeadLRUVsLRU/Comparison3/LRUVsDEadOnArrivalComparion3.csv'  # Update this with your desired output file path

    all_file_data = []

    # Set the current working directory
    os.chdir(working_directory)

    # Loop through subfolders in the root folder
    for foldername in os.listdir():
        folder_path = os.path.join(working_directory, foldername)
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
    working_directory = '/home/waqar/MappingTableWork/mappingWrokStats/hcftl/DeadLRUVsLRU/Comparison3/'  # Update this with your desired working directory path
    main(working_directory)
