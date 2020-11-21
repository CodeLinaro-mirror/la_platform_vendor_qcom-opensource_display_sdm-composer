/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *  * Neither the name of The Linux Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __SDMCOMP_SERVICE_INTF_H__
#define __SDMCOMP_SERVICE_INTF_H__

#include <stdarg.h>
#include "libqrtr.h"

namespace sdm {
class SDMCompServiceIntf;

enum SDMCompServiceEvents {
  kEventSetPanelBrightness,
  kEventSetDisplayConfig,
  kEventImportDemuraBuffers,
  kEventMax,
};

struct SDMCompServiceDispConfigs {
  uint32_t x_res = 0;
  uint32_t y_res = 0;
  uint32_t fps = 0;
  bool smart_panel = false;
  int config_idx = -1;
};

struct SDMCompServiceDemuraBufInfo {
  int calib_buf_fd = -1;
  int hfc_buf_fd = -1;
  uint32_t calib_buf_size = 0;
  uint32_t hfc_buf_size = 0;
  uint64_t panel_id = 0;
};

class SDMCompServiceCbIntf {
 public:
  virtual int OnEvent(SDMCompServiceEvents event_type, ...) = 0;

 protected:
  virtual ~SDMCompServiceCbIntf() { }
};

class SDMCompServiceIntf {
 public:
  static int Create(SDMCompServiceCbIntf *callback, SDMCompServiceIntf **intf);
  static int Destroy(SDMCompServiceIntf *intf);
 protected:
  virtual ~SDMCompServiceIntf() { }
};

}
#endif  // __SDMCOMP_SERVICE_INTF_H__

