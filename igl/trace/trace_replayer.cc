/*
 * Copyright (C) 2017 CAMELab
 *
 * This file is part of SimpleSSD.
 *
 * SimpleSSD is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * SimpleSSD is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with SimpleSSD.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "igl/trace/trace_replayer.hh"
#include <fstream>
#include "simplessd/icl/global_point.hh"
#include "simplessd/sim/trace.hh"
#include "simplessd/util/algorithm.hh"

namespace IGL {

TraceReplayer::TraceReplayer(Engine &e, BIL::BlockIOEntry &b,
                             std::function<void()> &f, ConfigReader &c)
    : IOGenerator(e, b, f),
      useLBAOffset(false),
      useLBALength(false),
      nextIOIsSync(false),
      reserveTermination(false),
      io_submitted(0),
      io_count(0),
      read_count(0),
      write_count(0),
      io_depth(0) {
  // Check file
  auto filename = c.readString(CONFIG_TRACE, TRACE_FILE);
  file.open(filename);
  externalFileName=filename;

std::cout << "Stats for the Trace: " << filename << std::endl;
  if (!file.is_open()) {
    SimpleSSD::panic("Failed to open trace file %s!", filename.c_str());
  }

  file.seekg(0, std::ios::end);
  fileSize = file.tellg();
  file.seekg(0, std::ios::beg);
  //printf("Trace player initialized\n");
  // Create regex
  try {
    regex = std::regex(c.readString(CONFIG_TRACE, TRACE_LINE_REGEX));//pass regex to the string
  }
  catch (std::regex_error &e) {
    SimpleSSD::panic("Invalid regular expression!");
  }

  

//exit();
  // Fill flags
  mode = (TIMING_MODE)c.readUint(CONFIG_TRACE, TRACE_TIMING_MODE);
  submissionLatency = c.readUint(CONFIG_GLOBAL, GLOBAL_SUBMISSION_LATENCY);
  completionLatency = c.readUint(CONFIG_GLOBAL, GLOBAL_COMPLETION_LATENCY);
  maxQueueDepth = c.readUint(CONFIG_TRACE, TRACE_QUEUE_DEPTH);
  max_io = c.readUint(CONFIG_TRACE, TRACE_IO_LIMIT);
  groupID[ID_OPERATION] =
      (uint32_t)c.readUint(CONFIG_TRACE, TRACE_GROUP_OPERATION);
  groupID[ID_BYTE_OFFSET] =
      (uint32_t)c.readUint(CONFIG_TRACE, TRACE_GROUP_BYTE_OFFSET);
  groupID[ID_BYTE_LENGTH] =
      (uint32_t)c.readUint(CONFIG_TRACE, TRACE_GROUP_BYTE_LENGTH);
  groupID[ID_LBA_OFFSET] =
      (uint32_t)c.readUint(CONFIG_TRACE, TRACE_GROUP_LBA_OFFSET);
  groupID[ID_LBA_LENGTH] =
      (uint32_t)c.readUint(CONFIG_TRACE, TRACE_GROUP_LBA_LENGTH);
  groupID[ID_TIME_SEC] = (uint32_t)c.readUint(CONFIG_TRACE, TRACE_GROUP_SEC);
  groupID[ID_TIME_MS] =
      (uint32_t)c.readUint(CONFIG_TRACE, TRACE_GROUP_MILI_SEC);
  groupID[ID_TIME_US] =
      (uint32_t)c.readUint(CONFIG_TRACE, TRACE_GROUP_MICRO_SEC);
  groupID[ID_TIME_NS] =
      (uint32_t)c.readUint(CONFIG_TRACE, TRACE_GROUP_NANO_SEC);
  groupID[ID_TIME_PS] =
      (uint32_t)c.readUint(CONFIG_TRACE, TRACE_GROUP_PICO_SEC);
  useHex = c.readBoolean(CONFIG_TRACE, TRACE_USE_HEX);  
  if (groupID[ID_OPERATION] == 0) {
    SimpleSSD::panic("Operation group ID cannot be 0");
  }

  if (groupID[ID_LBA_OFFSET] > 0) {//using lba offset
    useLBAOffset = true;
  }
  if (groupID[ID_LBA_LENGTH] > 0) {
    useLBALength = true;
  }

  if (useLBALength || useLBAOffset) {
    lbaSize = (uint32_t)c.readUint(CONFIG_TRACE, TRACE_LBA_SIZE);

    if (SimpleSSD::popcount(lbaSize) != 1) {
      SimpleSSD::panic("LBA size should be power of 2");
    }
  }

  if (!useLBAOffset && groupID[ID_BYTE_OFFSET] == 0) {
    SimpleSSD::panic("Both LBA Offset and Byte Offset group ID cannot be 0");
  }
  if (!useLBALength && groupID[ID_BYTE_LENGTH] == 0) {
    SimpleSSD::panic("Both LBA Length and Byte Length group ID cannot be 0");
  }

  timeValids[0] = groupID[ID_TIME_SEC] > 0 ? true : false;
  timeValids[1] = groupID[ID_TIME_MS] > 0 ? true : false;
  timeValids[2] = groupID[ID_TIME_US] > 0 ? true : false;
  timeValids[3] = groupID[ID_TIME_NS] > 0 ? true : false;
  timeValids[4] = groupID[ID_TIME_PS] > 0 ? true : false;

  if (!(timeValids[0] || timeValids[1] || timeValids[2] || timeValids[3] ||
        timeValids[4])) {
    if (mode == MODE_STRICT) {
      SimpleSSD::panic("No valid time field specified");
    }
  }

  firstTick = std::numeric_limits<uint64_t>::max();

  completionEvent = [this](uint64_t id) { iocallback(id); };

  submitEvent = engine.allocateEvent([this](uint64_t) { submitIO(); });
  
}

TraceReplayer::~TraceReplayer() {
  file.close();
      

  
}

void TraceReplayer::init(uint64_t bytesize, uint32_t bs) {
  ssdSize = bytesize;
  blocksize = bs;

  if ((useLBALength || useLBAOffset) && lbaSize < bs) {
    SimpleSSD::warn("LBA size of trace file is smaller than SSD's LBA size");
  }
  if(optimalreplacementPolicy)
  {
 readOptMapFromFile();
  }
  
  /*for (const auto& pair : optMap) {
            std::cout << "Key: " << pair.first << ", Values: ";
            for (uint64_t value : pair.second) {
                std::cout << value << " ";
            } 
            cout<< " VecotrSize "<<pair.second.size();
            std::cout << std::endl;
        }*/
}

void TraceReplayer::begin() {
  initTime = engine.getCurrentTick();

  parseLine();
//printf("this code is called how many times\n");

  if (mode == MODE_STRICT) {
    firstTick = linedata.tick;
  }
  else {
    firstTick = initTime;
  }

  if (reserveTermination) {
    SimpleSSD::warn("No I/O submitted. Check regular expression.");

    endCallback();
  }
  else {
    submitIO();
  }
}
void TraceReplayer::readOptMapFromFile()
{
   std::string filename = externalFileName;
    size_t lastSlash = filename.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        filename = filename.substr(lastSlash + 1); // Extract filename
    }

    std::string fullFilePath = outputPath + "/";
    if (cachedGC) {
        fullFilePath += "cached_Eviction_newOnly_" + filename;
    } else {
        fullFilePath += "" + filename;
    }
     std::ifstream inputFile(fullFilePath);
        std::string line;

        if (inputFile.is_open()) {
            while (std::getline(inputFile, line)) {
                // Assuming the format is "Key: lpn, Values: value1 value2 ..."
                std::istringstream iss(line);
                std::string keyString, valuesString, valueString;

                // Extract key part
                iss >> keyString; // "Key:"
                iss >> keyString; // "lpn,"

                // Extract values part
                iss >> valuesString; // "Values:"
                std::vector<uint64_t> values;

                while (iss >> valueString) {
                    values.push_back(std::stoull(valueString));
                }

                // Add the key-value pair to optMap
                uint64_t key = std::stoull(keyString);
                optMap[key] = values;
            }

            inputFile.close();
            std::cout << "OptMap loaded from file WithSize " << filename << optMap.size()<<std::endl;
        } else {
            std::cerr << "Unable to open file for reading." << filename<<std::endl;
        }
    }


void TraceReplayer::hashmapToFile()
{
      std::string filename = externalFileName;
    size_t lastSlash = filename.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        filename = filename.substr(lastSlash + 1); // Extract filename
    }

    std::string fullFilePath = outputPath + "/";
    if (useDeadOnArrivalPredictor) {
        fullFilePath += "predicted_" + filename;
    } else {
        fullFilePath += "reuseDistance_" + filename;
    }

    std::ofstream outputFile(fullFilePath);
    if (!outputFile.is_open()) {
        std::cerr << "Failed to open file for writing: " << fullFilePath << std::endl;
        return;
    }
    /*std::vector<std::pair<uint32_t, victimBlockInfo>> VecErasemap(victimBlockSequence.begin(), victimBlockSequence.end());
std::sort(VecErasemap.begin(), VecErasemap.end(), [](const std::pair<uint64_t, victimBlockInfo>& a, const std::pair<uint64_t, victimBlockInfo>& b) {
        return a.first < b.first; // Reverse the comparison for descending order
    });

    outputFile << "GC,BlockId,Cachedpages,Validpages" << std::endl;
    for (const auto& entry : VecErasemap) {
      outputFile << entry.first << "," << entry.second.BlockId << "," << entry.second.cachedPages<< "," << entry.second.validPages<<endl;
    }*/
    outputFile << "AccessFrequcy,Count" << std::endl;
    for (const auto& entry : AddressReuseDistanceClusters) {
      outputFile << entry.first << "," << entry.second<<endl;
    }
     /*for (const auto& pair : optMap) {
                outputFile << "Key: " << pair.first << ", Values: ";
                for (uint64_t value : pair.second) {
                    outputFile << value << " ";
                }
                outputFile << std::endl;
            }
    //outputFile.close();
     //outputFile << "Access,Count" << std::endl;
    //for (const auto& entry : LbaAccessFrequency) {
      // outputFile << entry.first << "," << entry.second << std::endl;
    //}
    outputFile.close();
  //###################################################################################
std::vector<std::pair<uint64_t, uint32_t>> GcVec(gcTimmingBucket.begin(), gcTimmingBucket.end());
std::sort(GcVec.begin(), GcVec.end(), [](const std::pair<uint64_t, uint32_t>& a, const std::pair<uint64_t, uint32_t>& b) {
        return a.second > b.second; // Reverse the comparison for descending order
    });

    outputFile << "GCLatency,Frequeny" << std::endl;
    for (const auto& entry : GcVec) {
       outputFile << entry.first << "," << entry.second << std::endl;
    }
    outputFile.close();
    ////###################################################################################// 
    fullFilePath=outputPath +"ReadLatency/";
          
                 
                      if (cachedGC) {
                      fullFilePath += "cachedReadLatency_" + filename;
                          } else {
                      fullFilePath += "greedyReadLatency_" + filename;
                          }
 ofstream  outputFile1(fullFilePath);
  if (!outputFile1.is_open()) {
                      std::cerr << "Failed to open file for writing: " << fullFilePath << std::endl;
                      return;
                  }
std::vector<std::pair<uint64_t, uint32_t>> ReadVec(ReadBusyBucket.begin(), ReadBusyBucket.end());
std::sort(ReadVec.begin(), ReadVec.end(), [](const std::pair<uint64_t, uint32_t>& a, const std::pair<uint64_t, uint32_t>& b) {
        return a.second > b.second; // Reverse the comparison for descending order
    });
                  outputFile1 << "ReadLatency,Frequeny" << std::endl;
                  for (const auto& entry : ReadVec) {
                    outputFile1 << entry.first << "," << entry.second << std::endl;
                  }
                  outputFile1.close();
     //###################################################################################//   
            fullFilePath= outputPath + "WriteLatency/";
            
                      if (cachedGC) {
                      fullFilePath += "cachedWriteLatency_" + filename;
                          } else {
                      fullFilePath += "greedyWriteLatency_" + filename;
                          }
                    ofstream  outputFile2(fullFilePath);
                  if (!outputFile2.is_open()) {
                      std::cerr << "Failed to open file for writing: " << fullFilePath << std::endl;
                      return;
                  }
std::vector<std::pair<uint64_t, uint32_t>> WriteVec(WriteBusyBucket.begin(), WriteBusyBucket.end());
std::sort(WriteVec.begin(), WriteVec.end(), [](const std::pair<uint64_t, uint32_t>& a, const std::pair<uint64_t, uint32_t>& b) {
        return a.second > b.second; // Reverse the comparison for descending order
    });
                  outputFile2 << "WriteLatency,Frequeny" << std::endl;
                  for (const auto& entry : WriteVec) {
                    outputFile2 << entry.first << "," << entry.second << std::endl;
                  }
                  outputFile2.close();

 //###################################################################################//
                  fullFilePath = outputPath + "PageMovements/";
                  
                      if (cachedGC) {
                      fullFilePath += "cached_" + filename;
                          } else {
                      fullFilePath += "greedy_" + filename;
                          }

                          ofstream outputFile3(fullFilePath);
                      if (!outputFile3.is_open()) {
                      std::cerr << "Failed to open file for writing: " << fullFilePath << std::endl;
                      return;
                  }
std::vector<std::pair<uint64_t, uint32_t>> PageMVec(pageMoveBucket.begin(), pageMoveBucket.end());
std::sort(PageMVec.begin(), PageMVec.end(), [](const std::pair<uint64_t, uint32_t>& a, const std::pair<uint64_t, uint32_t>& b) {
        return a.second > b.second; // Reverse the comparison for descending order
    });
                          outputFile3<<"PagesMoved,Frequeny" << std::endl;
                           for (const auto& entry : PageMVec) {
                    outputFile3 << entry.first << "," << entry.second << std::endl;
                  }
                  outputFile3.close();


                  //###################################################################################
                 fullFilePath = outputPath + "PageReuse/";
                      if (cachedGC) {
                      fullFilePath += "cached_" + filename;
                          } else {
                      fullFilePath += "greedy_" + filename;
                          }
                           
                  ofstream outputFile4(fullFilePath);
                      if (!outputFile4.is_open()) {
                      std::cerr << "Failed to open file for writing: " << fullFilePath << std::endl;
                      return;
                  }
 std::vector<std::pair<uint64_t, uint32_t>> PageReuseVec(PageReuseBucket.begin(), PageReuseBucket.end());
std::sort(PageReuseVec.begin(), PageReuseVec.end(), [](const std::pair<uint64_t, uint32_t>& a, const std::pair<uint64_t, uint32_t>& b) {
        return a.second > b.second; // Reverse the comparison for descending order
    });
                          outputFile4<<"PagesReused,Frequeny" << std::endl;
                           for (const auto& entry : PageReuseVec) {
                    outputFile4 << entry.first << "," << entry.second << std::endl;
                  }
                  outputFile4.close();

 //###################################################################################//*/

    
}
void TraceReplayer::printStats(std::ostream &out) {
  uint64_t tick = engine.getCurrentTick();

  //printf("Dirty Entries: %ld\n",dirtyStatusCounter);
  //printf("totalPagesMovedFromBlockToCache %lu\n",totalPagesMovedFromBlockToCache);
   ConfigReader c; // Initialize the configuration object
    auto filename = c.readString(CONFIG_TRACE, TRACE_FILE);
  //printf("\nTotal ICL Count %ld\n",iclCount);
  printf("DirtyPage.Updates %lu\n",totalDirtyStatusUpdate);
  printf("Total.Evictions %lu\n",totalEvictions);
  printf("Total.DirtyEvictions %lu\n",totalDirtyEvictions);
  printf("TotalTimesEvictionCalled %lu\n",totalTimesEvictionCalled);
  printf("TotalCleanEvictions1 %lu\n",TotalCleanEvicitons1);
  printf("TotalCleanEvictions2 %lu\n",TotalCleanEvicitons2);
  printf("Dirty Entries: %ld\n",dirtyStatusCounter);
 // printf("totalPagesMovedFromBlockToCache %lu\n",totalPagesMovedFromBlockToCache);
  printf("Total ICL Count %ld\n",iclCount);

  if(cachedGC)
  {
      printf("PagesCopied.to.Cache.From.Block %lu\n",totalPagesMovedFromBlockToCache);
  }
  else
  {
      printf("PagesCopied.from.VictumBlock.freeBlock %lu\n",totalPagesMovedFromBlockToBlock);

  }

  //printf("Plane,Reads\n");
  /*for(const auto& iter:ReadMap)
  {
    std::cout<<iter.first<<" ,"<<iter.second<<std::endl;
  }
  //printf("\n\nPlane,Writes\n");
  for(const auto& iter:WriteMap)
  {
    std::cout<<iter.first<<" ,"<<iter.second<<std::endl;
  }*/
  //hashmapToFile(filename)
  //hashmapToFile();
  if(!optimalreplacementPolicy)
  {
  //hashmapToFile();
  }
  
     // printf("Value of tick inTP %lu\n",tick);
  out << "*** Statistics of Trace Replayer ***" << std::endl;
  out << "Tick: " << tick << std::endl;
  out << "Time (ps): " << firstTick - initTime << " - " << tick << " ("
      << tick + firstTick - initTime << ")" << std::endl;
  out << "I/O (bytes): " << io_submitted << " ("
      << std::to_string((double)io_submitted / tick * 1000000000000.) << " B/s)"
      << std::endl;
  out << "I/O (counts): " << io_count << " (Read: " << read_count
      << ", Write: " << write_count << ")" << std::endl;
  out << "*** End of statistics ***" << std::endl;
  

  bioEntry.printStats(out);
}

void TraceReplayer::getProgress(float &val) {
  if (max_io == 0) {
    // If I/O count is unlimited, use file pointer for fast progress calculation
    uint64_t ptr;

    {
      std::lock_guard<std::mutex> guard(m);
      ptr = file.tellg();
    }

    val = (float)ptr / fileSize;
  }
  else {
    // Use submitted I/O count in progress calculation
    // If trace file contains I/O requests smaller than max_io, progress value
    // cannot reach 1.0 (100%)
    val = (float)io_count / max_io;
  }
}

uint64_t TraceReplayer::mergeTime(std::smatch &match) {
  uint64_t tick = 0;
  bool valid = true;

  if (timeValids[0] && match.size() > groupID[ID_TIME_SEC]) {
    tick += strtoul(match[groupID[ID_TIME_SEC]].str().c_str(), nullptr, 10) *
            1000000000000ULL;
  }
  else if (timeValids[0]) {
    valid = false;
  }

  if (timeValids[1] && match.size() > groupID[ID_TIME_MS]) {
    tick += strtoul(match[groupID[ID_TIME_MS]].str().c_str(), nullptr, 10) *
            1000000000ULL;
  }
  else if (timeValids[1]) {
    valid = false;
  }

  if (timeValids[2] && match.size() > groupID[ID_TIME_US]) {
    tick += strtoul(match[groupID[ID_TIME_US]].str().c_str(), nullptr, 10) *
            1000000ULL;
  }
  else if (timeValids[2]) {
    valid = false;
  }

  if (timeValids[3] && match.size() > groupID[ID_TIME_NS]) {
    tick += strtoul(match[groupID[ID_TIME_NS]].str().c_str(), nullptr, 10) *
            1000ULL;
  }
  else if (timeValids[3]) {
    valid = false;
  }

  if (timeValids[4] && match.size() > groupID[ID_TIME_PS]) {
    tick += strtoul(match[groupID[ID_TIME_PS]].str().c_str(), nullptr, 10);
  }
  else if (timeValids[4]) {
    valid = false;
  }

  if (!valid) {
    SimpleSSD::panic("Time parse failed");
  }

  return tick;
}

BIL::BIO_TYPE TraceReplayer::getType(std::string type) {
  io_count++;
  myIoCount++;
//printf("IO count %ld and old IO coont %lu\n",IoCount,io_count);
if( iclCount > SimlationIORequests)
//if(gcCounter>10)
{
  
  
  reserveTermination=true;
  
}
  switch (type[0]) {
    case 'r':
    case 'R':
      read_count++;

      return BIL::BIO_READ;
    case 'w':
    case 'W':
      write_count++;

      return BIL::BIO_WRITE;
    case 'f':
    case 'F':
      return BIL::BIO_FLUSH;
    case 't':
    case 'T':
    case 'd':
    case 'D':
      return BIL::BIO_TRIM;
  }

  return BIL::BIO_NUM;
}

void TraceReplayer::parseLine() {
  //printf("Inside tracer_player parse line function\n");
  std::string line;
  std::smatch match;
 //printf("Value of erase_counter####################### %ld\n",eraser);
/*if(eraser >= 300)
{
   //printf("Value of erase_counter %ld\n",erase_counter);

           reserveTermination = true;;
}*/

  // Read line
  while (true) {
    bool eof = false;

    {
      std::lock_guard<std::mutex> guard(m);

      eof = file.eof();
      std::getline(file, line);
      //printf("erase counter is %ld\n",erase_counter);
       
    }
    //bool matcher=std::regex_match(line,match,regex);

//printf("###########Value of boolena is %d\n",matcher);
    if (eof) {
      reserveTermination = true;

      if (io_depth == 0) {
        // No on-the-fly I/O
        endCallback();
      }

      return;
    }
    if (std::regex_match(line, match, regex)) {
      break;
    }
    //if(tracker==10)
    //tracker
   
  }

  // Get time
  linedata.tick = mergeTime(match);
 
  // Fill BIO
 
  if (useLBAOffset) { // i have changed the code...
  
  linedata.offset = strtoul(match[groupID[ID_LBA_OFFSET]].str().c_str(), nullptr , useHex ? 16 : 10);

                      
  }
  else {
    linedata.offset = strtoul(match[groupID[ID_BYTE_OFFSET]].str().c_str(),
                              nullptr, useHex ? 16 : 10);
  }

  if (useLBALength) {
    linedata.length = strtoul(match[groupID[ID_LBA_LENGTH]].str().c_str(),
                             nullptr, useHex ? 16 : 10);// change is done
  //linedata.length = strtoul(match[groupID[ID_LBA_LENGTH]].str().c_str(),
                             // nullptr, useHex ? 16 : 10) *
                      //lbaSize;
  }
  else {
    linedata.length = strtoul(match[groupID[ID_BYTE_LENGTH]].str().c_str(),
                              nullptr, useHex ? 16 : 10);
  }

  // This function increases I/O count
  linedata.type = getType(match[groupID[ID_OPERATION]].str());
 
}

void TraceReplayer::submitIO() {
  BIL::BIO bio;
  //printf("Inside trace player submit IO\n");
  if (linedata.type == BIL::BIO_NUM) {
    SimpleSSD::panic("Unexpected request type.");
  }
  

  bio.callback = completionEvent;
  bio.id = io_count;
  bio.type = linedata.type;
  bio.offset = linedata.offset;
  bio.length = linedata.length;
  
//std::cout <<" Trace player Id:"<< bio.id <<" TYPe: "<<bio.type<<" Offset: "<<bio.offset <<" length: "<<bio.length<<" Tick: "<<linedata.tick<<std::endl;
//printf("Thi is inside the submitter\n");
//printf("Timer %lu\n",linedata.tick);
  bioEntry.submitIO(bio);
//std::cout <<"Id:"<< bio.id <<bio.type<<bio.offset <<bio.offset<<std::endl;

  io_depth++;
 ioQueueDepth++;
  if ((max_io != 0 && io_count >= max_io)) {
    reserveTermination = true;

    return;
  }

  parseLine();
//printf("Valu of IODepth is %lu\n",ioQueueDepth);

  if (reserveTermination) {
    return;
  }

  switch (mode) {
    case MODE_STRICT:
      engine.scheduleEvent(submitEvent, linedata.tick - firstTick + initTime);
      break;
    case MODE_ASYNC:
      rescheduleSubmit(submissionLatency);
      break;
    default:
      break;
  }
}

void TraceReplayer::iocallback(uint64_t) {// this function is called after the completion of each IO request
  //printf("Inside callback in trace player value of IO depth is %lu\n",io_depth);
  io_depth--;
ioQueueDepth--;
  if (reserveTermination) {
    // Everything is done
    if (io_depth == 0) {
      endCallback();
    }
  }

  if (mode == MODE_SYNC || nextIOIsSync) {
    // MODE_ASYNC submission blocked by I/O depth limitation
    // Let's submit here
    nextIOIsSync = false;

    rescheduleSubmit(submissionLatency + completionLatency);
  }
}

void TraceReplayer::rescheduleSubmit(uint64_t breakTime) {
  if (mode == MODE_ASYNC) {
    if (io_depth >= maxQueueDepth) {
      nextIOIsSync = true;

      return;
    }
  }
  else if (mode == MODE_STRICT) {
    return;
  }

  engine.scheduleEvent(submitEvent, engine.getCurrentTick() + breakTime);
}

}  // namespace IGL
