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

#ifndef __FTL_PAGE_MAPPING__
#define __FTL_PAGE_MAPPING__

#include <cinttypes>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "icl/generic_cache.hh"
#include "ftl/abstract_ftl.hh"
#include "ftl/common/block.hh"
#include "ftl/ftl.hh"
#include "pal/pal.hh"
#include "icl/global_point.hh"
namespace SimpleSSD {

namespace FTL {
// 
class PageMapping : public AbstractFTL {
 private:
  PAL::PAL *pPAL;

  ConfigReader &conf;
  //ICL::GenericCache* pCache;
  FTL* pFTL;
  std::unordered_map<uint64_t, std::vector<std::pair<uint32_t, uint32_t>>>
      table;
  std::unordered_map<uint32_t, Block> blocks;
  std::list<Block> freeBlocks;
  uint32_t nFreeBlocks;  // For some libraries which std::list::size() is O(n)
  std::vector<uint32_t> lastFreeBlock;
  //std::vector<uint32_t> ersed
  Bitset lastFreeBlockIOMap;
  uint32_t lastFreeBlockIndex;
  //const std::vector<std::vector<ICL::Line>*>& cacheDataRef;//change is done here
   struct MappingEntry {
        // Define your structure for the mapping entry
        // This structure holds the actual mapping details
        uint32_t block_id=0;
        uint32_t page_Id=0;
        uint64_t lastAccessedTime=0; // To store last accessed time (tick)
        uint64_t cmtEntryAccessCount=0;
        uint64_t iclEntry;
    };
     struct shadowCacheEntry {
        // Define your structure for the mapping entry
        // This structure holds the actual mapping details
        uint32_t block_id=0;
        uint32_t page_Id=0;
        uint64_t lastAccessedTime=0; // To store last accessed time (tick)
        uint64_t cmtEntryAccessCount=0;
        uint64_t iclEntry;
    };
std::unordered_map<uint64_t, MappingEntry> cmt;
//std::unordered_map<uint64_t, MappingEntry> cmt;
std::unordered_map<uint64_t,uint64_t> evictedCmtEntries;
std::unordered_map<uint64_t, shadowCacheEntry> shadowwCache;
std::vector<float> DeadPagePercentage;

    //size_t cmtSize; // Maximum size of the CMT
   size_t shadowwCacheSize;
  bool bReclaimMore;
  bool bRandomTweak;
  uint32_t bitsetSize;
  uint64_t predictorThreshold;

  struct {
    uint64_t gcCount;
    uint64_t reclaimedBlocks;
    uint64_t validSuperPageCopies;
    uint64_t validPageCopies;
    uint64_t victimToFreeMovements;
    uint64_t cachedPageCount;
    uint64_t pagesNotCachedButValid;
    uint64_t pagesUpdatedDirty;
    uint64_t totalValidPagesMovement;
    uint64_t entreisPresentInmappingtableNotupdated;
    uint64_t entriesbelongingTothisblock;
    
    
  } stat;

struct returnedSSDParameters{
  uint32_t channel;
  uint32_t chip;
  uint32_t die;
  uint32_t plane;

} ;
  struct{
    uint64_t pageMappingFoundForReads;
    uint64_t TotalFtlReadRequests;
    uint64_t TotalFtlWriteRequests;
    uint64_t pageMappingFoundForWrites;
    float FreeBlockRatioValue;
    uint32_t cachedpagesfoundInAllBlocks;
    uint64_t inconsistantRequests;
  } ftlStats;
  struct {
    uint64_t cmtReadRequests;;
    uint64_t cmtReadRequestsHits;;
    uint64_t cmtWriteRequests;;
    uint64_t cmtWriteRequestsHits;;
    uint64_t cmtEntriesEvictedAgainRequested;
    uint64_t deadOnArrivalCounter;
    uint64_t cmtEvictions;
    uint64_t entriesWithOneAccessCount;
    uint64_t misPredictionCount;
    uint64_t EvictionsFromShadowCache;
    uint64_t entriesLoadedInCmt;
    uint64_t cleanEntriesEvictedFromCmt;
    uint64_t dirtyEntriesEvictedFromCmt;
    
  }cmtStats;
struct PageMoveBucket {
    uint32_t count;

    PageMoveBucket() : count(0) {}
};
    std::vector<PageMoveBucket> pageMoveStats;

  float freeBlockRatio();
  uint64_t getAccessCountOfLBA(uint64_t);
  void evictEntryFromCMT();
  uint32_t convertBlockIdx(uint32_t);
  uint32_t getFreeBlock(uint32_t);
  uint32_t getLastFreeBlock(Bitset &);
   uint32_t getUpdatedLastFreeBlock();
   uint32_t findBlockWithLeastValidDirtyPagesFromVector();
  void calculateVictimWeight(std::vector<std::pair<uint32_t, float>> &,
                             const EVICT_POLICY, uint64_t);
void addLbaToOptMap(uint64_t);
float returnDeadPagePErcent();
void optimalRepalcementPolicy();
void EvictFromShadowCache();
void displayCMt();
void calculateVictimCachedWeight(std::vector<std::pair<uint32_t, std::pair<uint32_t, uint32_t>>>&);
void findHotAccessBlock(std::vector<std::pair<uint32_t, std::pair<uint32_t, uint32_t>>>&);
void RemoveEntryFromTable(uint64_t,uint64_t &);
void newFTLRequest(uint64_t,uint64_t &) ;
void writeEvictedDataToFlash(uint64_t &);
uint64_t getBlockCachedPageCount();
void writeToLeastusedBlock(uint64_t,uint64_t &);
void printBlockState();
void callBlockResetFunction(uint64_t &);
SSDInfo getSSDInternalInfo(uint32_t);
//victimBlockInfo getVictimBlockInfo(uint32_t);
uint32_t getLPNEvictionFrequency(uint64_t);
  
  void selectVictimBlock(std::vector<uint32_t> &, uint64_t &);
  void doGarbageCollection(std::vector<uint32_t> &, uint64_t &);
  //void doGarbageCollection(std::vector<ICL::Line>&, std::vector<uint32_t>&, uint64_t&);
bool isLineEqual(const SimpleSSD::ICL::_Line& line1, const SimpleSSD::ICL::_Line& line2);
  float calculateWearLeveling();
  void calculateTotalPages(uint64_t &, uint64_t &);
  void printEvictLines();
void removeEntriesFromEvictionCache();
  void readInternal(Request &, uint64_t &);
  void writeInternal(Request &, uint64_t &, bool = true);
  void writeEvictedLinesToStorage(Request &, uint64_t &,std::vector<PAL::Request> &writeRequests);
  void trimInternal(Request &, uint64_t &);
  void eraseInternal(PAL::Request &, uint64_t &);
  void resetBlockAccesCount();
  uint32_t findVictimBlock(std::vector<std::pair<uint32_t, std::pair<uint32_t, uint32_t>>>&);

 public:
  PageMapping(ConfigReader &, Parameter &, PAL::PAL *, DRAM::AbstractDRAM *);
  ~PageMapping();

  bool initialize() override;

  void read(Request &, uint64_t &) override;
  void write(Request &, uint64_t &) override;
  void trim(Request &, uint64_t &) override;
  void undoCached(uint64_t ) override;
  void printFtlStats() override;
  void format(LPNRange &, uint64_t &) override;

  Status *getStatus(uint64_t, uint64_t) override;

  void getStatList(std::vector<Stats> &, std::string) override;
  void getStatValues(std::vector<double> &) override;
  void resetStatValues() override;
};

}  // namespace FTL

}  // namespace SimpleSSD

#endif
