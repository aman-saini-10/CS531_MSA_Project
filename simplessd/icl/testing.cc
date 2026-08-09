bool GenericCache::read(Request &req, uint64_t &tick) {
  bool ret = false;
  printf("Incoming Read: %lu Tick: %ld\n",req.range.slpn,tick);
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
        
        // DRAM delay
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
        
       // print();
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