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

#include "icl/generic_cache.hh"
#include "ftl/config.hh"
#include "icl/config.hh"
#include "icl/global_point.hh"
#include <algorithm>
#include <cstddef>
#include <limits>

#include "util/algorithm.hh"

namespace SimpleSSD {

namespace ICL {

GenericCache::GenericCache(ConfigReader &c, FTL::FTL *f, DRAM::AbstractDRAM *d)
    : AbstractCache(c, f, d),
      superPageSize(f->getInfo()->pageSize),
      parallelIO(f->getInfo()->pageCountToMaxPerf),
      lineCountInSuperPage(f->getInfo()->ioUnitInPage),
      lineCountInMaxIO(parallelIO * lineCountInSuperPage),
      waySize(conf.readUint(CONFIG_ICL, ICL_WAY_SIZE)),
      prefetchIOCount(conf.readUint(CONFIG_ICL, ICL_PREFETCH_COUNT)),
      prefetchIORatio(conf.readFloat(CONFIG_ICL, ICL_PREFETCH_RATIO)),
      useReadCaching(conf.readBoolean(CONFIG_ICL, ICL_USE_READ_CACHE)),
      useWriteCaching(conf.readBoolean(CONFIG_ICL, ICL_USE_WRITE_CACHE)),
      useReadPrefetch(conf.readBoolean(CONFIG_ICL, ICL_USE_READ_PREFETCH)),
      gen(rd()),
      dist(std::uniform_int_distribution<uint32_t>(0, waySize - 1)) {
  uint64_t cacheSize = conf.readUint(CONFIG_ICL, ICL_CACHE_SIZE);
  

  lineSize = superPageSize / lineCountInSuperPage;
   ghostWaySize= MAX(ghostCacheSize/lineSize, 1);
   if(victimSelectionPolicy==4)
   {
          cacheSize= cacheSize-ghostCacheSize;

   }
   //cacheSize=67108864;
  if (lineSize != superPageSize) {
    bSuperPage = true;
  }

  if (!conf.readBoolean(CONFIG_FTL, FTL::FTL_USE_RANDOM_IO_TWEAK)) {
    lineSize = superPageSize;
    lineCountInSuperPage = 1;
    lineCountInMaxIO = parallelIO;
  }

  if (!useReadCaching && !useWriteCaching) {
    return;
  }
  // Fully-associated?
  if (waySize == 0) {  // this is fully assoicative cache..
    setSize = 1;
    waySize = MAX(cacheSize / lineSize, 1);
  }
  else {  // this is for maintaining the assoicaitvity.
    setSize = MAX(cacheSize / lineSize / waySize, 1);
  }
  // printf("linesize: %u cachesize: %lu WaySize: %u SetSize
  // %u\n",lineSize,cacheSize, waySize,setSize);
  printf("Set size %u | Way size %u | Line size %u | Capacity %lu MB | Lines "
         "Superpage %u\n",
         setSize, waySize, lineSize, (uint64_t)(((setSize * waySize * lineSize)/1024)/1024),
         lineCountInSuperPage);
  printf("Set size %u | GhostWays %u | Line size %u | GhostCacheSize %lu MB | Lines "
         "Superpage %u\n",
         setSize, ghostWaySize, lineSize, (uint64_t)(((setSize * ghostWaySize * lineSize )/1024)/1024),
         lineCountInSuperPage);
  debugprint(LOG_ICL_GENERIC_CACHE,
             "CREATE  | line count in super page %u | line count in max I/O %u",
             lineCountInSuperPage, lineCountInMaxIO);

  cacheData.resize(setSize);
  ghostCacheData.resize(setSize);


  for (uint32_t i = 0; i < setSize; i++) {
    cacheData[i] = new Line[waySize]();  //intilization of cache data
  }

  evictData.resize(lineCountInSuperPage);
  ghostEvictData.resize(lineCountInSuperPage);
printf("Value of parallelIO: %u\n",parallelIO);
  for (uint32_t i = 0; i < lineCountInSuperPage; i++) {
    evictData[i] = (Line **)calloc(parallelIO, sizeof(Line *));

  }

    {
        for (uint32_t i = 0; i < setSize; i++) {
    ghostCacheData[i] = new Line[ghostWaySize]();  //intilization of cache data
      }
      for (uint32_t i = 0; i < lineCountInSuperPage; i++) {
    ghostEvictData[i] = (Line **)calloc(parallelIO, sizeof(Line *));
    
       }

    }
  prefetchTrigger = std::numeric_limits<uint64_t>::max();

  evictMode = (EVICT_MODE)conf.readInt(CONFIG_ICL, ICL_EVICT_GRANULARITY);
  prefetchMode =
      (PREFETCH_MODE)conf.readInt(CONFIG_ICL, ICL_PREFETCH_GRANULARITY);

  // Set evict policy functional
  policy = (EVICT_POLICY)conf.readInt(CONFIG_ICL, ICL_EVICT_POLICY);
  //printf("Policy used: ")
 policyAtGc = POLICY_LFU;
  switch (policy) {
    case POLICY_RANDOM:
      evictFunction = [this](uint32_t, uint64_t &) -> uint32_t {
        return dist(gen);
      };
      compareFunction = [this](Line *a, Line *b) -> Line * {
        if (a && b) {
          return dist(gen) > waySize / 2 ? a : b;
        }
        else if (a || b) {
          return a ? a : b;
        }
        else {
          return nullptr;
        }
      };
      printf("CacheEvictionPolicy: Random\n");
      break;
    case POLICY_FIFO:
      evictFunction = [this](uint32_t setIdx, uint64_t &tick) -> uint32_t {
        uint32_t wayIdx = 0;
        uint64_t min = std::numeric_limits<uint64_t>::max();

        for (uint32_t i = 0; i < waySize; i++) {
          tick += getCacheLatency() * 8;
          // pDRAM->read(MAKE_META_ADDR(setIdx, i, offsetof(Line, insertedAt)),
          // 8, tick);

          if (cacheData[setIdx][i].insertedAt < min) {
            min = cacheData[setIdx][i].insertedAt;
            wayIdx = i;
          }
        }

        return wayIdx;
      };
      compareFunction = [](Line *a, Line *b) -> Line * {

        if (a && b) {
          if (a->insertedAt < b->insertedAt) {
            return a;
          }
          else {
            return b;
          }
        }
        else if (a || b) {
          return a ? a : b;
        }
        else {
          return nullptr;
        }
      };
      printf("CacheEvictionPolicy: FIFO\n");

      break;
    case POLICY_LEAST_RECENTLY_USED:
      evictFunction = [this](uint32_t setIdx, uint64_t &tick) -> uint32_t {
        uint32_t wayIdx = 0;
        uint64_t min = std::numeric_limits<uint64_t>::max();

        for (uint32_t i = 0; i < waySize; i++) {  // check through all the
          tick += getCacheLatency() * 8;
          // pDRAM->read(MAKE_META_ADDR(setIdx, i, offsetof(Line,
          // lastAccessed)), 8, tick);

          if (cacheData[setIdx][i].lastAccessed < min) {
            min = cacheData[setIdx][i].lastAccessed;
            wayIdx = i;
          }
        }

        return wayIdx;  // return the cache line which has least recenctly used
                        // timestamp.
      };
      compareFunction = [](Line *a, Line *b) -> Line * {
        if (a && b) {
          if (a->lastAccessed < b->lastAccessed) {
            return a;
          }
          else {
            return b;
          }
        }
        else if (a || b) {
          return a ? a : b;
        }
        else {
          return nullptr;
        }
      };
         printf("CacheEvictionPolicy: LRU\n");

      break;
      case POLICY_LRU_CLEAN:
       evictFunction = [this](uint32_t setIdx, uint64_t &tick) -> uint32_t {
        uint32_t wayIdx = 0;
        uint64_t min = std::numeric_limits<uint64_t>::max();
        //bool foundClean = false;  // Flag to track if a clean line is found
        for (uint32_t i = 0; i < waySize; i++) {
            tick += getCacheLatency() * 8;
            // Update min if needed for LRU
            if (cacheData[setIdx][i].lastAccessed < min) {
                min = cacheData[setIdx][i].lastAccessed;
                wayIdx = i;
            }
            // Check if the line is clean
            if (cacheData[setIdx][i].dirty == false) {
                //foundClean = true;
                TotalCleanEvicitons1++;
                wayIdx = i;  // Return the first clean line found based on LRU
                break;
            }
        }
        return wayIdx;  // Return the cache line that is LRU and clean (if found)
    };
     compareFunction = [](Line *a, Line *b) -> Line * {
    if (a && b) {
        if (a->dirty == false && b->dirty == false) {
            // Both are clean, prioritize based on LRU
            return a->lastAccessed < b->lastAccessed ? a : b;
        } else if (a->dirty == false) {
            // Only 'a' is clean
            return a;
        } else if (b->dirty == false) {
            // Only 'b' is clean
            return b;
        } else {
            // Both are dirty, prioritize based on LRU
            return a->lastAccessed < b->lastAccessed ? a : b;
        }
    } else if (a || b) {
        return a ? a : b;
    } else {
        return nullptr;
    }
};
         printf("CacheEvictionPolicy: Clean_LRU\n");

      break;
      case POLICY_LFU:
          evictFunction = [this](uint32_t setIdx, uint64_t &tick) -> uint32_t {
              uint32_t wayIdx = 0;
              uint64_t minAccessCount = std::numeric_limits<uint64_t>::max();
              uint64_t minLastAccessTime = std::numeric_limits<uint64_t>::max();

              for (uint32_t i = 0; i < waySize; i++) {
                  tick += getCacheLatency() * 8;

                  if (cacheData[setIdx][i].accessCount < minAccessCount) {
                      // Found a line with a lower access count, prioritize it
                      minAccessCount = cacheData[setIdx][i].accessCount;
                      minLastAccessTime = cacheData[setIdx][i].lastAccessed;
                      wayIdx = i;
                  } else if (cacheData[setIdx][i].accessCount == minAccessCount &&
                            cacheData[setIdx][i].lastAccessed < minLastAccessTime) {
                      // Found a line with the same access count but less recently accessed, prioritize it
                      minLastAccessTime = cacheData[setIdx][i].lastAccessed;
                      wayIdx = i;
                  }
              }

              return wayIdx;
          };

              compareFunction = [](Line *a, Line *b) -> Line * {
                    
                  if (a && b) {
                    //std::cout << "a: " << a->tag <<"  b: "<<b->tag<<"\t";
                      if (a->accessCount < b->accessCount) {
                          // If a has a higher access count
                          return a;
                      } else if (b->accessCount < a->accessCount) {
                          // If b has a higher access count
                          return b;
                      } else {
                          // If both have the same access count, compare based on lastAccessTime (LRU tie-breaking)
                          if (a->lastAccessed < b->lastAccessed) {
                              return a;
                          } else {
                              return b;
                          }
                      }
                      
                  } else if (a || b) {
                      return a ? a : b;
                  } else {
                      return nullptr;
                  }
              };

                      printf("CacheEvictionPolicy: LFU\n");
                    break;
              default:
                panic("Undefined cache evict policy");

                break;
  }
      switch(policyAtGc)
        {
          ///////This portion has been added////////////////////////////////
          case POLICY_LRU_CLEAN:
           evictFunction1 = [this](uint32_t setIdx, uint64_t &tick) -> uint32_t {
                  uint32_t wayIdx = 0;
                  uint64_t min = std::numeric_limits<uint64_t>::max();
                  //bool foundClean = false;  // Flag to track if a clean line is found
                  for (uint32_t i = 0; i < waySize; i++) {
                      tick += getCacheLatency() * 8;
                      // Update min if needed for LRU
                      if (cacheData[setIdx][i].lastAccessed < min) {
                          min = cacheData[setIdx][i].lastAccessed;
                          wayIdx = i;
                      }
                      // Check if the line is clean
                      if (cacheData[setIdx][i].dirty == false) {
                          //foundClean = true;
                          TotalCleanEvicitons1++;
                          wayIdx = i;  // Return the first clean line found based on LRU
                          break;
                      }
                  }
                  return wayIdx;  // Return the cache line that is LRU and clean (if found)
              };
            compareFunction1 = [](Line *a, Line *b) -> Line * {
                      //printf("This is the true thing\n");
                    if (a && b) {
                        if (a->dirty == false && b->dirty == false) {
                            // Both are clean, prioritize based on LRU
                            return a->lastAccessed < b->lastAccessed ? a : b;
                        } else if (a->dirty == false) {
                            // Only 'a' is clean
                            return a;
                        } else if (b->dirty == false) {
                            // Only 'b' is clean
                            return b;
                        } else {
                            // Both are dirty, prioritize based on LRU
                            return a->lastAccessed < b->lastAccessed ? a : b;
                        }
                    } else if (a || b) {
                        return a ? a : b;
                    } else {
                        return nullptr;
                    }
                };
                 printf("CacheEvictionPolicyAtGC: POLICY_LRU_CLEAN\n");
                  break;
          case POLICY_LFU:
                evictFunction1 = [this](uint32_t setIdx, uint64_t &tick) -> uint32_t {
                    uint32_t wayIdx = 0;
                    uint64_t minAccessCount = std::numeric_limits<uint64_t>::max();
                    uint64_t minLastAccessTime = std::numeric_limits<uint64_t>::max();

                    for (uint32_t i = 0; i < waySize; i++) {
                        tick += getCacheLatency() * 8;

                        if (cacheData[setIdx][i].accessCount < minAccessCount) {
                            // Found a line with a lower access count, prioritize it
                            minAccessCount = cacheData[setIdx][i].accessCount;
                            minLastAccessTime = cacheData[setIdx][i].lastAccessed;
                            wayIdx = i;
                        } else if (cacheData[setIdx][i].accessCount == minAccessCount &&
                                  cacheData[setIdx][i].lastAccessed < minLastAccessTime) {
                            // Found a line with the same access count but less recently accessed, prioritize it
                            minLastAccessTime = cacheData[setIdx][i].lastAccessed;
                            wayIdx = i;
                        }
                    }

                    return wayIdx;
                };

                    compareFunction1 = [](Line *a, Line *b) -> Line * {
                          
                        if (a && b) {
                          //std::cout << "a: " << a->tag <<"  b: "<<b->tag<<"\t";
                            if (a->accessCount < b->accessCount) {
                                // If a has a higher access count
                                return a;
                            } else if (b->accessCount < a->accessCount) {
                                // If b has a higher access count
                                return b;
                            } else {
                                // If both have the same access count, compare based on lastAccessTime (LRU tie-breaking)
                                if (a->lastAccessed < b->lastAccessed) {
                                    return a;
                                } else {
                                    return b;
                                }
                            }
                            
                        } else if (a || b) {
                            return a ? a : b;
                        } else {
                            return nullptr;
                        }
                    };
                     printf("CacheEvictionPolicyAtGC: POLICY_LFU\n");

              break;
                    default:
                            panic("Undefined cache evict policyAtGC");

                            break;
        
        }
///////This portion has been added////////////////////////////////
  memset(&stat, 0, sizeof(stat));
}

GenericCache::~GenericCache() {
  if (!useReadCaching && !useWriteCaching) {
    return;
  }

  for (uint32_t i = 0; i < setSize; i++) {
    delete[] cacheData[i];
  }

  for (uint32_t i = 0; i < lineCountInSuperPage; i++) {
    free(evictData[i]);
  }
}

uint64_t GenericCache::getCacheLatency() {
  static uint64_t latency = conf.readUint(CONFIG_ICL, ICL_CACHE_LATENCY);
  static uint64_t core = conf.readUint(CONFIG_CPU, CPU::CPU_CORE_ICL);

  return (core == 0) ? 0 : latency / core;
}

uint32_t GenericCache::calcSetIndex(uint64_t lca) {
  return lca % setSize;// actual set to flush
}

void GenericCache::calcIOPosition(uint64_t lca, uint32_t &row, uint32_t &col) {
  uint32_t tmp = lca % lineCountInMaxIO;

  row = tmp % lineCountInSuperPage;
  col = tmp / lineCountInSuperPage;
  //printf("For lca %lu Row is %u Col is %u LinecountMIX %u linesINSuperPage %u\n",lca,row,col,lineCountInMaxIO,lineCountInSuperPage);
}

uint32_t GenericCache::getEmptyWay(uint32_t setIdx, uint64_t &tick) { // the getEmptyWay function is responsible for finding an available empty way (cache entry) in the specified set of the cache. 
  uint32_t retIdx = waySize;
  uint64_t minInsertedAt = std::numeric_limits<uint64_t>::max();

  for (uint32_t wayIdx = 0; wayIdx < waySize; wayIdx++) {
    Line &line = cacheData[setIdx][wayIdx];

    if (!line.valid) {
      tick += getCacheLatency() * 8;
      // pDRAM->read(MAKE_META_ADDR(setIdx, wayIdx, offsetof(Line, insertedAt)),
      // 8, tick);

      if (minInsertedAt > line.insertedAt) {
        minInsertedAt = line.insertedAt;
        retIdx = wayIdx;
      }
    }
  }

  return retIdx;
}
uint32_t GenericCache::getGhostEmptyWay(uint32_t setIdx, uint64_t &tick) { // the getEmptyWay function is responsible for finding an available empty way (cache entry) in the specified set of the cache. 
  uint32_t retIdx = ghostWaySize;
  uint64_t minInsertedAt = std::numeric_limits<uint64_t>::max();

  for (uint32_t wayIdx = 0; wayIdx < ghostWaySize; wayIdx++) {
    Line &line = ghostCacheData[setIdx][wayIdx];

    if (!line.valid) {
      tick += getCacheLatency() * 8;
      // pDRAM->read(MAKE_META_ADDR(setIdx, wayIdx, offsetof(Line, insertedAt)),
      // 8, tick);

      if (minInsertedAt > line.insertedAt) {
        minInsertedAt = line.insertedAt;
        retIdx = wayIdx;
      }
    }
  }

  return retIdx;
}
uint32_t GenericCache::getEvictedWay(uint32_t setIdx, uint64_t lca) { // the getEmptyWay function is responsible for finding an available empty way (cache entry) in the specified set of the cache. 
  uint32_t retIdx = waySize;
  

  for (uint32_t wayIdx = 0; wayIdx < waySize; wayIdx++) {
    Line &line = cacheData[setIdx][wayIdx];

    if (line.tag==lca) {
      //tick += getCacheLatency() * 8;
      // pDRAM->read(MAKE_META_ADDR(setIdx, wayIdx, offsetof(Line, insertedAt)),
      // 8, tick);
        retIdx=wayIdx;
        break;
     
    }
  }

  return retIdx;
}

uint32_t GenericCache::getValidWay(uint64_t lca, uint64_t &tick) {
  uint32_t setIdx = calcSetIndex(lca);
  uint32_t wayIdx;

  for (wayIdx = 0; wayIdx < waySize; wayIdx++) {
    Line &line = cacheData[setIdx][wayIdx];
  
    tick += getCacheLatency() * 8;
    // pDRAM->read(MAKE_META_ADDR(setIdx, wayIdx, offsetof(Line, tag)), 8,
    // tick);

    if (line.valid && line.tag == lca) {
      break;
    }
  }

  return wayIdx;
}
uint32_t GenericCache::getNewValidWay(uint64_t lca, uint64_t &tick) {
  uint32_t setIdx = calcSetIndex(lca);
  uint32_t wayIdx;

  for (wayIdx = 0; wayIdx < ghostWaySize; wayIdx++) {
    Line &line = ghostCacheData[setIdx][wayIdx];
   // printf("Tag: %lu Dirty %d LastA %lu Insert %lu Valid %d\n",line.tag,line.dirty,line.lastAccessed,line.insertedAt,line.valid);
  
    tick += getCacheLatency() * 8;
    // pDRAM->read(MAKE_META_ADDR(setIdx, wayIdx, offsetof(Line, tag)), 8,
    // tick);

    if (line.valid && line.tag == lca) {
      break;
    }
  }

  return wayIdx;
}
bool GenericCache::updateDirty(uint64_t lca, uint64_t &tick) {
  uint32_t setIdx = calcSetIndex(lca);
  uint32_t wayIdx;
bool ret=false;
caching.dirtyUpdateRequests++;
   wayIdx = getValidWay(lca, tick);// geting a valid way
    if (wayIdx != waySize) { // some space is avaiable in the cache to absorbe writes
      
     // stat.request[1]++;  //writing to the cache for updating the stats.
      totalDirtyStatusUpdate++;
      cacheData[setIdx][wayIdx].dirty = true;
      cacheData[setIdx][wayIdx].accessCount++;
      dirtyStatusCounter++;
     // PageReuseBucket[cacheData[setIdx][wayIdx].accessCount]++;

      // DRAM access
      //pDRAM->write(&cacheData[setIdx][wayIdx],lineSize, tick);// cache hit and write

         //tick += applyLatency(CPU::ICL__GENERIC_CACHE, CPU::WRITE);

      ret = true;
    }
    
   return ret;
}

void GenericCache::checkSequential(Request &req, SequentialDetect &data) {
                  //printf(" Lst LPN %lu CurrentLPN %lu Diff %lu\n",data.lastRequest.range.slpn,req.range.slpn,(req.range.slpn-data.lastRequest.range.slpn));

  if (data.lastRequest.reqID == req.reqID) {
    data.lastRequest.range = req.range;
    data.lastRequest.offset = req.offset;
    data.lastRequest.length = req.length;
    data.lastRequest.range.slpn=req.range.slpn;

    return;
  }
/*if (data.lastRequest.range.slpn * lineSize + data.lastRequest.offset +
          data.lastRequest.length ==
      req.range.slpn * lineSize + req.offset) */
 //**************************************************** Old Implemented******************** 
   //printf("PrefectectIOcount %u Range %lu Offset %lu\n",prefetchIOCount,req.range.slpn,req.offset);
       // printf("PrefectectIOcount %u Range %lu Offset %lu req.reqID: %lu req.length: %lu\n",prefetchIOCount,req.range.slpn,req.offset,req.reqID,req.length);
       //printf("data.lastRequest.range.slpn * lineSize + data.lastRequest.offse  %lu req.range.slpn * lineSize + req.offset %lu\n",(data.lastRequest.range.slpn * lineSize + data.lastRequest.offset +
          //data.lastRequest.length),(req.range.slpn * lineSize + req.offset));

 if (data.lastRequest.range.slpn * lineSize + data.lastRequest.offset +
          data.lastRequest.length ==
      req.range.slpn * lineSize + req.offset ) 
      {
    if (!data.enabled) {
      data.hitCounter++;
      data.accessCounter += data.lastRequest.offset + data.lastRequest.length;

      if (data.hitCounter >= prefetchIOCount &&
          (float)data.accessCounter / superPageSize >= prefetchIORatio) {
        data.enabled = true;
        caching.prefetcherActivationCounter++;
       // printf("data.hitCounter %u data.accessCounter %u prefetchIORatio %f data.lastRequest.offset %lu data.lastRequest.length %lu LPN %lu \n",data.hitCounter,data.accessCounter,prefetchIORatio,data.lastRequest.offset,data.lastRequest.length,req.range.slpn);
      }
    }
  }
  else {
    data.enabled = false;
    data.hitCounter = 0;
    data.accessCounter = 0;
     //**************************************************** Old Implemented******************** 
  }
     
  data.lastRequest = req;
}


void GenericCache::ghostEviction(uint64_t tick, bool flush) {
  FTL::Request reqInternal(lineCountInSuperPage);
  uint64_t beginAt;
  uint64_t finishedAt = tick;

  debugprint(LOG_ICL_GENERIC_CACHE, "----- | Begin eviction");

  for (uint32_t row = 0; row < lineCountInSuperPage; row++) {
    for (uint32_t col = 0; col < parallelIO; col++) {
      beginAt = tick;

      if (ghostEvictData[row][col] == nullptr) {
        continue;
      }
        totalEvictions++;
        ghostCache.ghostCacheTotalEvictions++;
      if (ghostEvictData[row][col]->valid && ghostEvictData[row][col]->dirty) {
        reqInternal.lpn = ghostEvictData[row][col]->tag / lineCountInSuperPage;
        reqInternal.ioFlag.reset();
        reqInternal.ioFlag.set(row);
        totalDirtyEvictions++;
        ghostCache.ghostCacheDirtyEvictions++;
        
    // printf("GhostEviction %lu\n",reqInternal.lpn);
              pFTL->write(reqInternal, beginAt);
      }

      if (flush) {
        ghostEvictData[row][col]->valid = false;
        ghostEvictData[row][col]->tag = 0;
      }

      ghostEvictData[row][col]->insertedAt = beginAt;
      ghostEvictData[row][col]->lastAccessed = beginAt;
      ghostEvictData[row][col]->dirty = false;
      ghostEvictData[row][col] = nullptr;

       

      finishedAt = MAX(finishedAt, beginAt);
    }
  }

  debugprint(LOG_ICL_GENERIC_CACHE,
             "----- | End eviction | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")",
             tick, finishedAt, finishedAt - tick);

}
void GenericCache::updateGhostCacheForWrites(uint32_t setIdx,uint32_t wayIdx,uint64_t &tick)
{
           
      if (tick < ghostCacheData[setIdx][wayIdx].insertedAt) {
        tick = ghostCacheData[setIdx][wayIdx].insertedAt;
      }

      
        // Update last accessed time
        ghostCacheData[setIdx][wayIdx].insertedAt = tick;
        ghostCacheData[setIdx][wayIdx].lastAccessed = tick;
     
      // Update last accessed time
      ghostCacheData[setIdx][wayIdx].dirty = true;
      ghostCacheData[setIdx][wayIdx].accessCount++;
     // cacheData[setIdx][wayIdx].loadedFromBlock=req.loadedFromBlock;
     // DRAM access
      pDRAM->write(&ghostCacheData[setIdx][wayIdx], lineSize, tick);// cache hit and write
}
 void GenericCache::updateGhostCacheForReads(uint32_t setIdx,uint32_t wayIdx,uint64_t &tick)
 {
     //uint64_t tickBackup = tick;

      // Wait cache to be valid
      if (tick < ghostCacheData[setIdx][wayIdx].insertedAt) {
        tick = ghostCacheData[setIdx][wayIdx].insertedAt;
      }

      // Update last accessed time
      ghostCacheData[setIdx][wayIdx].lastAccessed = tick;
      ghostCacheData[setIdx][wayIdx].accessCount++;

      // DRAM access
      pDRAM->read(&ghostCacheData[setIdx][wayIdx],lineSize , tick);// 

     
 }

void GenericCache::evictCache(uint64_t tick, bool flush) { //@whm
  FTL::Request reqInternal(lineCountInSuperPage);
  liner=lineCountInSuperPage;
  uint64_t beginAt;
  uint64_t finishedAt = tick;
 // printEvictData();
  debugprint(LOG_ICL_GENERIC_CACHE, "----- | Begin eviction");
  for (uint32_t row = 0; row < lineCountInSuperPage; row++) {
    for (uint32_t col = 0; col < parallelIO; col++) { //for (uint32_t col = 0; col < parallelIO; col++) changed here
      beginAt = tick;
          totalTimesEvictionCalled++;

      if (evictData[row][col] == nullptr) {
        continue;
      }
      totalEvictions++;
      lbaEvictionFrequency[evictData[row][col]->tag]++;
      caching.lastEvictedTick=evictData[row][col]->lastAccessed;//// this is used to keep the track of the lastEvictedData.
      //printf("Evicted LPN %lu LASTACCESSEDAT %lu\n",evictData[row][col]->tag,caching.lastEvictedTick);
                if(doEvictionByGC)
                {
                    pageEvictionIndicator[evictData[row][col]->tag]=true;
                        }
                else
                {
                  pageEvictionIndicator[evictData[row][col]->tag]=false;
                          }
      if (evictData[row][col]->valid && evictData[row][col]->dirty) {
        reqInternal.lpn = evictData[row][col]->tag / lineCountInSuperPage;
        reqInternal.ioFlag.reset();
        reqInternal.ioFlag.set(row);
        totalDirtyEvictions++;
       // printf(" DirtyEviction Of LPN %lu\n",evictData[row][col]->tag);

        /*if(evictData[row][col]->loadedFromBlock)
        {
          PageReuseBucket[evictData[row][col]->accessCount]++;
        }*/
        //frequencyBucket[evictData[row][col]->accessCount]++;
        pFTL->DecachePage(reqInternal.lpn);
        globalLBaReuseMap[reqInternal.lpn]=evictData[row][col]->accessCount;
        //LbaAccessFrequency[evictData[row][col]->accessCount]++;
        pFTL->write(reqInternal, beginAt);
        
      }
      else
      {
        TotalCleanEvicitons2++;
        //printf("CleanEviction Of LPN %lu\n",evictData[row][col]->tag);
        //frequencyBucket[evictData[row][col]->accessCount]++;
        pFTL->DecachePage(evictData[row][col]->tag);
      }

      if (flush) {
        evictData[row][col]->valid = false;
        evictData[row][col]->tag = 0;
        evictData[row][col]->accessCount=0;
      }

      evictData[row][col]->insertedAt = beginAt;
      evictData[row][col]->lastAccessed = beginAt;//
      evictData[row][col]->dirty = false;
      evictData[row][col]->loadedFromBlock=false;
      evictData[row][col] = nullptr;
      finishedAt = MAX(finishedAt, beginAt);
      if(getin)
        {
            printf("Eviction Done from Cache\n");
        }
       // break;
    }
  }
      doEvictionByGC=false;
  debugprint(LOG_ICL_GENERIC_CACHE,
             "----- | End eviction | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")",
             tick, finishedAt, finishedAt - tick);
         
}
void GenericCache::modifedEviction(uint64_t &tick,uint64_t lca) {
  FTL::Request reqInternal(lineCountInSuperPage);
  uint64_t beginAt;
  uint64_t finishedAt = tick;
//printf("Modified Eviction called\n");
  debugprint(LOG_ICL_GENERIC_CACHE, "----- | Begin eviction");
  
      beginAt = tick;
        reqInternal.lpn = lca;
        reqInternal.ioFlag.reset();
        reqInternal.ioFlag.set(lca % lineCountInSuperPage);// modified evictions needed to be performed
        pFTL->write(reqInternal, beginAt);// this is the write operation to the nand flash
        printf("Eviction with %lu done\n", reqInternal.lpn );
finishedAt = MAX(finishedAt, beginAt);
  debugprint(LOG_ICL_GENERIC_CACHE,
             "----- | End eviction | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")",
             tick, finishedAt, finishedAt - tick);
           
//tick=finishedAt;
         
}

// True when hit
bool GenericCache::read(Request &req, uint64_t &tick) {
  bool ret = false;
   // printf("IncomingRead: %lu Tick: %ld Length %lu\n",req.range.slpn,tick,req.length);

//printIncommingRequest(req);
  debugprint(LOG_ICL_GENERIC_CACHE,
             "READ  | REQ %7u-%-4u | LCA %" PRIu64 " | SIZE %" PRIu64,
             req.reqID, req.reqSubID, req.range.slpn, req.length);
//printf("The ICL Count %ld\n",iclCount);

  if (useReadCaching) {
    uint32_t setIdx = calcSetIndex(req.range.slpn);// this is ok..getting the actual set Id
    uint32_t wayIdx; 
    uint64_t arrived = tick;

    if (useReadPrefetch) {
      checkSequential(req, readDetect);
    }

    wayIdx = getValidWay(req.range.slpn, tick);//this is ok// this will give the way number where from we have to read.
    // Do we have valid data?
    if (wayIdx != waySize) {// if read hit..
      uint64_t tickBackup = tick;

      // Wait cache to be valid
      if (tick < cacheData[setIdx][wayIdx].insertedAt) {
        tick = cacheData[setIdx][wayIdx].insertedAt;
      }

      // Update last accessed time
      cacheData[setIdx][wayIdx].lastAccessed = tick;
      cacheData[setIdx][wayIdx].accessCount++;

      // DRAM access
      pDRAM->read(&cacheData[setIdx][wayIdx], req.length, tick);// 

      debugprint(LOG_ICL_GENERIC_CACHE,
                 "READ  | Cache hit at (%u, %u) | %" PRIu64 " - %" PRIu64
                 " (%" PRIu64 ")",
                 setIdx, wayIdx, arrived, tick, tick - arrived);

      ret = true; //return true if the read is found in the cache;
       caching.pageReadHitsInMainCache++;

      // Do we need to prefetch data?
      if (useReadPrefetch && req.range.slpn == prefetchTrigger) {
        debugprint(LOG_ICL_GENERIC_CACHE, "READ  | Prefetch triggered");

        req.range.slpn = lastPrefetched;

        // Backup tick
        arrived = tick;
        tick = tickBackup;
        goto ICL_GENERIC_CACHE_READ;// this is for prefetching the data.
      }/// this is read portion ok with respect to me..
    }
    // We should read data from NVM
    else {  // if it is not present in the cache then read from nvm and place in the Ram with some empty slot.
    ICL_GENERIC_CACHE_READ:
      FTL::Request reqInternal(lineCountInSuperPage, req);
      std::vector<std::pair<uint64_t, uint64_t>> readList;
      uint32_t row, col;  // Variable for I/O position (IOFlag)
      uint64_t dramAt;
      uint64_t beginLCA, endLCA;
      uint64_t beginAt, finishedAt = tick;

      if (readDetect.enabled) {
        // TEMP: Disable DRAM calculation for prevent conflict
        pDRAM->setScheduling(false);

        if (!ret) {
          debugprint(LOG_ICL_GENERIC_CACHE, "READ  | Read ahead triggered");
        }

        beginLCA = req.range.slpn;
        //printf(" super********beginLCA %lu endLCA %lu lineCountInMaxIO %u prefetchTrigger %lu lineCountInSuperPage %u lastPrefetched %lu\n",beginLCA,endLCA,lineCountInMaxIO,prefetchTrigger,lineCountInSuperPage,lastPrefetched);

        // If super-page is disabled, just read all pages from all planes
        if (prefetchMode == MODE_ALL || !bSuperPage) {
          endLCA = beginLCA + lineCountInMaxIO;
          prefetchTrigger = beginLCA + lineCountInMaxIO / 2;

        }
        else {
          endLCA = beginLCA + lineCountInSuperPage;
          prefetchTrigger = beginLCA + lineCountInSuperPage / 2;

        }

        lastPrefetched = endLCA;
      }
      else { 
        beginLCA = req.range.slpn;
        endLCA = beginLCA + 1;
      }

      for (uint64_t lca = beginLCA; lca < endLCA; lca++) {
        beginAt = tick;
        //printf("Insde the loop with LCA %lu \t",lca);
        // Check cache
        if (getValidWay(lca, beginAt) != waySize) { //  agar ye cache ma hai tw kuchmat karo
          continue;
        }

  
        // Find way to write data read from NVM
        setIdx = calcSetIndex(lca);
        wayIdx = getEmptyWay(setIdx, beginAt);// this portion is also ok..
       //printf("Empty way returned %u lpn %lu\n",wayIdx,lca);
          /*#################################################################*/
          if (wayIdx == waySize) {
          wayIdx = evictFunction(setIdx, beginAt);

          //if (cacheData[setIdx][wayIdx].dirty) {
            // We need to evict data before write
            calcIOPosition(cacheData[setIdx][wayIdx].tag, row, col);
            evictData[row][col] = cacheData[setIdx] + wayIdx;
                      evictCache(tick);

         // }
        }
          /*#################################################################*/
        /*if (wayIdx == waySize) {// if the cache is full
        //printf("WayId Before %u\n",wayIdx);
          wayIdx = evictFunction(setIdx, beginAt);// gives the way to be evicted from a given set.
          //printf("EvictedWay Returned %u\n",wayIdx);
            calcIOPosition(cacheData[setIdx][wayIdx].tag, row, col);
            //printf("Way selected for eviction %lu Col %u Row %u\n",cacheData[setIdx][wayIdx].tag,col,row);
            evictData[row][col] = cacheData[setIdx] + wayIdx;
           // printf("Evicted Entry contains %lu Dirty :%d Valid: %d\n",evictData[row][col]->tag,evictData[row][col]->dirty,evictData[row][col]->valid);
          //}
          evictCache(tick);
        }*/
        //printf("Before Contents of evictionCache\n");
        //printEvictData();
        cacheData[setIdx][wayIdx].insertedAt = beginAt;
        cacheData[setIdx][wayIdx].lastAccessed = beginAt;
        cacheData[setIdx][wayIdx].valid = true;
        cacheData[setIdx][wayIdx].dirty = false;

        readList.push_back({lca, ((uint64_t)setIdx << 32) | wayIdx});// this is the IO i have to read from the flash
        //printf("Rad list is crated witht he size %ld\n",readList.size());

        finishedAt = MAX(finishedAt, beginAt);
        //printf("Just after-------\n");
     //print();
      }
     // printf("\n");
      tick = finishedAt;

      evictCache(tick);
    // eviction from the cache will take place...code ok

      for (auto &iter : readList) {
        Line *pLine = &cacheData[iter.second >> 32][iter.second & 0xFFFFFFFF];

        // Read data
        reqInternal.lpn = iter.first / lineCountInSuperPage;// ok
        reqInternal.ioFlag.reset();
        reqInternal.ioFlag.set(iter.first % lineCountInSuperPage);

        beginAt = tick;  // Ignore cache metadata access
        pFTL->read(reqInternal, beginAt);// this is the data to be read from the block.
       // printf("Page is read from the block %d\n",block_index);
        if(pageEvictionIndication(reqInternal.lpn))// if the page that has been again red from the cache is rereferenced after the eviction by GC.
        {
              caching.pageEvictedByGcAgainAccessed++;
        }
        if(!pageEvictedByHost(reqInternal.lpn))
        {
         caching.pageEvictedByHostAgainAccessed++; 
        }
        // DRAM delay
        dramAt = pLine->insertedAt;
        pDRAM->write(pLine, lineSize, dramAt);

        // Set cache data
        beginAt = MAX(beginAt, dramAt);

        pLine->insertedAt = beginAt;
        pLine->lastAccessed = beginAt;
        //printf("BeginAt %lu\n",beginAt);
        //pLine->lastAccessed = (beginAt+caching.lastEvictedTick)/2;
        pLine->tag = iter.first;
        pLine->valid=true;// this is the updated version.
            //pageisPresentInSSD=false;
                
        pLine->accessCount=1;

        if (pLine->tag == req.range.slpn) {
          finishedAt = beginAt;
        }
            //printf(" after Read Internal\n");
        
        //print();
        block_index= -1;
        debugprint(LOG_ICL_GENERIC_CACHE,
                   "READ  | Cache miss at (%u, %u) | %" PRIu64 " - %" PRIu64
                   " (%" PRIu64 ")",
                   iter.second >> 32, iter.second & 0xFFFFFFFF, tick, beginAt,
                   beginAt - tick);
      }

      tick = finishedAt;

      if (readDetect.enabled) {
        if (ret) {
          // This request was prefetch
          debugprint(LOG_ICL_GENERIC_CACHE, "READ  | Prefetch done");

          // Restore tick
          tick = arrived;
        }
        else {
          debugprint(LOG_ICL_GENERIC_CACHE, "READ  | Read ahead done");
        }

        // TEMP: Restore
        pDRAM->setScheduling(true);
      }
    }

    tick += applyLatency(CPU::ICL__GENERIC_CACHE, CPU::READ);
  }//if reading cache used or not
  else {
    FTL::Request reqInternal(lineCountInSuperPage, req);
        
    pDRAM->write(nullptr, req.length, tick);

    pFTL->read(reqInternal, tick);//this is 
  }
 

  stat.request[0]++; // overall number of ICL Read Requests

  if (ret) {
    stat.cache[0]++; // read hit
  }
//

   // print();
//printf("readingOwn %lu at %lu\n",req.range.slpn,tick);
caching.mainCachePageReadRequest++;
  return ret;
}
void GenericCache::print()
{
    printf("--Contents of ActualCache--------\n");

for(uint32_t i=0;i<setSize;i++)
      {
       for (uint32_t j = 0; j < waySize; j++) {
        Line& line = cacheData[i][j];

        std::cout << "Line " << j << ": ";
        std::cout << "Tag: " << line.tag << ", Last Accessed: " << line.lastAccessed
                  << ", Inserted At: " << line.insertedAt << ", Dirty: " << line.dirty
                  << ", Valid: " << line.valid 
                  << ", AccessCount: "<<line.accessCount
                  << ", LoadedFromBlock: "<<line.loadedFromBlock;
        // Print more information if needed...
        std::cout << std::endl;
         std::cout << std::endl;
      }
    }
}
void GenericCache::printEvictData()
{
  printf("-----Contents of evictionCache--------\n");
for (size_t row = 0; row < evictData.size(); ++row) {
    for (size_t col = 0; col < parallelIO; ++col) {
        Line** linePtr = evictData[row];
        
        if (linePtr && linePtr[col]) {
            Line* line = linePtr[col];
            std::cout << "Row: " << row << ", Col: " << col ;
            std::cout << " Tag: " << line->tag;
            std::cout << " Last Accessed: " << line->lastAccessed ;
            std::cout << " Inserted At: " << line->insertedAt ;
            std::cout << " Dirty: " << (line->dirty ? "true" : "false") ;
            std::cout << " Valid: " << (line->valid ? "true" : "false")<<endl ;
            //std::cout << " Block ID: " << line->bId << std::endl;
            //std::cout << "---------------------" << std::endl;
        }
    }
}

}

bool GenericCache:: writeVictimPagesToCache(Request &req,uint64_t &tick)
{
      
  bool ret = false;
  uint64_t flash = tick;
  bool dirty = false;
  debugprint(LOG_ICL_GENERIC_CACHE,
             "WRITE | REQ %7u-%-4u | LCA %" PRIu64 " | SIZE %" PRIu64,
             req.reqID, req.reqSubID, req.range.slpn, req.length);

  FTL::Request reqInternal(lineCountInSuperPage, req);

  if (req.length <= lineSize) {
    dirty = true;
  }
  else {
    pFTL->write(reqInternal, flash);// direct write the the storage.
    
  }

  if (useWriteCaching) {
    uint32_t setIdx = calcSetIndex(req.range.slpn);// this will give it a set id where the write operation will take place.
    
    uint32_t wayIdx;
    
      uint64_t arrived = tick;

      wayIdx = getEmptyWay(setIdx, tick);// get an empty way for the current code is ok.

      if (wayIdx != waySize) { // cache miss with cache line avaiable
        // Wait cache to be valid
        if (tick < cacheData[setIdx][wayIdx].insertedAt) {
          tick = cacheData[setIdx][wayIdx].insertedAt;
        }

        // TODO: TEMPORAL CODE
        // We should only show DRAM latency when cache become dirty
        if (dirty) {
          // Update last accessed time
          cacheData[setIdx][wayIdx].insertedAt = tick;
          cacheData[setIdx][wayIdx].lastAccessed = tick;
          //cacheData[setIdx][wayIdx].lastAccessed = (tick + caching.lastEvictedTick)/2;  //insert the data at the middle of the lru list
        }
        else {
          cacheData[setIdx][wayIdx].insertedAt = flash;
          cacheData[setIdx][wayIdx].lastAccessed = flash;
        }

        // Update last accessed time
        cacheData[setIdx][wayIdx].valid = true;
        cacheData[setIdx][wayIdx].dirty = dirty;
        cacheData[setIdx][wayIdx].tag = req.range.slpn;
        cacheData[setIdx][wayIdx].accessCount++;
        cacheData[setIdx][wayIdx].loadedFromBlock=req.loadedFromBlock;

        // DRAM access
        pDRAM->write(&cacheData[setIdx][wayIdx], req.length, tick);
        caching.mainCacheWritesWithoutFlush++;
        ret = true;
      }
      // We have to flush
      else { //if way returned is equal to the size of the cache .
        uint32_t row, col;  // Variable for I/O position (IOFlag)
        uint32_t setToFlush = calcSetIndex(req.range.slpn);// code is ok


        for (setIdx = 0; setIdx < setSize; setIdx++) {
          for (wayIdx = 0; wayIdx < waySize; wayIdx++) {
            if (cacheData[setIdx][wayIdx].valid) {
              calcIOPosition(cacheData[setIdx][wayIdx].tag, row, col);// ok returns the value for row and colums
              evictData[row][col] = compareFunction(evictData[row][col],cacheData[setIdx] + wayIdx);

            }
          }
        }
            
        if (evictMode == MODE_SUPERPAGE) {
          uint32_t row, col;  // Variable for I/O position (IOFlag)

          for (row = 0; row < lineCountInSuperPage; row++) {
            for (col = 0; col < parallelIO - 1; col++) {
              evictData[row][col + 1] =
                  compareFunction(evictData[row][col], evictData[row][col + 1]);
              evictData[row][col] = nullptr;
            }
          }
        }

        // We must flush setToFlush set
        bool have = false;

        for (row = 0; row < lineCountInSuperPage; row++) {
          for (col = 0; col < parallelIO; col++) {
            if (evictData[row][col] && calcSetIndex(evictData[row][col]->tag) == setToFlush) {
              have = true;
            }
          }
        }

        // We don't have setToFlush
         Line *pLineToFlush = nullptr;
        if (!have) {// agr huma eviction k liye kuch nai mila 
         

          for (wayIdx = 0; wayIdx < waySize; wayIdx++) {
            if (cacheData[setToFlush][wayIdx].valid) {
              pLineToFlush =
                  compareFunction(pLineToFlush, cacheData[setToFlush] + wayIdx);
            }
          }

          if (pLineToFlush) {
            calcIOPosition(pLineToFlush->tag, row, col);

            evictData[row][col] = pLineToFlush;
          }
        }
   

        tick += getCacheLatency() * setSize * waySize * 8;

    
          doEvictionByGC=true; // this is set when we are doing eviction becuuse of GC
        evictCache(tick, true);//this is ok code no need 
 
        // Update cacheline of current request
        setIdx = setToFlush;
        wayIdx = getEmptyWay(setIdx, tick);

        if (wayIdx == waySize) {
          panic("Cache corrupted!");
        }

        // DRAM latency
        pDRAM->write(&cacheData[setIdx][wayIdx], req.length, tick);

        // Update cache data
        cacheData[setIdx][wayIdx].insertedAt = tick;
       cacheData[setIdx][wayIdx].lastAccessed = tick; //cacheData[setIdx][wayIdx].lastAccessed = (tick + caching.lastEvictedTick)/2;
        //cacheData[setIdx][wayIdx].lastAccessed = (tick + caching.lastEvictedTick)/2; // entered element is inserted at the middle of mru positition
        cacheData[setIdx][wayIdx].valid = true;
        cacheData[setIdx][wayIdx].dirty = true;
        cacheData[setIdx][wayIdx].tag = req.range.slpn;
        cacheData[setIdx][wayIdx].accessCount=1;
        cacheData[setIdx][wayIdx].loadedFromBlock=req.loadedFromBlock;
       // printf("in the start write done on the set after eviction: %u & Way %u\n",setIdx,wayIdx);
      }  // marks the end of the code when the cache is full and there is no way:
         // avaialabe to handle the write so eviction has to take place.

      debugprint(LOG_ICL_GENERIC_CACHE,
                 "WRITE | Cache miss at (%u, %u) | %" PRIu64 " - %" PRIu64
                 " (%" PRIu64 ")",
                 setIdx, wayIdx, arrived, tick, tick - arrived);
    //}// this is part cache was full and the eviction takes place.
    //tick += applyLatency(CPU::ICL__GENERIC_CACHE, CPU::WRITE);
  }
  else {
    if (dirty) {
      pFTL->write(reqInternal, tick);
    }
    else {
      tick = flash;
    }

    // TEMP: Disable DRAM calculation for prevent conflict
    pDRAM->setScheduling(false);

    pDRAM->read(nullptr, req.length, tick);

    pDRAM->setScheduling(true);
  }

  stat.request[1]++;//overall write requests

  if (ret) {
    stat.cache[1]++;//writerequsts that that updated the same value in the cache//write hit
  }
    caching.mainCachePageWriteRequest++;

  return ret;
}
bool GenericCache::pageEvictionIndication(uint64_t lpn)// this will tell us if the page is evicted by the GC or by Host,, it is populated in the EvictCache function.
{
  auto iter= pageEvictionIndicator.find(lpn);
  bool temp=false;
  if(iter !=pageEvictionIndicator.end())
  {
    temp = iter->second;
  }
  return temp;
}
bool GenericCache::pageEvictedByHost(uint64_t lpn)// this will tell us if the page is evicted by the GC or by Host,, it is populated in the EvictCache function.
{
  auto iter= pageEvictionIndicator.find(lpn);
  bool temp=true;
  if(iter !=pageEvictionIndicator.end())
  {
    temp = iter->second;
  }
  return temp;
}
// True when cold-miss/hit
bool GenericCache::write(
    Request &req,
    uint64_t &tick) {  // this generic cache will handle the eviction and
                       // placement of requests in the cache
  // printf("IncomingWrite: %lu Tick: %ld Length %lu\n",req.range.slpn,tick,req.length);
      
  bool ret = false;
  uint64_t flash = tick;
  bool dirty = false;
 //printf("\nIncomming %lu IOCount %ld\n",req.range.slpn,myIoCount);
//printIncommingRequest(req);
  debugprint(LOG_ICL_GENERIC_CACHE,
             "WRITE | REQ %7u-%-4u | LCA %" PRIu64 " | SIZE %" PRIu64,
             req.reqID, req.reqSubID, req.range.slpn, req.length);

  FTL::Request reqInternal(lineCountInSuperPage, req);

  if (req.length <= lineSize) {
    dirty = true;
  }
  else {
    pFTL->write(reqInternal, flash);// direct write the the storage.
    
  }

  if (useWriteCaching) {
    uint32_t setIdx = calcSetIndex(req.range.slpn);// this will give it a set id where the write operation will take place.
    
    uint32_t wayIdx;
    wayIdx = getValidWay(req.range.slpn, tick);//  if the requested Address is in the cache;
    if (wayIdx != waySize) { // some space is avaiable in the cache to absorbe writes
      uint64_t arrived = tick;
      if (tick < cacheData[setIdx][wayIdx].insertedAt) {
        tick = cacheData[setIdx][wayIdx].insertedAt;
      }

      // TODO: TEMPORAL CODE
      // We should only show DRAM latency when cache become dirty
      if (dirty) {
        // Update last accessed time
        cacheData[setIdx][wayIdx].insertedAt = tick;
        cacheData[setIdx][wayIdx].lastAccessed = tick;
      }
      else {
        cacheData[setIdx][wayIdx].insertedAt = flash;
        cacheData[setIdx][wayIdx].lastAccessed = flash;
      }

      // Update last accessed time
      cacheData[setIdx][wayIdx].dirty = dirty;
      cacheData[setIdx][wayIdx].accessCount++;
      cacheData[setIdx][wayIdx].loadedFromBlock=req.loadedFromBlock;


      // DRAM access
      pDRAM->write(&cacheData[setIdx][wayIdx], req.length, tick);// cache hit and write
      caching.pageWriteHitsInMainCache++;
      debugprint(
          LOG_ICL_GENERIC_CACHE,
          "WRITE | Cache hit at (%u, %u) | %" PRIu64 " - %" PRIu64 " (%" PRIu64
          ")",
          setIdx, wayIdx, arrived, tick,
          tick - arrived);  // it indicates that the cache access was a hit
//printf("write ka pehla part\n");
//printf("writeHit for LPn %lu and ICL %ld\n",req.range.slpn,iclCount);
      ret = true;
    }// this was the cache overwrite type of operation
    else {  // when the cache miss occurs and there is an empty way to handle
            // the write request.
      uint64_t arrived = tick;

      wayIdx = getEmptyWay(setIdx, tick);// get an empty way for the current code is ok.

      if (wayIdx != waySize) { // cache miss with cache line avaiable
        // Wait cache to be valid
        if (tick < cacheData[setIdx][wayIdx].insertedAt) {
          tick = cacheData[setIdx][wayIdx].insertedAt;
        }

        // TODO: TEMPORAL CODE
        // We should only show DRAM latency when cache become dirty
        if (dirty) {
          // Update last accessed time
          cacheData[setIdx][wayIdx].insertedAt = tick;
          cacheData[setIdx][wayIdx].lastAccessed = tick;
        }
        else {
          cacheData[setIdx][wayIdx].insertedAt = flash;
          cacheData[setIdx][wayIdx].lastAccessed = flash;
        }

        // Update last accessed time
        cacheData[setIdx][wayIdx].valid = true;
        cacheData[setIdx][wayIdx].dirty = dirty;
        cacheData[setIdx][wayIdx].tag = req.range.slpn;
        cacheData[setIdx][wayIdx].accessCount++;
        cacheData[setIdx][wayIdx].loadedFromBlock=req.loadedFromBlock;

        // DRAM access
        pDRAM->write(&cacheData[setIdx][wayIdx], req.length, tick);
        caching.mainCacheWritesWithoutFlush++;
        ret = true;
      }
      // We have to flush
      else { //if way returned is equal to the size of the cache .
        uint32_t row, col;  // Variable for I/O position (IOFlag)
        uint32_t setToFlush = calcSetIndex(req.range.slpn);// code is ok


        for (setIdx = 0; setIdx < setSize; setIdx++) {
          for (wayIdx = 0; wayIdx < waySize; wayIdx++) {
            if (cacheData[setIdx][wayIdx].valid) {
              calcIOPosition(cacheData[setIdx][wayIdx].tag, row, col);// ok returns the value for row and colums
              evictData[row][col] = compareFunction(evictData[row][col],cacheData[setIdx] + wayIdx);

            }
          }
        }
            //printf("JustBeforeEvictions\n");
            //printEvictData();

        if (evictMode == MODE_SUPERPAGE) {
          uint32_t row, col;  // Variable for I/O position (IOFlag)

          for (row = 0; row < lineCountInSuperPage; row++) {
            for (col = 0; col < parallelIO - 1; col++) {
              evictData[row][col + 1] =
                  compareFunction(evictData[row][col], evictData[row][col + 1]);
              evictData[row][col] = nullptr;
            }
          }
        }

        // We must flush setToFlush set
        bool have = false;

        for (row = 0; row < lineCountInSuperPage; row++) {
          for (col = 0; col < parallelIO; col++) {
            if (evictData[row][col] && calcSetIndex(evictData[row][col]->tag) == setToFlush) {
              have = true;
            }
          }
        }

        // We don't have setToFlush
         Line *pLineToFlush = nullptr;
        if (!have) {// agr huma eviction k liye kuch nai mila 
         

          for (wayIdx = 0; wayIdx < waySize; wayIdx++) {
            if (cacheData[setToFlush][wayIdx].valid) {
              pLineToFlush =
                  compareFunction(pLineToFlush, cacheData[setToFlush] + wayIdx);
            }
          }

          if (pLineToFlush) {
            calcIOPosition(pLineToFlush->tag, row, col);

            evictData[row][col] = pLineToFlush;
          }
        }
   

        tick += getCacheLatency() * setSize * waySize * 8;

    
 
        evictCache(tick, true);//this is ok code no need 
 
        // Update cacheline of current request
        setIdx = setToFlush;
        wayIdx = getEmptyWay(setIdx, tick);

        if (wayIdx == waySize) {
          panic("Cache corrupted!");
        }

        // DRAM latency
        pDRAM->write(&cacheData[setIdx][wayIdx], req.length, tick);

        // Update cache data
        cacheData[setIdx][wayIdx].insertedAt = tick;
        cacheData[setIdx][wayIdx].lastAccessed = tick;
        cacheData[setIdx][wayIdx].valid = true;
        cacheData[setIdx][wayIdx].dirty = true;
        cacheData[setIdx][wayIdx].tag = req.range.slpn;
        cacheData[setIdx][wayIdx].accessCount=1;
        cacheData[setIdx][wayIdx].loadedFromBlock=req.loadedFromBlock;
       // printf("in the start write done on the set after eviction: %u & Way %u\n",setIdx,wayIdx);
      }  // marks the end of the code when the cache is full and there is no way:
         // avaialabe to handle the write so eviction has to take place.

      debugprint(LOG_ICL_GENERIC_CACHE,
                 "WRITE | Cache miss at (%u, %u) | %" PRIu64 " - %" PRIu64
                 " (%" PRIu64 ")",
                 setIdx, wayIdx, arrived, tick, tick - arrived);
    }// this is part cache was full and the eviction takes place.
    tick += applyLatency(CPU::ICL__GENERIC_CACHE, CPU::WRITE);
  }
  else {
    if (dirty) {
      pFTL->write(reqInternal, tick);
    }
    else {
      tick = flash;
    }

    // TEMP: Disable DRAM calculation for prevent conflict
    pDRAM->setScheduling(false);

    pDRAM->read(nullptr, req.length, tick);

    pDRAM->setScheduling(true);
  }

  stat.request[1]++;//overall write requests

  if (ret) {
    stat.cache[1]++;//writerequsts that that updated the same value in the cache//write hit
  }
    caching.mainCachePageWriteRequest++;
//print();
  return ret;
}
bool GenericCache::writeBlockToCache(
    Request &req,
    uint64_t &tick) {  // this generic cache will handle the eviction and
                       // placement of requests in the cache
                       dirtyStatusCounter++;
  bool ret = false;
  uint64_t flash = tick;
  bool dirty = false;
  debugprint(LOG_ICL_GENERIC_CACHE,
             "WRITE | REQ %7u-%-4u | LCA %" PRIu64 " | SIZE %" PRIu64,
             req.reqID, req.reqSubID, req.range.slpn, req.length);
  FTL::Request reqInternal(lineCountInSuperPage, req);
       
  if (req.length <=lineSize) {
    dirty = true;
  }
  

  if (useWriteCaching) {
    uint32_t setIdx = calcSetIndex(req.range.slpn);// this will give it a set id where the write operation will take place.
    
    uint32_t wayIdx;

    wayIdx = getNewValidWay(req.range.slpn, tick);// A way which is valid and the lca == equal to the tag of that way means cache hit
    if (wayIdx != ghostWaySize) { // some space is avaiable in the cache to absorbe writes
      uint64_t arrived = tick;
      // Wait cache to be valid
      if (tick < ghostCacheData[setIdx][wayIdx].insertedAt) {
        tick = ghostCacheData[setIdx][wayIdx].insertedAt;
      }

      // TODO: TEMPORAL CODE
      // We should only show DRAM latency when cache become dirty
      if (dirty) {
        // Update last accessed time
        ghostCacheData[setIdx][wayIdx].insertedAt = tick;
        ghostCacheData[setIdx][wayIdx].lastAccessed = tick;
      }
      else {
        ghostCacheData[setIdx][wayIdx].insertedAt = flash;
        ghostCacheData[setIdx][wayIdx].lastAccessed = flash;
      }

      // Update last accessed time
      ghostCacheData[setIdx][wayIdx].dirty = dirty;

      // DRAM access
      pDRAM->write(&ghostCacheData[setIdx][wayIdx], req.length, tick);// cache hit and write
        //printf("write done on the set: %u & Way %u\n",setIdx,wayIdx);

        
      debugprint(
          LOG_ICL_GENERIC_CACHE,
          "WRITE | Cache hit at (%u, %u) | %" PRIu64 " - %" PRIu64 " (%" PRIu64
          ")",
          setIdx, wayIdx, arrived, tick,
          tick - arrived);  // it indicates that the cache access was a hit
          ghostCache.ghostCachepageWriteHits++;
      ret = true;
    }// this was the cache overwrite type of operation
    else {  
      uint64_t arrived = tick;
      wayIdx = getGhostEmptyWay(setIdx, tick);// get an empty way for the current code is ok.
      if (wayIdx != ghostWaySize) { // cache is not full
        // Wait cache to be valid
        if (tick < ghostCacheData[setIdx][wayIdx].insertedAt) {
          tick = ghostCacheData[setIdx][wayIdx].insertedAt;
        }

        // TODO: TEMPORAL CODE
        // We should only show DRAM latency when cache become dirty
        if (dirty) {
          // Update last accessed time
          ghostCacheData[setIdx][wayIdx].insertedAt = tick;
          ghostCacheData[setIdx][wayIdx].lastAccessed = tick;
        }
        else {
          ghostCacheData[setIdx][wayIdx].insertedAt = flash;
          ghostCacheData[setIdx][wayIdx].lastAccessed = flash;
        }

        // Update last accessed time
        ghostCacheData[setIdx][wayIdx].valid = true;
        ghostCacheData[setIdx][wayIdx].dirty = dirty;
        ghostCacheData[setIdx][wayIdx].tag = req.range.slpn;

        // DRAM access
        pDRAM->write(&ghostCacheData[setIdx][wayIdx], req.length, tick);
                //printf("write done on the set: %u & Way %u\n",setIdx,wayIdx);
            ghostCache.ghostCacheWritesWithoutFlush++;
        ret = true;
      }
      // We have to flush
      else {// when the cache is already full
        uint32_t row, col;  // Variable for I/O position (IOFlag)////// **************************************from here we have to do flushing*********************
        uint32_t setToFlush = calcSetIndex(req.range.slpn);// calculate the setId of the set to be flushed

        //printf("Incoming write is here %lu\n",req.range.slpn);
        for (setIdx = 0; setIdx < setSize; setIdx++) {//this 
          for (wayIdx = 0; wayIdx < ghostWaySize; wayIdx++) {
            if (ghostCacheData[setIdx][wayIdx].valid) {
              calcIOPosition(ghostCacheData[setIdx][wayIdx].tag, row, col);// ok returns the value for row and colums
              ghostEvictData[row][col] = compareFunction(ghostEvictData[row][col], ghostCacheData[setIdx] + wayIdx);//determine which cache lines should be evicted
                                                   // printf("Evicted way ID: %u\n", wayIdx);                                   
                                                   
            }
          }
        }

        if (evictMode == MODE_SUPERPAGE) {
          uint32_t row, col;  // Variable for I/O position (IOFlag)
      //  printf("Evict mode enabled\n");
          for (row = 0; row < lineCountInSuperPage; row++) {
            for (col = 0; col < parallelIO - 1; col++) {
              ghostEvictData[row][col + 1] =compareFunction(ghostEvictData[row][col], ghostEvictData[row][col + 1]);   
              ghostEvictData[row][col] = nullptr;
            }
          }
        }

        // We must flush setToFlush set
        bool have = false;

        for (row = 0; row < lineCountInSuperPage; row++) {
          for (col = 0; col < parallelIO; col++) {
            if (ghostEvictData[row][col] && calcSetIndex(ghostEvictData[row][col]->tag) == setToFlush) {
              have = true;
            }
          }
        }

        // We don't have setToFlush
         Line *pLineToFlush = nullptr;
        if (!have) {
         

          for (wayIdx = 0; wayIdx < ghostWaySize; wayIdx++) {
            if (ghostCacheData[setToFlush][wayIdx].valid) {
              pLineToFlush =compareFunction(pLineToFlush, ghostCacheData[setToFlush] + wayIdx);
            }
          }

          if (pLineToFlush) {
            calcIOPosition(pLineToFlush->tag, row, col);

            ghostEvictData[row][col] = pLineToFlush;// evict data to flush
          }
        }
           
        tick += getCacheLatency() * setSize * ghostWaySize * 8;
        ghostEviction(tick, true);

        setIdx = setToFlush;
        wayIdx = getGhostEmptyWay(setIdx, tick);

        if (wayIdx == ghostWaySize) {
          panic("Cache corrupted!");
        }

        // DRAM latency
        pDRAM->write(&ghostCacheData[setIdx][wayIdx], req.length, tick);

        // Update cache data
        ghostCacheData[setIdx][wayIdx].insertedAt = tick;
        ghostCacheData[setIdx][wayIdx].lastAccessed = tick;
        ghostCacheData[setIdx][wayIdx].valid = true;
        ghostCacheData[setIdx][wayIdx].dirty = true;
        ghostCacheData[setIdx][wayIdx].tag = req.range.slpn;
        
      }  // marks the end of the code when the cache is full and there is no way
         // avaialabe to handle the write so eviction has to take place.

      debugprint(LOG_ICL_GENERIC_CACHE,
                 "WRITE | Cache miss at (%u, %u) | %" PRIu64 " - %" PRIu64
                 " (%" PRIu64 ")",
                 setIdx, wayIdx, arrived, tick, tick - arrived);
    }

    tick += applyLatency(CPU::ICL__GENERIC_CACHE, CPU::WRITE);
  }
  else {// this is used only if cache is not used for writing
    if (dirty) {
     // pFTL->write(reqInternal, tick);
    }
    else {
      tick = flash;
    }

    // TEMP: Disable DRAM calculation for prevent conflict
    
    pDRAM->setScheduling(false);

    pDRAM->read(nullptr, req.length, tick);

    pDRAM->setScheduling(true);
  }

  stat.request[1]++;

  if (ret) {
    stat.cache[1]++; //writerequest that updated the same value in the cache //overwrite
  }

    ghostCache.ghostCachePageWriteRequest++;
  return ret;
}

bool GenericCache:: writeForCachedGC( Request &req, uint64_t &tick){
  // printf("IncomingWrite: %lu Tick: %ld\n",req.range.slpn,tick);
      
  bool ret = false;
  uint64_t flash = tick;
  bool dirty = false;
  debugprint(LOG_ICL_GENERIC_CACHE,
             "WRITE | REQ %7u-%-4u | LCA %" PRIu64 " | SIZE %" PRIu64,
             req.reqID, req.reqSubID, req.range.slpn, req.length);

  FTL::Request reqInternal(lineCountInSuperPage, req);

  if (req.length <= lineSize) {
    dirty = true;
  }
  else {
    pFTL->write(reqInternal, flash);// direct write the the storage.
    
  }

  if (useWriteCaching) {
    uint32_t setIdx = calcSetIndex(req.range.slpn);// this will give it a set id where the write operation will take place.
    
    uint32_t wayIdx;
    wayIdx = getValidWay(req.range.slpn, tick);// geting a valid way
    if (wayIdx != waySize) { // some space is avaiable in the cache to absorbe writes
      uint64_t arrived = tick;
      if (tick < cacheData[setIdx][wayIdx].insertedAt) {
        tick = cacheData[setIdx][wayIdx].insertedAt;
      }

      // TODO: TEMPORAL CODE
      // We should only show DRAM latency when cache become dirty
      if (dirty) {
        // Update last accessed time
        cacheData[setIdx][wayIdx].insertedAt = tick;
        cacheData[setIdx][wayIdx].lastAccessed = tick;
      }
      else {
        cacheData[setIdx][wayIdx].insertedAt = flash;
        cacheData[setIdx][wayIdx].lastAccessed = flash;
      }

      // Update last accessed time
      cacheData[setIdx][wayIdx].dirty = dirty;
      cacheData[setIdx][wayIdx].accessCount++;
      cacheData[setIdx][wayIdx].loadedFromBlock=req.loadedFromBlock;
      // DRAM access
      pDRAM->write(&cacheData[setIdx][wayIdx], req.length, tick);// cache hit and write
      //movePageFromGhostToCacheWrites++;
      caching.pageWriteHitsInMainCache++;
      debugprint(
          LOG_ICL_GENERIC_CACHE,
          "WRITE | Cache hit at (%u, %u) | %" PRIu64 " - %" PRIu64 " (%" PRIu64
          ")",
          setIdx, wayIdx, arrived, tick,
          tick - arrived);  
      ret = true;
    }
    else {  // when the cache miss occurs and there is an empty way to handle
      uint64_t arrived = tick;
      uint64_t returnedWayFromGhostCache= getNewValidWay(req.range.slpn,tick); //search in ghostCache
      if(returnedWayFromGhostCache != ghostWaySize)
      {
            updateGhostCacheForWrites(setIdx,returnedWayFromGhostCache,tick);
            ghostCache.pagesMovedFromGhostCacheWrites++;
            ret = true;
            //return ret;
      }
      else
      {
      wayIdx = getEmptyWay(setIdx, tick);// get an empty way for the current code is ok.

      if (wayIdx != waySize) { // cache miss with cache line avaiable
        // Wait cache to be valid
        if (tick < cacheData[setIdx][wayIdx].insertedAt) {
          tick = cacheData[setIdx][wayIdx].insertedAt;
        }

        // TODO: TEMPORAL CODE
        // We should only show DRAM latency when cache become dirty
        if (dirty) {
          // Update last accessed time
          cacheData[setIdx][wayIdx].insertedAt = tick;
          cacheData[setIdx][wayIdx].lastAccessed = tick;
        }
        else {
          cacheData[setIdx][wayIdx].insertedAt = flash;
          cacheData[setIdx][wayIdx].lastAccessed = flash;
        }

        // Update last accessed time
        cacheData[setIdx][wayIdx].valid = true;
        cacheData[setIdx][wayIdx].dirty = dirty;
        cacheData[setIdx][wayIdx].tag = req.range.slpn;
        cacheData[setIdx][wayIdx].accessCount++;
        cacheData[setIdx][wayIdx].loadedFromBlock=req.loadedFromBlock;

        // DRAM access
        pDRAM->write(&cacheData[setIdx][wayIdx], req.length, tick);
        caching.mainCacheWritesWithoutFlush++;
        ret = true;
      }
      // We have to flush
      else { //if way returned is equal to the size of the cache .
        uint32_t row, col;  // Variable for I/O position (IOFlag)
        uint32_t setToFlush = calcSetIndex(req.range.slpn);// code is ok


        for (setIdx = 0; setIdx < setSize; setIdx++) {
          for (wayIdx = 0; wayIdx < waySize; wayIdx++) {
            if (cacheData[setIdx][wayIdx].valid) {
              calcIOPosition(cacheData[setIdx][wayIdx].tag, row, col);// ok returns the value for row and colums
              evictData[row][col] = compareFunction(evictData[row][col],cacheData[setIdx] + wayIdx);

            }
          }
        }
            //printf("JustBeforeEvictions\n");
            //printEvictData();

        if (evictMode == MODE_SUPERPAGE) {
          uint32_t row, col;  // Variable for I/O position (IOFlag)

          for (row = 0; row < lineCountInSuperPage; row++) {
            for (col = 0; col < parallelIO - 1; col++) {
              evictData[row][col + 1] =
                  compareFunction(evictData[row][col], evictData[row][col + 1]);
              evictData[row][col] = nullptr;
            }
          }
        }

        // We must flush setToFlush set
        bool have = false;

        for (row = 0; row < lineCountInSuperPage; row++) {
          for (col = 0; col < parallelIO; col++) {
            if (evictData[row][col] && calcSetIndex(evictData[row][col]->tag) == setToFlush) {
              have = true;
            }
          }
        }

        // We don't have setToFlush
         Line *pLineToFlush = nullptr;
        if (!have) {// agr huma eviction k liye kuch nai mila 
         

          for (wayIdx = 0; wayIdx < waySize; wayIdx++) {
            if (cacheData[setToFlush][wayIdx].valid) {
              pLineToFlush =
                  compareFunction(pLineToFlush, cacheData[setToFlush] + wayIdx);
            }
          }

          if (pLineToFlush) {
            calcIOPosition(pLineToFlush->tag, row, col);

            evictData[row][col] = pLineToFlush;
          }
        }
   

        tick += getCacheLatency() * setSize * waySize * 8;

    
 
        evictCache(tick, true);//this is ok code no need 
 
        // Update cacheline of current request
        setIdx = setToFlush;
        wayIdx = getEmptyWay(setIdx, tick);

        if (wayIdx == waySize) {
          panic("Cache corrupted!");
        }

        // DRAM latency
        pDRAM->write(&cacheData[setIdx][wayIdx], req.length, tick);

        // Update cache data
        cacheData[setIdx][wayIdx].insertedAt = tick;
        cacheData[setIdx][wayIdx].lastAccessed = tick;
        cacheData[setIdx][wayIdx].valid = true;
        cacheData[setIdx][wayIdx].dirty = true;
        cacheData[setIdx][wayIdx].tag = req.range.slpn;
        cacheData[setIdx][wayIdx].accessCount=1;
        cacheData[setIdx][wayIdx].loadedFromBlock=req.loadedFromBlock;
       // printf("in the start write done on the set after eviction: %u & Way %u\n",setIdx,wayIdx);
      }  
      debugprint(LOG_ICL_GENERIC_CACHE,
                 "WRITE | Cache miss at (%u, %u) | %" PRIu64 " - %" PRIu64
                 " (%" PRIu64 ")",
                 setIdx, wayIdx, arrived, tick, tick - arrived);
    }// elsepart of the ghostpart;
    }// this is part cache was full and the eviction takes place.
    tick += applyLatency(CPU::ICL__GENERIC_CACHE, CPU::WRITE);
  }
  else {
    if (dirty) {
      pFTL->write(reqInternal, tick);
    }
    else {
      tick = flash;
    }

    // TEMP: Disable DRAM calculation for prevent conflict
    pDRAM->setScheduling(false);

    pDRAM->read(nullptr, req.length, tick);

    pDRAM->setScheduling(true);
  }

  stat.request[1]++;//overall write requests

  if (ret) {
    stat.cache[1]++;//writerequsts that that updated the same value in the cache//write hit
  }

     // print();
//printf("Written %lu at %lu\n",req.range.slpn,tick);
caching.mainCachePageWriteRequest++;
  return ret;

    }
    
    void GenericCache::printIncommingRequest(Request &request)
    {
        std::cout << "reqID: " << request.reqID << ", reqSubID: " << request.reqSubID << ", offset: " << request.offset << ", length: " << request.length <<" LPN:"<<request.range.slpn<<endl;;

    }
bool GenericCache::readForCachedGc(Request &req, uint64_t &tick){
  bool ret = false;
  //printf("Incoming Read: %lu Tick: %ld\n",req.range.slpn,tick);

  debugprint(LOG_ICL_GENERIC_CACHE,
             "READ  | REQ %7u-%-4u | LCA %" PRIu64 " | SIZE %" PRIu64,
             req.reqID, req.reqSubID, req.range.slpn, req.length);
//printf("The ICL Count %ld\n",iclCount);
  if (useReadCaching) {
    uint32_t setIdx = calcSetIndex(req.range.slpn);// this is ok..getting the actual set Id
    uint32_t wayIdx;
    uint64_t arrived = tick;

    if (useReadPrefetch) {
      checkSequential(req, readDetect);
    }

    wayIdx = getValidWay(req.range.slpn, tick);//this is ok// this will give the way number where from we have to read.
    // Do we have valid data?
    if (wayIdx != waySize) {// if read hit..
      uint64_t tickBackup = tick;

      // Wait cache to be valid
      if (tick < cacheData[setIdx][wayIdx].insertedAt) {
        tick = cacheData[setIdx][wayIdx].insertedAt;
      }

      // Update last accessed time
      cacheData[setIdx][wayIdx].lastAccessed = tick;
      cacheData[setIdx][wayIdx].accessCount++;

      // DRAM access
      pDRAM->read(&cacheData[setIdx][wayIdx], req.length, tick);// 

      debugprint(LOG_ICL_GENERIC_CACHE,
                 "READ  | Cache hit at (%u, %u) | %" PRIu64 " - %" PRIu64
                 " (%" PRIu64 ")",
                 setIdx, wayIdx, arrived, tick, tick - arrived);

      ret = true; //return true if the read is found in the cache;
      caching.pageReadHitsInMainCache++;

      // Do we need to prefetch data?
      if (useReadPrefetch && req.range.slpn == prefetchTrigger) {
        debugprint(LOG_ICL_GENERIC_CACHE, "READ  | Prefetch triggered");

        req.range.slpn = lastPrefetched;

        // Backup tick
        arrived = tick;
        tick = tickBackup;
        //movePageFromGhostToCacheReads++;
        //caching.pageReadHitsInMainCache++;
        goto ICL_GENERIC_CACHE_READ;// this is for prefetching the data.
      }/// this is read portion ok with respect to me..
    }
    // We should read data from NVM
    else {  // if it is not present in the cache then read from nvm and place in the Ram with some empty slot.
    ICL_GENERIC_CACHE_READ:
      FTL::Request reqInternal(lineCountInSuperPage, req);
      std::vector<std::pair<uint64_t, uint64_t>> readList;
      uint32_t row, col;  // Variable for I/O position (IOFlag)
      uint64_t dramAt;
      uint64_t beginLCA, endLCA;
      uint64_t beginAt, finishedAt = tick;

         uint32_t ghostWayReturned= getNewValidWay(reqInternal.lpn,tick);
         beginAt = tick;
         if(ghostWayReturned != ghostWaySize) // way is present in the ghostCache
         {
          updateGhostCacheForReads(setIdx,ghostWayReturned,beginAt);
          //movePageFromGhostToCacheReads++;
          ghostCache.pagesMovedFromGhostCacheReads++;
              ret = true;
         }
        else
        {
        if (readDetect.enabled) {
        // TEMP: Disable DRAM calculation for prevent conflict
        pDRAM->setScheduling(false);

        if (!ret) {
          debugprint(LOG_ICL_GENERIC_CACHE, "READ  | Read ahead triggered");
        }

        beginLCA = req.range.slpn;

        // If super-page is disabled, just read all pages from all planes
        if (prefetchMode == MODE_ALL || !bSuperPage) {
          endLCA = beginLCA + lineCountInMaxIO;
          prefetchTrigger = beginLCA + lineCountInMaxIO / 2;
        }
        else {
          endLCA = beginLCA + lineCountInSuperPage;
          prefetchTrigger = beginLCA + lineCountInSuperPage / 2;
        }

        lastPrefetched = endLCA;
      }
      else {
        beginLCA = req.range.slpn;
        endLCA = beginLCA + 1;
      }
      for (uint64_t lca = beginLCA; lca < endLCA; lca++) {
        beginAt = tick;

        // Check cache
        if (getValidWay(lca, beginAt) != waySize) {
          continue;
        }

  
        // Find way to write data read from NVM
        setIdx = calcSetIndex(lca);
        wayIdx = getEmptyWay(setIdx, beginAt);// this portion is also ok..
       //printf("Empty way returned %u lpn %lu\n",wayIdx,lca);
          /*#################################################################*/
          if (wayIdx == waySize) {
          wayIdx = evictFunction(setIdx, beginAt);

          //if (cacheData[setIdx][wayIdx].dirty) {
            // We need to evict data before write
            calcIOPosition(cacheData[setIdx][wayIdx].tag, row, col);
            evictData[row][col] = cacheData[setIdx] + wayIdx;
                      evictCache(tick);

          //}
        }
        cacheData[setIdx][wayIdx].insertedAt = beginAt;
        cacheData[setIdx][wayIdx].lastAccessed = beginAt;
        cacheData[setIdx][wayIdx].valid = true;
        cacheData[setIdx][wayIdx].dirty = false;

        readList.push_back({lca, ((uint64_t)setIdx << 32) | wayIdx});// this is the IO i have to read from the flash
        //printf("Rad list is crated witht he size %ld\n",readList.size());

        finishedAt = MAX(finishedAt, beginAt);

      }

      tick = finishedAt;

      evictCache(tick);

      for (auto &iter : readList) {
        Line *pLine = &cacheData[iter.second >> 32][iter.second & 0xFFFFFFFF];

        // Read data
        reqInternal.lpn = iter.first / lineCountInSuperPage;// ok
        reqInternal.ioFlag.reset();
        reqInternal.ioFlag.set(iter.first % lineCountInSuperPage);

          // Ignore cache metadata access
        
        pFTL->read(reqInternal, beginAt);// this is the data to be read from the block.

        dramAt = pLine->insertedAt;
        pDRAM->write(pLine, lineSize, dramAt);

        // Set cache data
        beginAt = MAX(beginAt, dramAt);

        pLine->insertedAt = beginAt;
        pLine->lastAccessed = beginAt;
        pLine->tag = iter.first;
        pLine->valid=true;// this is the updated version.
            //pageisPresentInSSD=false;
                
        pLine->accessCount=1;

        if (pLine->tag == req.range.slpn) {
          finishedAt = beginAt;
        }
            //printf(" after Read Internal\n");
     
        block_index= -1;
        debugprint(LOG_ICL_GENERIC_CACHE,
                   "READ  | Cache miss at (%u, %u) | %" PRIu64 " - %" PRIu64
                   " (%" PRIu64 ")",
                   iter.second >> 32, iter.second & 0xFFFFFFFF, tick, beginAt,
                   beginAt - tick);
  }
    

      tick = finishedAt;

      if (readDetect.enabled) {
        if (ret) {
          // This request was prefetch
          debugprint(LOG_ICL_GENERIC_CACHE, "READ  | Prefetch done");

          // Restore tick
          tick = arrived;
        }
        else {
          debugprint(LOG_ICL_GENERIC_CACHE, "READ  | Read ahead done");
        }

        // TEMP: Restore
        pDRAM->setScheduling(true);
      }
    }
  }

    tick += applyLatency(CPU::ICL__GENERIC_CACHE, CPU::READ);
  }//if reading cache used or not
  else {
    FTL::Request reqInternal(lineCountInSuperPage, req);
        
    pDRAM->write(nullptr, req.length, tick);

    pFTL->read(reqInternal, tick);//this is 
  }
 

  stat.request[0]++; // overall number of ICL Read Requests

  if (ret) {
    stat.cache[0]++; // read hit
  }
  caching.mainCachePageReadRequest++;
  return ret;

}

void GenericCache:: displayEvictLines()
{
  printf("Contents of evictLines-------\n");
  for (const auto& line : evictLines) {
        std::cout << "Tag: " << line.tag << ", Last Accessed: " << line.lastAccessed
                  << ", Inserted At: " << line.insertedAt << ", Dirty: " << line.dirty
                  << ", Valid: " << line.valid << ", bId: " << line.bId << std::endl;
    }

}
void GenericCache:: displayGhostCache()
{
   printf("--Contents of ActualCache--------\n");

for(uint32_t i=0;i<setSize;i++)
      {
       for (uint32_t j = 0; j < ghostWaySize; j++) {
        Line& line = ghostCacheData[i][j];

        std::cout << "Line " << j << ": ";
        std::cout << "Tag: " << line.tag << ", Last Accessed: " << line.lastAccessed
                  << ", Inserted At: " << line.insertedAt << ", Dirty: " << line.dirty
                  << ", Valid: " << line.valid 
                  << ", AccessCount: "<<line.accessCount
                  << ", LoadedFromBlock: "<<line.loadedFromBlock;
        // Print more information if needed...
        std::cout << std::endl;
         std::cout << std::endl;
      }
    }
}
void GenericCache:: EvictedLinesToFTl(uint64_t &tick)
{
  FTL::Request reqInternal(lineCountInSuperPage);
  liner=lineCountInSuperPage;
  uint64_t beginAt;
  uint64_t finishedAt = tick;
  uint64_t time_access=0;
  //int p=0;
  getin=true;
  getin2=false;
 // int ioTosend=0;
 //displayEvictLines();
 if(evictLines.size() <16) return;
  for (auto it = evictLines.begin(); it != evictLines.end();) {
  beginAt=tick;
 //if(ioTosend++ > 25)return;
    const auto& line = *it;
       SimpleSSD::ICL::Line dummyLine = line; // Copy the line to a dummy variable
            it = evictLines.erase(it);
            reqInternal.ioFlag.reset();
        reqInternal.ioFlag.set(0);
            
    if (dummyLine.dirty && dummyLine.valid) {
     
      
      reqInternal.lpn=dummyLine.tag;
       if(tick>time_access)
       {
             logical_check=true;
             time_access=tick;
         // printf(" eviction Id: %d Tag: %lu dirty %d  Size %ld IOCount %ld GCCount %d\n", p++,line.tag,line.dirty,evictLines.size(),myIoCount,gcCounter);
               pFTL->DecachePage(reqInternal.lpn);
             pFTL->write(reqInternal, beginAt);
             
              
            //writeInternal(EvictedRequest, tick,true);
                        
            
       }
     
            //break;
            //newFTLRequest(dummyLine.tag,beginAt);
            //writeInternal(Request &req, uint64_t &tick)
            //finishedAt = MAX(finishedAt, tick);
            
        
        //internal++;
    } 
    else
    {
           pFTL->DecachePage(reqInternal.lpn);
    }
    
     finishedAt = MAX(finishedAt, beginAt);
   tick += applyLatency(CPU::FTL__PAGE_MAPPING, CPU::WRITE);
   printf("done with eviction of LPN %lu\n",reqInternal.lpn);
   //if(ioTosend++ >20)break;
}

            //  printf("Eviction cache size size is after Removal##################### %ld\n",evictLines.size());
            getin=false;
            getin2=true;
              evictLines.clear();

}

// True when flushed
void GenericCache::flush(LPNRange &range, uint64_t &tick) {
 //printf("I wanted to check if the page has reached here or not\n");
  if (useReadCaching || useWriteCaching) {
    uint64_t ftlTick = tick;
    uint64_t finishedAt = tick;
    FTL::Request reqInternal(lineCountInSuperPage);

    for (uint32_t setIdx = 0; setIdx < setSize; setIdx++) {
      for (uint32_t wayIdx = 0; wayIdx < waySize; wayIdx++) {
        Line &line = cacheData[setIdx][wayIdx];

        tick += getCacheLatency() * 8;

        if (line.tag >= range.slpn && line.tag < range.slpn + range.nlp) {
          if (line.dirty) {
            reqInternal.lpn = line.tag / lineCountInSuperPage;
            reqInternal.ioFlag.set(line.tag % lineCountInSuperPage);

            ftlTick = tick;
            pFTL->write(reqInternal, ftlTick);
            finishedAt = MAX(finishedAt, ftlTick);
          }

          line.valid = false;;
        }
      }
    }

    tick = MAX(tick, finishedAt);;
    tick += applyLatency(CPU::ICL__GENERIC_CACHE, CPU::FLUSH);
  }
}

// True when hit
void GenericCache::trim(LPNRange &range, uint64_t &tick) {
  if (useReadCaching || useWriteCaching) {
    uint64_t ftlTick = tick;
    uint64_t finishedAt = tick;
    FTL::Request reqInternal(lineCountInSuperPage);

    for (uint32_t setIdx = 0; setIdx < setSize; setIdx++) {
      for (uint32_t wayIdx = 0; wayIdx < waySize; wayIdx++) {
        Line &line = cacheData[setIdx][wayIdx];

        tick += getCacheLatency() * 8;

        if (line.tag >= range.slpn && line.tag < range.slpn + range.nlp) {
          reqInternal.lpn = line.tag / lineCountInSuperPage;
          reqInternal.ioFlag.set(line.tag % lineCountInSuperPage);

          ftlTick = tick;
          pFTL->trim(reqInternal, ftlTick);
          finishedAt = MAX(finishedAt, ftlTick);

          line.valid = false;
        }
      }
    }

    tick = MAX(tick, finishedAt);
    tick += applyLatency(CPU::ICL__GENERIC_CACHE, CPU::TRIM);
  }
}

void GenericCache::format(LPNRange &range, uint64_t &tick) {
  if (useReadCaching || useWriteCaching) {
    uint64_t lpn;
    uint32_t setIdx;
    uint32_t wayIdx;
//printf("Here is some times\n");
    for (uint64_t i = 0; i < range.nlp; i++) {
      lpn = range.slpn + i;
      setIdx = calcSetIndex(lpn);
      wayIdx = getValidWay(lpn, tick);

      if (wayIdx != waySize) {
        // Invalidate
        cacheData[setIdx][wayIdx].valid = false;
      }
    }
  }

  // Convert unit
  range.slpn /= lineCountInSuperPage;
  range.nlp = (range.nlp - 1) / lineCountInSuperPage + 1;

  pFTL->format(range, tick);
  tick += applyLatency(CPU::ICL__GENERIC_CACHE, CPU::FORMAT);
}

void GenericCache::getStatList(std::vector<Stats> &list, std::string prefix) {
  Stats temp;

  temp.name = prefix + "generic_cache.read.request_count";
  temp.desc = "Read request count";
  list.push_back(temp);

  temp.name = prefix + "generic_cache.read.from_cache";
  temp.desc = "Read requests that served from cache";
  list.push_back(temp);

  temp.name = prefix + "generic_cache.write.request_count";
  temp.desc = "Write request count";
  list.push_back(temp);

  temp.name = prefix + "generic_cache.write.to_cache";
  temp.desc = "Write requests that served to cache";
  list.push_back(temp);
  //genericCacheStats();
}
void GenericCache::genericCacheStats()
{
 
  printf("DirtyUpdateRequestRecived %lu\n",caching.dirtyUpdateRequests);
  printf("DirtyPage.Updates %lu\n",totalDirtyStatusUpdate);
  printf("Total.Evictions %lu\n",totalEvictions);
  printf("Total.DirtyEvictions %lu\n",totalDirtyEvictions);
  printf("TotalReadReqestMainCache %lu\n",caching.mainCachePageReadRequest);
  printf("TotalWriteReqestMainCache %lu\n",caching.mainCachePageWriteRequest);
  printf("ReadHitsInMainCache %lu\n",caching.pageReadHitsInMainCache);
  printf("WriteHitsInMainCache %lu\n",caching.pageWriteHitsInMainCache);
  printf("TotalWriteReqestGhostCache %lu\n",ghostCache.ghostCachePageWriteRequest);
  printf("GhostCacheWriteHits %lu\n",ghostCache.ghostCachepageWriteHits);
  printf("PagesServedFromGhostCacheReads %lu\n",ghostCache.pagesMovedFromGhostCacheReads);
  printf("PagesServedFromGhostCacheWrites %lu\n",ghostCache.pagesMovedFromGhostCacheWrites);
  printf("GhostCacheTotalEvictions %lu\n",ghostCache.ghostCacheTotalEvictions);
  printf("GhostCacheDirtyEvictions %lu\n",ghostCache.ghostCacheDirtyEvictions);
  printf("prefetcherActivationCounter %lu\n",caching.prefetcherActivationCounter);
  printf("PageEvictedByGcAgainAccessed %lu\n",caching.pageEvictedByGcAgainAccessed);
  printf("pageEvictedByHostAgainAccessed %lu\n",caching.pageEvictedByHostAgainAccessed);



  if(cachedGC)
  {
      printf("PagesCopied.to.Cache.From.Block %lu\n",totalPagesMovedFromBlockToCache);
  }
  else
  {
      printf("PagesCopied.from.VictumBlock.freeBlock %lu\n",totalPagesMovedFromBlockToCache);

  }
}

void GenericCache::getStatValues(std::vector<double> &values) {
  values.push_back(stat.request[0]);//read requests
  values.push_back(stat.cache[0]);//read hits
  values.push_back(stat.request[1]);//writeRequests
  values.push_back(stat.cache[1]);//writeHits
}

void GenericCache::resetStatValues() {
  memset(&stat, 0, sizeof(stat));
  memset(&caching, 0, sizeof(caching));
  memset(&ghostCache, 0, sizeof(ghostCache));
}

}  // namespace ICL

}  // namespace SimpleSSD
