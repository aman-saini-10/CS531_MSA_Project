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

#include "icl/icl.hh"
#include <cstdint>
#include "cpu/def.hh"
#include "ftl/config.hh"
#include "icl/global_point.hh"
#include "dram/simple.hh"
#include "icl/generic_cache.hh"
#include "util/algorithm.hh"
#include "util/def.hh"

namespace SimpleSSD {

namespace ICL {

ICL::ICL(ConfigReader &c) : conf(c) {
 
  switch (conf.readInt(CONFIG_DRAM, DRAM::DRAM_MODEL)) {
    case DRAM::SIMPLE_MODEL:
      pDRAM = new DRAM::SimpleDRAM(conf);

      break;
    default:
      panic("Undefined DRAM model");

      break;;; 
  }

  pFTL = new FTL::FTL(conf, pDRAM);

  FTL::Parameter *param = pFTL->getInfo();;

  if (conf.readBoolean(CONFIG_FTL, FTL::FTL_USE_RANDOM_IO_TWEAK)) {
    totalLogicalPages =
        param->totalLogicalBlocks * param->pagesInBlock * param->ioUnitInPage;
    logicalPageSize = param->pageSize / param->ioUnitInPage;
  }
  else {
    totalLogicalPages = param->totalLogicalBlocks * param->pagesInBlock;
    logicalPageSize = param->pageSize;
  }
  pCache = new GenericCache(conf, pFTL, pDRAM);
  globalCache=pCache;
  globalCache->tester();
}

ICL::~ICL() {
  delete pCache;
  delete pFTL;
  delete pDRAM;
}

void ICL::read(Request &req, uint64_t &tick) {
  uint64_t beginAt;
  uint64_t finishedAt = tick;
  uint64_t reqRemain = req.length;
  Request reqInternal;
  reqInternal.reqID = req.reqID;
  reqInternal.offset = req.offset;
  for (uint64_t i = 0; i < req.range.nlp; i++) {
    beginAt = tick;

    reqInternal.reqSubID = i + 1;
    reqInternal.range.slpn = req.range.slpn + i;
    reqInternal.length = MIN(reqRemain, logicalPageSize );
      iclCount++;
    //printf("In IclRead Tick %lu LPN %lu length %lu offset %lu\n",tick,reqInternal.range.slpn,reqInternal.length,reqInternal.offset);
    if(victimSelectionPolicy==4)
    {
    //pCache->readForCachedGc(reqInternal,beginAt);
    pCache->read(reqInternal, beginAt);
    }
    else{
       pCache->read(reqInternal, beginAt);
    }
    reqRemain -= reqInternal.length;
    reqInternal.offset = 0;
        
    finishedAt = MAX(finishedAt, beginAt);
            

  
  //printf("In ReadValue of BeginAt %lu Tick %lu i %lu\n",beginAt,tick,i);

  }

  debugprint(LOG_ICL,
             "READ  | LCA %" PRIu64 " + %" PRIu64 " | %" PRIu64 " - %" PRIu64
             " (%" PRIu64 ")",
             req.range.slpn, req.range.nlp, tick, finishedAt,
             finishedAt - tick);
tick = finishedAt;
tick += applyLatency(CPU::ICL, CPU::READ);
 
}

void ICL::write(Request &req, uint64_t &tick) {
  uint64_t beginAt;
  uint64_t finishedAt = tick;
  uint64_t reqRemain = req.length;
  Request reqInternal;
  reqInternal.reqID = req.reqID;
  reqInternal.offset = req.offset;

  for (uint64_t i = 0; i < req.range.nlp; i++) {// iteratively break each IO requests in small parts of each page size
    beginAt = tick;

    reqInternal.reqSubID = i + 1;
    reqInternal.range.slpn = req.range.slpn + i;
    reqInternal.length = MIN(reqRemain, logicalPageSize );

      iclCount++;
        // printf("In IclWrite Tick %lu LPN %lu length %lu offset %lu\n",tick,reqInternal.range.slpn,reqInternal.length,reqInternal.offset);

      
     if(victimSelectionPolicy==4)
    {
      //pCache->writeForCachedGC(reqInternal,beginAt);
     pCache->write(reqInternal, beginAt);
    }
    else{
      pCache->write(reqInternal, beginAt);
    }
     if(putPagesInCache)
      {
            writeToCacheMimic(beginAt);
      }
    reqRemain -= reqInternal.length;
    reqInternal.offset =0;

    finishedAt = MAX(finishedAt, beginAt);
    
  //printf("In WriteValue of BeginAt %lu Tick %lu i %lu\n",beginAt,tick,i);
  if(iclCount % 500000 ==0)
        {
          printf("completed %lu Requests\n",iclCount);
        }    
  }

  debugprint(LOG_ICL,
             "WRITE | LCA %" PRIu64 " + %" PRIu64 " | %" PRIu64 " - %" PRIu64
             " (%" PRIu64 ")",
             req.range.slpn, req.range.nlp, tick, finishedAt,
             finishedAt - tick);
tick = finishedAt;          
tick += applyLatency(CPU::ICL, CPU::WRITE);
  
}

void ICL::flush(LPNRange &range, uint64_t &tick) {
  uint64_t beginAt = tick;

  pCache->flush(range, tick);

  debugprint(LOG_ICL,
             "FLUSH | LCA %" PRIu64 " + %" PRIu64 " | %" PRIu64 " - %" PRIu64
             " (%" PRIu64 ")",
             range.slpn, range.nlp, beginAt, tick, tick - beginAt);

  tick += applyLatency(CPU::ICL, CPU::FLUSH);
}

void ICL::trim(LPNRange &range, uint64_t &tick) {
  uint64_t beginAt = tick;

  pCache->trim(range, tick);

  debugprint(LOG_ICL,
             "TRIM  | LCA %" PRIu64 " + %" PRIu64 " | %" PRIu64 " - %" PRIu64
             " (%" PRIu64 ")",
             range.slpn, range.nlp, beginAt, tick, tick - beginAt);

  tick += applyLatency(CPU::ICL, CPU::TRIM);
}

void ICL::format(LPNRange &range, uint64_t &tick) {
  uint64_t beginAt = tick;

  pCache->format(range, tick);

  debugprint(LOG_ICL,
             "FORMAT| LCA %" PRIu64 " + %" PRIu64 " | %" PRIu64 " - %" PRIu64
             " (%" PRIu64 ")",
             range.slpn, range.nlp, beginAt, tick, tick - beginAt);

  tick += applyLatency(CPU::ICL, CPU::FORMAT);
}
void ICL::writeToCacheMimic(uint64_t &tick) // here i am write the request from the block to the cache..
{
  uint64_t beginAt;
   uint64_t finishedAt = tick;
  uint64_t i=0;
  SimpleSSD::ICL::Request request;
 // printf("Before the Loop IclCount %lu Tick %lu\n\n",iclCount,tick);
  while (!movePagesToCache.empty()) {
    beginAt=tick;
        uint64_t element = movePagesToCache.front();  // Get the front element
        movePagesToCache.erase(movePagesToCache.begin());  // Remove the front element
        request.reqSubID = i++;
        request.range.slpn = element;// Change is done
        request.length = logicalPageSize;
        request.offset = 0;
        request.loadedFromBlock=true;
        //printf("Writing %lu and SizeOfBlocksPages %ld\n",element,movePagesToCache.size());
        //pCache->writeBlockToCache(request, beginAt);// assigning the request to theDRAM// new
        //pCache->write(request, beginAt);
        pCache->writeVictimPagesToCache(request,beginAt);
        finishedAt = MAX(finishedAt, beginAt);
        //tick = finishedAt;
         //tick += applyLatency(CPU::ICL, CPU::WRITE); 
    }
  //printf("After the Loop IclCount %lu Tick %lu\n\n",iclCount,tick);

    if(movePagesToCache.size()==0)
    {
    putPagesInCache=false;
    //return;
    }
    tick = finishedAt;
    //tick += applyLatency(CPU::ICL, CPU::WRITE);  
    return;
}

void ICL::getLPNInfo(uint64_t &t, uint32_t &s) {
  t = totalLogicalPages;
  s = logicalPageSize;
}

uint64_t ICL::getUsedPageCount(uint64_t lcaBegin, uint64_t lcaEnd) {
  uint32_t ratio = pFTL->getInfo()->ioUnitInPage;

  return pFTL->getUsedPageCount(lcaBegin / ratio, lcaEnd / ratio) * ratio;
}

void ICL::getStatList(std::vector<Stats> &list, std::string prefix) {
  pCache->getStatList(list, prefix + "icl.");
  pDRAM->getStatList(list, prefix + "dram.");
  pFTL->getStatList(list, prefix);
  
}

void ICL::getStatValues(std::vector<double> &values) {
  pCache->getStatValues(values);
  pDRAM->getStatValues(values);
  pFTL->getStatValues(values);
  pCache->genericCacheStats();
}

void ICL::resetStatValues() {
  pCache->resetStatValues();
  pDRAM->resetStatValues();
  pFTL->resetStatValues();
}

}  // namespace ICL

}  // namespace SimpleSSD
