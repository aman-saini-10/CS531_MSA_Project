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

#ifndef __ICL_GENERIC_CACHE__
#define __ICL_GENERIC_CACHE__

#include <functional>
#include <random>
#include <vector>

#include "icl/abstract_cache.hh"

namespace SimpleSSD {

namespace ICL {

class GenericCache : public AbstractCache {
 private:
  const uint32_t superPageSize;
  const uint32_t parallelIO;
  uint32_t lineCountInSuperPage;
  uint32_t lineCountInMaxIO;
  uint32_t lineSize;
  uint32_t setSize;
  uint32_t waySize;
  uint32_t ghostWaySize;

  const uint32_t prefetchIOCount;
  const float prefetchIORatio;

  const bool useReadCaching;
  const bool useWriteCaching;
  const bool useReadPrefetch;

  bool bSuperPage;

  struct SequentialDetect {
    bool enabled;
    Request lastRequest;
    uint32_t hitCounter;
    uint32_t accessCounter;

    SequentialDetect() : enabled(false), hitCounter(0), accessCounter(0) {
      lastRequest.reqID = 1;
    }
  } readDetect;

  uint64_t prefetchTrigger;
  uint64_t lastPrefetched;

  PREFETCH_MODE prefetchMode;
  EVICT_MODE evictMode;
  EVICT_POLICY policy;
  EVICT_POLICY policyAtGc;
  std::function<uint32_t(uint32_t, uint64_t &)> evictFunction;
  std::function<uint32_t(uint32_t, uint64_t &)> evictFunction1;

  std::function<Line *(Line *, Line *)> compareFunction;
    std::function<Line *(Line *, Line *)> compareFunction1;

  std::random_device rd;
  std::mt19937 gen;
  std::uniform_int_distribution<uint32_t> dist;

  std::vector<Line *> cacheData;
  std::vector<Line **> evictData;
  std::vector<Line *> ghostCacheData;
  std::vector<Line **> ghostEvictData;

  uint64_t getCacheLatency();

  uint32_t calcSetIndex(uint64_t);
  void calcIOPosition(uint64_t, uint32_t &, uint32_t &);
uint32_t getEvictedWay(uint32_t,uint64_t);
  uint32_t getEmptyWay(uint32_t, uint64_t &);
  uint32_t getGhostEmptyWay(uint32_t, uint64_t &);

  uint32_t getValidWay(uint64_t, uint64_t &);
  uint32_t getNewValidWay(uint64_t, uint64_t &);
  void checkSequential(Request &, SequentialDetect &);
  void print();
  void displayEvictLines();
  void printEvictData();
  void printIncommingRequest(Request &);
 
  void ghostEviction(uint64_t, bool = true);
 void displayGhostCache();
 //void updateGhostCacheOnHit(uint32_t,uint32_t,uint64_t &);
 void updateGhostCacheForReads(uint32_t,uint32_t,uint64_t &);
 void updateGhostCacheForWrites(uint32_t,uint32_t,uint64_t &);
 bool pageEvictionIndication(uint64_t);
  bool pageEvictedByHost(uint64_t);

  void evictCache(uint64_t, bool = true);
  

  // Stats
  struct {
    uint64_t request[2];
    uint64_t cache[2];
  } stat;
struct{
  uint64_t pageWriteHitsInMainCache;
  uint64_t pageReadHitsInMainCache;
  uint64_t mainCachePageReadRequest;
  uint64_t mainCachePageWriteRequest;
  uint64_t mainCachereadsWithoutFlush;
  uint64_t mainCacheWritesWithoutFlush;
  uint64_t prefetcherActivationCounter;
  uint64_t dirtyUpdateRequests;
  uint64_t lastEvictedTick;
  uint64_t pageEvictedByGcAgainAccessed;
  uint64_t pageEvictedByHostAgainAccessed;

} caching;
struct{
  uint64_t ghostCachepageWriteHits;
  uint64_t ghostCachepageReadHits;
  uint64_t ghostCachePageReadRequest;
  uint64_t ghostCachePageWriteRequest;
  uint64_t pagesMovedFromGhostCacheReads;
  uint64_t pagesMovedFromGhostCacheWrites;
  uint64_t ghostCacheWritesWithoutFlush;
  uint64_t ghostCacheTotalEvictions;
  uint64_t ghostCacheDirtyEvictions;
  //uint64_t ghostprefetcherActivationCounter;
} ghostCache;
 public:
  GenericCache(ConfigReader &, FTL::FTL *, DRAM::AbstractDRAM *);
  ~GenericCache();

  bool read(Request &, uint64_t &) override;
  bool write(Request &, uint64_t &) override;
  bool writeBlockToCache(Request &, uint64_t &)override;// used for writing the evicted block to the cache
bool updateDirty(uint64_t, uint64_t &) override;// updating the mapping of each
  void flush(LPNRange &, uint64_t &) override;
  void trim(LPNRange &, uint64_t &) override;
  void format(LPNRange &, uint64_t &) override;
void modifedEviction(uint64_t &,uint64_t ) override;
  void getStatList(std::vector<Stats> &, std::string) override;
  void getStatValues(std::vector<double> &) override;
  void resetStatValues() override;
  void EvictedLinesToFTl(uint64_t &) override;
  bool readForCachedGc(Request &, uint64_t &) override;
 bool writeForCachedGC(Request &, uint64_t &) override;
 bool writeVictimPagesToCache(Request &, uint64_t &) override;
  void genericCacheStats() override;
  
  
};

}  // namespace ICL

}  // namespace SimpleSSD

#endif
