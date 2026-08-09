#ifndef GLOBAL_CACHE_H
#define GLOBAL_CACHE_H

#include <cstdint>
#include <chrono>
#include <unordered_map>
#include "icl/generic_cache.hh"

struct palInfo
{
    uint32_t channelNum;
    uint32_t ChipNum;
    uint32_t DieNum;
    uint32_t PlaneNum;
    uint32_t BlockNum;
    uint32_t ftlBlockIndex;

};
struct SSDInfo
{
    uint32_t channels;
    uint32_t Chips;
    uint32_t Dies;
    uint32_t Planes;
    

};
extern SimpleSSD::ICL::AbstractCache* globalCache;
struct victimBlockInfo{
  uint32_t validPages;
  uint32_t BlockId;
  uint32_t cachedPages;
  victimBlockInfo() : validPages(0),BlockId(0),cachedPages(0){}
};

extern std:: unordered_map<uint32_t,victimBlockInfo> victimBlockSequence;
//extern SimpleSSD::ICL::ICL *globalICL;
extern bool multiVictimBlock;
extern bool SameChipmultiVictimBlock;
extern uint32_t ssdInternals[4];
extern bool eraserIO;
extern int block_index;
extern long test_read;
extern long totalFtlReads;
extern long undoCaching;
extern long overwrittenPages;
extern long all_cached_pages;
extern long erase_counter;
extern long eraser;
extern uint32_t liner;
extern int tempo;
extern int input_trace;
extern std::vector<SimpleSSD::ICL::Line> evictLines;
extern std::vector<SimpleSSD::ICL::Line> linesToRemove;
extern std::vector<uint32_t>erased_block_id;
extern std::vector<uint64_t>movePagesToCache;
extern std::vector<uint64_t>movePagesToBlock;
extern long  myIoCount;
extern bool terminator;
extern bool logical_check;
extern int lpnUpdate;
extern int gcCounter;
extern long submittedIOs;
extern uint64_t ioQueueDepth;
extern long dirtyStatusCounter;
extern bool evictLinesCalled;
extern int FreePageInErasedBlock;;
extern bool switchToEvictions;
extern int glocalEvictor;
extern uint32_t currentBlockId;// this is for containing the Id of the block containig the evicted entries.
extern bool getin;
extern bool getin2;
extern uint64_t iclCount;
extern bool putPagesInCache;
extern uint64_t totalPagesMovedFromBlockToCache;
extern uint64_t totalPagesMovedFromBlockToBlock;
extern uint64_t totalPagesVictimToFresh;
extern uint64_t totalEvictions;
extern uint64_t totalDirtyEvictions;
extern uint64_t totalDirtyStatusUpdate;
extern bool cachedGC;
extern uint64_t timeDiff;
extern std::unordered_map<uint64_t, uint32_t> gcTimmingBucket; 
extern std::unordered_map<uint64_t, uint64_t> globalLBaReuseMap; 
extern std::unordered_map<uint64_t, uint32_t> ReadMap; 
extern std::unordered_map<uint64_t, uint32_t> WriteMap; 
extern std::unordered_map<uint64_t, palInfo> eraseMapBucket; 
extern std::unordered_map<uint64_t, uint32_t> frequencyBucket; 
extern std::unordered_map<uint32_t, uint32_t> pageMoveBucket;;  // Declare globally
extern std::unordered_map<uint64_t, uint32_t> ReadBusyBucket; 
extern std::unordered_map<uint64_t, uint32_t> WriteBusyBucket; 
extern std::unordered_map<uint64_t, uint64_t> PageReuseBucket; 
extern std::unordered_map<uint64_t, uint32_t> HotnessMeter; 
extern std:: unordered_map<uint64_t, uint64_t> LbaAccessFrequency;
extern std:: unordered_map<uint64_t, uint64_t> AddressReuseDistanceMap;
extern std:: unordered_map<uint64_t, uint64_t> AddressReuseDistanceClusters;
extern std:: unordered_map<uint64_t, uint64_t> cmtEntryEvictionAccessFrequency;
extern std::unordered_map<uint64_t, uint32_t> gcInvocationBucket;
extern std::unordered_map<uint64_t, uint32_t> lbaEvictionFrequency;
extern std::unordered_map<uint64_t, bool> pageEvictionIndicator;// this will indicate if the page is evictedby Gc or by host
extern std::chrono::high_resolution_clock::time_point startTime;
extern uint64_t startingTick;
extern uint64_t ghostCacheSize;
extern uint64_t ghostEvictCacheSize;
extern std::string externalFileName;
extern std:: string outputPath;
extern uint64_t totalTimesEvictionCalled;
extern uint64_t TotalCleanEvicitons1,TotalCleanEvicitons2;
extern bool pageisPresentInSSD;
extern int victimSelectionPolicy;
extern uint64_t movePageFromGhostToCacheReads;
extern uint64_t movePageFromGhostToCacheWrites;
extern bool blockToBlockMovement;
extern bool doEvictionByGC;
extern uint64_t SimlationIORequests;
extern bool pageAwareofCache;
extern uint64_t nlogicalpagesinSSD;
extern uint64_t cmtSullSize;
extern std::unordered_map<uint64_t,std::vector<uint64_t>> optMap;
extern bool optimalreplacementPolicy;
extern bool useDeadOnArrivalPredictor;
extern bool printDeadOnArrival;
extern bool usePortionOfCacheAsShadow;
extern size_t cmtSize; 
extern bool enableCMT;





//extern const SimpleSSD::FTL::EVICT_POLICY globalPolicy;

#endif // GLOBAL_CACHE_H

