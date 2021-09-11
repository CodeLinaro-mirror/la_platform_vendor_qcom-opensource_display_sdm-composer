/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
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
#include "sdm_comp_service_intf.h"
#include "utils/sys.h"
#include "sdm_comp_service_extn_intf.h"
#include "sdm_comp_interface.h"

#include <mutex>

using std::mutex;

namespace sdm {

class SDMCompService : public SDMCompServiceIntf {
 public:
  explicit SDMCompService(SDMCompServiceCbIntf *callback) : callback_(callback) { }
  int Init();
  int Deinit();
  static int QRTREventHandler(SDMCompService *sdm_comp_service);
  ~SDMCompService() { }

 private:
  void CommandHandler(const struct qrtr_packet &qrtr_pkt);
  void SendResponse(int node, int port, const Response &rsp);
  void HandleImportDemuraBuffers(const struct qrtr_packet &qrtr_pkt);
  void HandleSetBacklight(const struct qrtr_packet &qrtr_pkt);
  void HandleSetDisplayConfigs(const struct qrtr_packet &qrtr_pkt);
  void HandleSetProperties(const struct qrtr_packet &qrtr_pkt);
  SDMCompDisplayType GetSDMCompDisplayType(DisplayType disp_type);

  int (*QrtrOpen)(int rport);
  void (*QrtrClose)(int sock);
  int (*QrtrSendTo)(int sock, uint32_t node, uint32_t port, const void *data, unsigned int sz);
  int (*QrtrPublish)(int sock, uint32_t service, uint16_t version, uint16_t instance);
  int (*QrtrBye)(int sock, uint32_t service, uint16_t version, uint16_t instance);
  int (*QrtrPoll)(int sock, unsigned int ms);
  int (*QrtrDecode)(struct qrtr_packet *dest, void *buf, size_t len,
                    const struct sockaddr_qrtr *sq);
  std::mutex qrtr_lock_;
  int qrtr_fd_ = -1;
  DynLib qrtr_lib_;

  SDMCompServiceCbIntf *callback_ = NULL;
  MemBuf *mem_buf_ = nullptr;
  DynLib extension_lib_;
  CreateSDMCompExtnIntf create_sdm_comp_extn_intf_ = nullptr;
  DestroySDMCompExtnIntf destroy_sdm_comp_extn_intf_ = nullptr;
  SDMCompServiceExtnIntf *sdm_comp_service_extn_intf_ = nullptr;
  bool init_done_ = false;
};

}  // namespace sdm

#endif  // __SDM_COMP_SERVICE_H__

