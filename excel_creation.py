import os
import pandas as pd
import re

def extract_data_from_file(file_path):
    with open(file_path, 'r') as file:
        data = file.readlines()

    file_data = {}
    for line in data:
        if line.startswith('VictimBlockSelctionPolicy:'):
            file_data['VictimBlockSelctionPolicy'] = line.strip().replace('VictimBlockSelctionPolicy:', '').strip()
        elif line.startswith('TraceFile:'):
            file_data['TraceFile'] = os.path.basename(line.strip().replace('Stats for the Trace:', '').strip())
        elif line.startswith('Total physical blocks'):
            file_data['Total physical blocks'] = line.strip().replace('Total physical blocks', '').strip()
        elif line.find('Capacity') != -1:
            match = re.search(r'Capacity\s+(\d+)', line)
            if match:
                value = match.group(1)
                file_data['Capacity'] = value
        elif line.find('GhostCacheSize') != -1:
            match = re.search(r'GhostCacheSize\s+(\d+)', line)
            if match:
                value = match.group(1)
                file_data['GhostCacheSize'] = value
        # Extract the new fields
        elif 'DirtyPage.Updates' in line:
            file_data['DirtyPage.Updates'] = line.strip().split()[-1]
        elif line.startswith('Fill Ratio'):
            file_data['Fill Ratio'] = line.strip().split()[-1]
        elif line.startswith('GCThrushold'):
            file_data['GCThrushold'] = line.strip().split()[-1]
        elif line.startswith('Total.Evictions'):
            file_data['Total.Evictions'] = line.strip().split()[-1]
        elif line.startswith('GhostCacheTotalEvictions'):
            file_data['GhostCacheTotalEvictions'] = line.strip().split()[-1]
        elif line.startswith('GhostCacheDirtyEvictions'):
            file_data['GhostCacheDirtyEvictions'] = line.strip().split()[-1]
        elif line.startswith('Total.DirtyEvictions'):
            file_data['Total.DirtyEvictions'] = line.strip().split()[-1]
        elif line.startswith('TotalFtlReadRequests'):
            file_data['TotalFtlReadRequests'] = line.strip().split()[-1]
        elif line.startswith('TotalFtlWriteRequests'):
            file_data['TotalFtlWriteRequests'] = line.strip().split()[-1]
        elif line.startswith('pageMappingFoundForReads'):
            file_data['pageMappingFoundForReads'] = line.strip().split()[-1]
        elif line.startswith('pageMappingFoundForWrites'):
            file_data['pageMappingFoundForWrites'] = line.strip().split()[-1]
        elif line.startswith('TotalReadReqestMainCache'):
            file_data['TotalReadReqestMainCache'] = line.strip().split()[-1]
        elif line.startswith('TotalWriteReqestMainCache'):
            file_data['TotalWriteReqestMainCache'] = line.strip().split()[-1]
        elif line.startswith('ReadHitsInMainCache'):
            file_data['ReadHitsInMainCache'] = line.strip().split()[-1]
        elif line.startswith('WriteHitsInMainCache'):
            file_data['WriteHitsInMainCache'] = line.strip().split()[-1]
        elif line.startswith('TotalWriteReqestGhostCache'):
            file_data['TotalWriteReqestGhostCache'] = line.strip().split()[-1]
        elif line.startswith('GhostCacheWriteHits'):
            file_data['GhostCacheWriteHits'] = line.strip().split()[-1]    
        elif line.startswith('PagesServedFromGhostCacheReads'):
            file_data['PagesServedFromGhostCacheReads'] = line.strip().split()[-1]
        elif line.startswith('PagesServedFromGhostCacheWrites'):
            file_data['PagesServedFromGhostCacheWrites'] = line.strip().split()[-1]
        elif line.startswith('prefetcherActivationCounter'):
            file_data['prefetcherActivationCounter'] = line.strip().split()[-1]     
        elif line.startswith('PagesCopied.'):
            file_data['PagesCopied.to.Cache.From.Block'] = line.strip().split()[-1]
        elif line.startswith('PageEvictedByGcAgainAccessed'):
            file_data['PageEvictedByGcAgainAccessed'] = line.strip().split()[-1]
        elif line.startswith('pageEvictedByHostAgainAccessed'):
            file_data['pageEvictedByHostAgainAccessed'] = line.strip().split()[-1]
        elif line.startswith('cachedpagesfoundInAllBlocks'):
            file_data['cachedpagesfoundInAllBlocks'] = line.strip().split()[-1]
        elif line.startswith('BlockToBlockMovement'):
            file_data['BlockToBlockMovement'] = line.strip().split()[-1]
        elif line.startswith('timeSpendOnGc'):
            file_data['timeSpendOnGc'] = line.strip().split()[-1]
        elif line.startswith('TimeSpendOnReads'):
            file_data['TimeSpendOnReads'] = line.strip().split()[-1]
        elif line.startswith('TimeSpendOnWrites'):
            file_data['TimeSpendOnWrites'] = line.strip().split()[-1]
        elif line.startswith('Simulation Tick (ps):'):
            file_data['Simulation Tick (ps)'] = line.strip().replace('Simulation Tick (ps):', '').strip()
        elif line.startswith('Host time duration (sec):'):
            file_data['Host time duration (sec)'] = line.strip().replace('Host time duration (sec):', '').strip()
        elif line.startswith('CacheEvictionPolicy:'):
             file_data['cache_eviction_policy'] = line.strip().replace('CacheEvictionPolicy:', '').strip()
        else:
            line_data = line.strip().split('\t')
            if len(line_data) >= 2:
                key = line_data[0].strip()
                value = line_data[1].strip()
                file_data[key] = value

    return file_data

def main():
    root_folder = '/home/waqar/SimpleSSD-Standalone/pageStateAwareStats/RunToCheckFillGCThrushSensitivity/CachedGCWithOnlyMigratins/FillRatios/'  # Replace with the path to the root folder containing multiple subfolders
    output_file = '/home/waqar/SimpleSSD-Standalone/pageStateAwareStats/RunToCheckFillGCThrushSensitivity/CachedGCWithOnlyMigratins/FillRatios/AllFillAndGcs.csv'

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

