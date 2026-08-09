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
    /*wayIdx = getValidWay(req.range.slpn, tick);// geting a valid way
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

      ret = true;
    }// this was the cache overwrite type of operation
    else {*/  // when the cache miss occurs and there is an empty way to handle
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
              evictData[row][col] = compareFunction1(evictData[row][col],cacheData[setIdx] + wayIdx);

            }
          }
        }
            
        if (evictMode == MODE_SUPERPAGE) {
          uint32_t row, col;  // Variable for I/O position (IOFlag)

          for (row = 0; row < lineCountInSuperPage; row++) {
            for (col = 0; col < parallelIO - 1; col++) {
              evictData[row][col + 1] =
                  compareFunction1(evictData[row][col], evictData[row][col + 1]);
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
                  compareFunction1(pLineToFlush, cacheData[setToFlush] + wayIdx);
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
    //}// this is part cache was full and the eviction takes place.
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

  return ret;
}