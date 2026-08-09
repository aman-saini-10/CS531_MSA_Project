#!/bin/bash

# Set the name of the program executable
#program_name="your_program_name"

# Get a list of input files from the foldertop

input_folder="/home/waqar/Downloads/MSR_SimpleSSD/MSR1/hcftlTraces/"
#input_folder="/home/waqar/Downloads/MSR_SimpleSSD/MSR1/ReadIntensiveTraces/" #This run is for the deadPredicitonDetectionActivated
input_files=$(ls "$input_folder")
new_eviction_policy="0"
cacheEviction="2"
fill_ratio="0.95"
invalid_ratio="0.0"
gc="0.05"
RamSize="67108864"
cmt="65536"
dpred="false"
#for cmt in "16392" "32784" "65568" "131136" "262272"; do
for cmt in "65568"; do
    output_folder="/home/waqar/MappingTableWork/mappingWrokStats/hcftl/DeadLRUVsLRU/predictorThrreshold/Pred2_$cmt"
    mkdir -p "$output_folder"

    # Loop through each input file  
    for input_file in $input_files; do
        # Update the configuration file with the current input file name and RamSize
        sed -i "s#^File = .*#File = $input_folder$input_file#" config/sample.cfg
        sed -i "351s@^EvictPolicy = .*@EvictPolicy = $new_eviction_policy@" simplessd/config/sample.cfg
        sed -i "336s@^FillRatio = .*@FillRatio = $fill_ratio@" simplessd/config/sample.cfg
        sed -i "341s@^InvalidPageRatio = .*@InvalidPageRatio = $invalid_ratio@" simplessd/config/sample.cfg
        sed -i "358s@^GCThreshold = .*@GCThreshold = $gc@" simplessd/config/sample.cfg
        sed -i "386s@^CacheSize = .*@CacheSize = $RamSize@" simplessd/config/sample.cfg
        sed -i "421s@^EvictPolicy = .*@EvictPolicy = $cacheEviction@" simplessd/config/sample.cfg
        #sed -i "83s@^optimalreplacementPolicy = .*@optimalreplacementPolicy = $opt ;@" simplessd/ftl/page_mapping.cc
        sed -i "90s/size_t cmtSize = [0-9]\+;/size_t cmtSize = $cmt;/" simplessd/icl/global_point.cc
        sed -i '87s/\(bool useDeadOnArrivalPredictor *= *\)\(false\)/\1true/' simplessd/icl/global_point.cc #flag is changed from true to false
        sed -i '89s/\(bool usePortionOfCacheAsShadow *= *\)\(true\)/\1false/' simplessd/icl/global_point.cc

        #sed -i "s@^bool optimalreplacementPolicy=.*;@bool optimalreplacementPolicy=$optimalreplacementPolicy;@" simplessd/icl/global_point.cc
        # Compile the program
        make

        # Run the program in the background with output redirected to the specific folder
        ./simplessd-standalone ./config/sample.cfg ./simplessd/config/sample.cfg ./result > "$output_folder/$input_file" &
        # Wait for some time before starting the next run (you can adjust this time)
        sleep 2
    done

    # Wait for all background processes to finish
    wait
done
