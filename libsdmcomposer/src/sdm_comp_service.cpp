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

#include <errno.h>
#include <unistd.h>

#include <mutex>
#include <algorithm>

#include "sdm_comp_service.h"
#include "debug_handler.h"

#define __CLASS__ "SDMCompService"

namespace sdm {

int SDMCompService::Init() {
  std::lock_guard<std::mutex> lock(qrtr_lock_);
  int err = 0;

  qrtr_fd_ = qrtr_open(0);
  if (qrtr_fd_ < 0) {
    DLOGE("Failed to create qrtr socket");
    err = -EINVAL;
    goto cleanup;
  }

  err = qrtr_publish(qrtr_fd_, SDM_COMP_SERVICE_ID, SDM_COMP_SERVICE_VERSION,
                     SDM_COMP_SERVICE_INSTANCE);
  if (err < 0) {
    DLOGE("failed to publish rmtfs service %d", err);
    goto cleanup;
  }

  err = MemBuf::GetInstance(&mem_buf_);
  if (err != 0) {
    DLOGE("MemBuf::GetInstance failed!! %d\n", err);
    goto cleanup;
  }

  // Try to load extension library & get handle to its interface.
  if (extension_lib_.Open(EXTN_LIB_NAME)) {
    if (!extension_lib_.Sym(CREATE_SDMCOMP_SERVICE_EXTN,
                            reinterpret_cast<void **>(&create_sdm_comp_extn_intf_)) ||
        !extension_lib_.Sym(DESTROY_SDMCOMP_SERVICE_EXTN,
                            reinterpret_cast<void **>(&destroy_sdm_comp_extn_intf_))) {
      DLOGE("Unable to load symbols, error = %s", extension_lib_.Error());
      err = -ENOENT;
      goto cleanup;
    }

    err = create_sdm_comp_extn_intf_(qrtr_fd_, sdm_comp_intf_, &sdm_comp_service_extn_intf_);
    if (err != 0) {
      DLOGE("Unable to create sdm comp service extenstion interface");
      goto cleanup;
    }
  } else {
    DLOGW("Unable to load = %s, error = %s", EXTN_LIB_NAME, extension_lib_.Error());
  }

  return 0;
cleanup:
  Deinit();

  return err;
}

int SDMCompService::Deinit() {
  std::lock_guard<std::mutex> lock(qrtr_lock_);

  destroy_sdm_comp_extn_intf_(sdm_comp_service_extn_intf_);

  if (mem_buf_) {
    MemBuf::PutInstance();
  }
  if (demura_cfg_buf_fd_ > 0) {
    close(demura_cfg_buf_fd_);
    demura_cfg_buf_fd_ = -1;
  }
  if (demura_hfc_buf_fd_ > 0) {
    close(demura_hfc_buf_fd_);
    demura_hfc_buf_fd_ = -1;
  }
  if (qrtr_fd_ > 0) {
    qrtr_bye(qrtr_fd_, SDM_COMP_SERVICE_ID, SDM_COMP_SERVICE_VERSION,
             SDM_COMP_SERVICE_INSTANCE);
    qrtr_close(qrtr_fd_);
  }
  return 0;
}

void SDMCompService::SendResponse(const Response &rsp) {
  int ret = qrtr_sendto(qrtr_fd_, qrtr_node_, qrtr_port_, &rsp, sizeof(rsp));
  if (ret < 0) {
    DLOGE("Failed to send response for command %d ret %d", rsp.id, ret);
  }
}

int SDMCompService::GetImportedDemuraBuffers(int *cfg_buf_fd, int *hfc_buf_fd) {
  if (!cfg_buf_fd || !hfc_buf_fd) {
    return -EINVAL;
  }
  *cfg_buf_fd = demura_cfg_buf_fd_;
  *hfc_buf_fd = demura_hfc_buf_fd_;
  return 0;
}

void SDMCompService::ImportDemuraBuffers(const struct qrtr_packet &qrtr_pkt) {
  Response rsp = {};
  Command *cmd = reinterpret_cast<Command *>(qrtr_pkt.data);
  rsp.id = cmd->id;
  DemuraMemHandle *demura_mem_hdl = &cmd->cmd_export_demura_buf.demura_mem_handle;

  int error = mem_buf_->Import(demura_mem_hdl->cfg_mem_hdl, &demura_cfg_buf_fd_);
  if (error != 0) {
    DLOGW("Import failed with %d", error);
    rsp.status = error;
    SendResponse(rsp);
    return;
  }

  error = mem_buf_->Import(demura_mem_hdl->hfc_mem_hdl, &demura_hfc_buf_fd_);
  if (error != 0) {
    DLOGW("Import failed with %d", error);
    close(demura_cfg_buf_fd_);
    demura_cfg_buf_fd_ = -1;
    rsp.status = error;
  }

  SendResponse(rsp);
}

void SDMCompService::CommandHandler(const struct qrtr_packet &qrtr_pkt) {
  if (qrtr_pkt.data_len < sizeof(Command)) {
    DLOGW("Invalid packet!! length %zu", qrtr_pkt.data_len);
    return;
  }

  if (qrtr_port_ == -1 || qrtr_node_ == -1) {
    qrtr_port_ = qrtr_pkt.port;
    qrtr_node_ = qrtr_pkt.node;
  }
  Command *cmd = reinterpret_cast<Command *>(qrtr_pkt.data);

  DLOGI("Received command %d from client fd %d node %d port %d", cmd->id, qrtr_fd_, qrtr_pkt.node,
         qrtr_pkt.port);

  switch (cmd->id) {
    case kCmdExportDemuraBuffers:
      ImportDemuraBuffers(qrtr_pkt);
      break;
    default:
      sdm_comp_service_extn_intf_->CommandHandler(qrtr_pkt);
      break;
  }
}

int SDMCompService::QRTREventHandler(SDMCompService *sdm_comp_service) {
  struct sockaddr_qrtr soc_qrtr = {};
  struct qrtr_packet qrtr_pkt = {};
  socklen_t soc_len;
  char buf[4096] = {};

  while(1) {
    int ret = qrtr_poll(sdm_comp_service->qrtr_fd_, -1);
    if (ret < 0) {
      continue;
    }

    soc_len = sizeof(soc_qrtr);
    ret = recvfrom(sdm_comp_service->qrtr_fd_, buf, sizeof(buf), 0, (sockaddr *)&soc_qrtr,
                   &soc_len);
    if (ret < 0) {
      if (errno == EAGAIN) {
        continue;
      }
      return ret;
    }

    {
      std::lock_guard<std::mutex> lock(sdm_comp_service->qrtr_lock_);

      ret = qrtr_decode(&qrtr_pkt, buf, ret, &soc_qrtr);
      if (ret < 0) {
        DLOGE("failed to decode incoming message");
        continue;
      }

      switch (qrtr_pkt.type) {
        case QRTR_TYPE_DEL_CLIENT:
          if (sdm_comp_service->qrtr_port_ == qrtr_pkt.port &&
              sdm_comp_service->qrtr_node_ == qrtr_pkt.node) {
            DLOGI("Client with port %d node %d is disconnected", qrtr_pkt.port, qrtr_pkt.node);
          }
          break;

        case QRTR_TYPE_BYE:
          DLOGI("System server goes down");
          break;

        case QRTR_TYPE_DATA:
          sdm_comp_service->CommandHandler(qrtr_pkt);
          break;
      }
    }
  }

  return 0;
}

}
