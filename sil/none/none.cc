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

#include "sil/none/none.hh"

#include "simplessd/util/algorithm.hh"
#include "simplessd/icl/global_point.hh"

namespace SIL {

namespace None {

Driver::Driver(Engine &e, SimpleSSD::ConfigReader &conf)
    : BIL::DriverInterface(e), totalLogicalPages(0), logicalPageSize(0) {
  pHIL = new SimpleSSD::HIL::HIL(conf);
}

Driver::~Driver() {
  delete pHIL;;
}

void Driver::init(std::function<void()> &func) {
  pHIL->getLPNInfo(totalLogicalPages, logicalPageSize);

  // No initialization process is needed for NoneDriver
  beginFunction = func;

  auto eid = engine.allocateEvent([this](uint64_t) { beginFunction(); });
  engine.scheduleEvent(eid, 0);
   printf("SIL::None::Driver: Total SSD capacity: %" PRIu64 " GB\n",
                  (((totalLogicalPages * logicalPageSize)/1024)/1024)/1024);
  SimpleSSD::info("SIL::None::Driver: Total SSD capacity: %" PRIu64 " GB",
                  (((totalLogicalPages * logicalPageSize)/1024)/1024)/1024);
  SimpleSSD::info("SIL::None::Driver: Logical Page Size: %" PRIu32 " bytes",
                  logicalPageSize);
}

void Driver::getInfo(uint64_t &bytesize, uint32_t &minbs) {
  bytesize = totalLogicalPages * logicalPageSize;
  minbs = 512;
}

void Driver::submitIO(BIL::BIO &bio) {
  SimpleSSD::HIL::Request req;
  auto *pFunc = new std::function<void(uint64_t)>(bio.callback);

  // Convert to request
  //printf("In BIO offset %lu,Length %lu \n",bio.offset,bio.length);
  uint64_t incomingRequestAddress =bio.offset / logicalPageSize;

 // uint64_t numBits = (uint64_t)(log2(incomingRequestAddress)) + 1;
   uint64_t mask = (uint64_t)(log2(nlogicalpagesinSSD)) + 1;

//printf("Number of bits needed: %lu mask %lu\n", numBits,mask);
//std::exit(0);
  if(incomingRequestAddress <= nlogicalpagesinSSD)
  {
  req.reqID = bio.id;
  req.range.slpn = incomingRequestAddress; // which page the request belongs to 
  req.range.nlp = DIVCEIL(bio.length, logicalPageSize);
  req.offset =bio.offset % logicalPageSize;
  req.length = bio.length;
  req.context = (void *)bio.id;
  }
  else{
     

   
   uint64_t extractMask = (1ull << mask) - 1;  //this gives all the ones
   
    uint64_t extractedBits = incomingRequestAddress & (extractMask);// this is the actual request in the size of ssd
    //uint64_t remainingBits = incomingRequestAddress & (~extractMask);
    //printf(" incom %lu extrmask %lu extrBits %lu \t",incomingRequestAddress,extractMask,extractedBits);
    //printf("IR %lu extractMask %lu extractedBits %lu \t ",incomingRequestAddress,extractMask,extractedBits);
    //incomingRequestAddress &= extractMask;
    extractedBits %= nlogicalpagesinSSD;
    incomingRequestAddress=extractedBits;
     mask = (int)(log2(incomingRequestAddress)) + 1;
//printf("\toutgoing %lu bits %lu\n", incomingRequestAddress,mask);
    // Display the modified outgoing request address
   
 //bio.offset =incomingRequestAddress;
    req.reqID = bio.id;
  req.range.slpn = incomingRequestAddress; // which page the request belongs to 
  req.range.nlp = DIVCEIL(bio.length, logicalPageSize);
  req.offset =0;
  req.length = bio.length;
  req.context = (void *)bio.id;
  if(req.range.slpn > nlogicalpagesinSSD)
  {
    printf("********************************slp %lu OUTSide Range %lu mask %lu\n", req.range.slpn,nlogicalpagesinSSD,mask);
  }
  }
  
  //printf("NONE.CC Req Id %lu Range SLPn %lu Range NLPn %lu offset %lu Length %lu\n",req.reqID,req.range.slpn,req.range.nlp,req.offset,req.length);
  //printf("NONE Req Id: %lu Old offset: %lu SLPN: %lu Range N: %lu NewOffset: %lu\n",req.reqID,bio.offset,req.range.slpn,req.range.nlp,req.offset);
  
  req.function = [pFunc](uint64_t, void *context) {
    pFunc->operator()((uint64_t)context);
    delete pFunc;
  };

  // Submit
  switch (bio.type) {
    case BIL::BIO_READ:
      pHIL->read(req);
      //printf("this is function in SIL with read request.\n");
      break;
    case BIL::BIO_WRITE:
      pHIL->write(req);
      break;
    case BIL::BIO_FLUSH:
      pHIL->flush(req);
      break;
    case BIL::BIO_TRIM:
      pHIL->trim(req);
      break;
    default:
      break;
  }
}

void Driver::initStats(std::vector<SimpleSSD::Stats> &list) {
  pHIL->getStatList(list, "");
  SimpleSSD::getCPUStatList(list, "cpu");
}

void Driver::getStats(std::vector<double> &values) {
  pHIL->getStatValues(values);
  SimpleSSD::getCPUStatValues(values);
}

}  // namespace None

}  // namespace SIL
