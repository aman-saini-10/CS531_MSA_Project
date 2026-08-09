#!/bin/bash

# Set the name of the program executable
#program_name="your_program_name"

# Get a list of input files from the foldertop

input_folder="/home/waqar/Downloads/MSR_SimpleSSD/MSR1/hcftlTraces/"
#input_folder="/home/waqar/Downloads/MSR_SimpleSSD/MSR1/ReadIntensiveTraces/"
input_files=$(ls "$input_folder")
new_eviction_policy="0"
cacheEviction="2"
fill_ratio="0.95"
invalid_ratio="0.0"
gc="0.18"
RamSize="67108864"
# Loop through each input file
for input_file in $input_files; do
    # Update the configuration file with the current input file name
    sed -i "s#^File = .*#File = $input_folder$input_file#" config/sample.cfg
    sed -i "351s@^EvictPolicy = .*@EvictPolicy = $new_eviction_policy@" simplessd/config/sample.cfg
    sed -i "336s@^FillRatio = .*@FillRatio = $fill_ratio@" simplessd/config/sample.cfg
    sed -i "341s@^InvalidPageRatio = .*@InvalidPageRatio = $invalid_ratio@" simplessd/config/sample.cfg
    sed -i "358s@^GCThreshold = .*@GCThreshold = $gc@" simplessd/config/sample.cfg
    sed -i "386s@^CacheSize = .*@CacheSize = $RamSize@" simplessd/config/sample.cfg
    sed -i "421s@^EvictPolicy = .*@EvictPolicy = $cacheEviction@" simplessd/config/sample.cfg
    # Compile the program
    make

    # Run the program in the background
    ./simplessd-standalone ./config/sample.cfg ./simplessd/config/sample.cfg ./result > /home/waqar/MappingTableWork/mappingWrokStats/hcftl/SmallOptimalVsSmallLRU/GraphDataFromDeadOnArrivalCacheState/CMTStateWithDOARemoved/$input_file &

    # Wait for some time before starting the next run (you can adjust this time)
    sleep 2
done

# Wait for all background processes to finishqq
wait

