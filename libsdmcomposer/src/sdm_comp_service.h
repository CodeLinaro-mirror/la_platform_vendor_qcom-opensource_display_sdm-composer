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

#ifndef __SDM_COMP_SERVICE_H__
#define __SDM_COMP_SERVICE_H__

#include <stdint.h>

#include "vm_interface.h"
#include "libqrtr.h"
#include "membuf_wrapper.h"
#include "sdm_comp_interface.h"
#include "utils/sys.h"
#include "sdm_comp_service_extn_intf.h"

#include <mutex>

using std::mutex;

namespace sdm {

class SDMCompService    {
 public:
  explicit SDMCompService(SDMCompInterface *sdm_comp_intf) : sdm_comp_intf_(sdm_comp_intf) { }

  int Init();
  int Deinit();
  int GetImportedDemuraBuffers(int *cfg_buf_fd, int *hfc_buf_fd);
  static int QRTREventHandler(SDMCompService *sdm_comp_service);

  ~SDMCompService() { }

 private:
  void CommandHandler(const struct qrtr_packet &qrtr_pkt);
  void ImportDemuraBuffers(const struct qrtr_packet &qrtr_pkt);
  void SendResponse(const Response &rsp);

  std::mutex qrtr_lock_;
  int qrtr_fd_ = -1;
  int qrtr_port_ = -1;
  int qrtr_node_ = -1;

  SDMCompInterface *sdm_comp_intf_ = NULL;
  MemBuf *mem_buf_ = nullptr;
  int demura_cfg_buf_fd_ = -1;
  int demura_hfc_buf_fd_ = -1;
  DynLib extension_lib_;
  CreateSDMCompExtnIntf create_sdm_comp_extn_intf_ = nullptr;
  DestroySDMCompExtnIntf destroy_sdm_comp_extn_intf_ = nullptr;
  SDMCompServiceExtnIntf *sdm_comp_service_extn_intf_ = nullptr;
};

}  // namespace sdm

#endif  // __SDM_COMP_SERVICE_H__

