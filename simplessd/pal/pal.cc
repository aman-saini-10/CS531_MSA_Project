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

#include "pal/pal.hh"

#include "pal/pal_old.hh"
#include "icl/global_point.hh"

namespace SimpleSSD {

namespace PAL {

PAL::PAL(ConfigReader &c) : conf(c) {
  static const char name[4][16] = {"Channel", "Way", "Die", "Plane"};
  uint32_t value[4];
  uint8_t superblock = conf.getSuperblockConfig();

  param.channel = conf.readUint(CONFIG_PAL, PAL_CHANNEL);
  param.package = conf.readUint(CONFIG_PAL, PAL_PACKAGE);

  param.die = conf.readUint(CONFIG_PAL, NAND_DIE);
  param.plane = conf.readUint(CONFIG_PAL, NAND_PLANE);
  param.block = conf.readUint(CONFIG_PAL, NAND_BLOCK);
  param.page = conf.readUint(CONFIG_PAL, NAND_PAGE);
  param.pageSize = conf.readUint(CONFIG_PAL, NAND_PAGE_SIZE);
  param.superBlock = param.block;
  param.superPageSize = param.pageSize;

{
  ssdInternals[0]=param.channel;
  ssdInternals[1]=param.package;
  ssdInternals[2]=param.die;
  ssdInternals[3]=param.plane;
}

  // Super block includes channel
  if (superblock & INDEX_CHANNEL) {
    param.superPageSize *= param.channel;
    value[0] = param.channel;
  }
  else {
    param.superBlock *= param.channel;
  }

  // Super block includes way (package)
  if (superblock & INDEX_PACKAGE) {
    param.superPageSize *= param.package;
    value[1] = param.package;
  }
  else {
    param.superBlock *= param.package;
  }

  // Super block includes die
  if (superblock & INDEX_DIE) {
    param.superPageSize *= param.die;
    value[2] = param.die;
  }
  else {
    param.superBlock *= param.die;
  }

  // Super block includes plane
  if (conf.readBoolean(CONFIG_PAL, NAND_USE_MULTI_PLANE_OP) ||
      (superblock & INDEX_PLANE)) {
    param.superPageSize *= param.plane;
    value[3] = param.plane;
  }
  else {
    param.superBlock *= param.plane;
  }

  // Partial I/O tweak
  param.pageInSuperPage = param.superPageSize / param.pageSize;

  // TODO: If PAL revised, this code may not needed
  if (conf.readBoolean(CONFIG_PAL, NAND_USE_MULTI_PLANE_OP)) {
    param.pageInSuperPage /= param.plane;
  }
const char* components[4] = {"Channel", "Package", "Die", "Plane"};
//uint32_t value[4] = {param.channel, param.package, param.die, param.plane};

// Iterate through the components to check if they are part of the superblock
for (int i = 0; i < 4; ++i) {
    // Check if the bit related to the component is set in the superblock configuration
    if (superblock & (1 << i)) {
        printf("%s is part of the superblock with value: %u\n", components[i], value[i]);
    }
}
  // Print super block information
  printf("Channel |   Way   |   Die   |  Plane  |  Block  |   Page  \n");
  printf( "%7u | %7u | %7u | %7u | %7u | %7u\n", param.channel,
             param.package, param.die, param.plane, param.block, param.page);
  /*debugprint(LOG_PAL, "Multi-plane mode %s",
             conf.readBoolean(CONFIG_PAL, NAND_USE_MULTI_PLANE_OP)
                 ? "enabled"
                // : "disabled");*/
 printf( "Multi-plane mode %s", conf.readBoolean(CONFIG_PAL, NAND_USE_MULTI_PLANE_OP)
                 ? "enabled\n"
                 : "disabled\n");
  debugprint(LOG_PAL, "Superblock multiplier");

  for (int i = 0; i < 4; i++) {
    if (superblock & (1 << i)) {
      debugprint(LOG_PAL, "x%u (%s)", value[i], name[i]);
      printf("LOG_PAL, x%u (%s)\n", value[i], name[i]);
    }
  }

  debugprint(LOG_PAL, "Page size %u -> Super Page Size %u", param.pageSize,
             param.superPageSize);
  debugprint(
      LOG_PAL, "Total block count %u -> Super Block Count %u",
      param.channel * param.package * param.die * param.plane * param.block,
      param.superBlock);;;;;

  pPAL = new PALOLD(param, c);;
}

PAL::~PAL() {
  delete pPAL;
}

void PAL::read(Request &req, uint64_t &tick) {
  pPAL->read(req, tick);
}

void PAL::write(Request &req, uint64_t &tick) {
  pPAL->write(req, tick);
}

void PAL::erase(Request &req, uint64_t &tick) {
  pPAL->erase(req, tick);
}

void PAL::copyback(uint32_t, uint32_t, uint32_t, uint64_t &) {
  panic("Copyback not implemented");
}

Parameter *PAL::getInfo() {
  return &param;
}

void PAL::getStatList(std::vector<Stats> &list, std::string prefix) {
  pPAL->getStatList(list, prefix + "pal.");
}

void PAL::getStatValues(std::vector<double> &values) {
  pPAL->getStatValues(values);
}

void PAL::resetStatValues() {
  pPAL->resetStatValues();;
}

}  // namespace PAL

}  // namespace SimpleSSD
