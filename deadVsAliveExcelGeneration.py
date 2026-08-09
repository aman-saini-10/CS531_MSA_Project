import os
import pandas as pd

def extract_data_from_file(file_path):
    with open(file_path, 'r') as file:
        data = file.readlines()

    file_data = {
        'Filename': os.path.basename(file_path),  # Store filename as the first column
        'DeadPagePercentage': '',  # Add a new key for DeadPagePercentage
    }

    capture_values = False  # Flag to indicate whether to capture values

    for line in data:
        line = line.strip()  # Remove leading/trailing whitespaces

        if line.startswith('DeadPagePercentage'):
            # Start capturing values from the next line
            capture_values = True
            continue

        if capture_values and line.startswith((' ', '\t')):
            # If line starts with whitespace, capture the values
            values_str = line.strip()
            values = values_str.split()
            file_data['DeadPagePercentage'] += ' '.join(values) + ' '  # Concatenate the values

        elif capture_values:
            # If the line doesn't start with whitespace, stop capturing values
            capture_values = False

    return file_data

# Rest of the script remains unchanged

# Example usage
if __name__ == "__main__":
    working_directory = '/home/waqar/MappingTableWork/mappingWrokStats/hcftl/DeadLRUVsLRU/Comparion2/'  # Update this with your desired working directory path
    output_file = '/home/waqar/MappingTableWork/mappingWrokStats/hcftl/DeadLRUVsLRU/Comparion2/DeadPagePercentage.csv'  # Update this with your desired output file path

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
