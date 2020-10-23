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
  // Try to load qrtr library & get handle to its interface.
  if (qrtr_lib_.Open("libqrtr.so.1.0.0")) {
    if (!qrtr_lib_.Sym("qrtr_open", reinterpret_cast<void **>(&QrtrOpen)) ||
        !qrtr_lib_.Sym("qrtr_close", reinterpret_cast<void **>(&QrtrClose)) ||
        !qrtr_lib_.Sym("qrtr_sendto", reinterpret_cast<void **>(&QrtrSendTo)) ||
        !qrtr_lib_.Sym("qrtr_publish", reinterpret_cast<void **>(&QrtrPublish)) ||
        !qrtr_lib_.Sym("qrtr_bye", reinterpret_cast<void **>(&QrtrBye)) ||
        !qrtr_lib_.Sym("qrtr_poll", reinterpret_cast<void **>(&QrtrPoll)) ||
        !qrtr_lib_.Sym("qrtr_decode", reinterpret_cast<void **>(&QrtrDecode))) {
      DLOGE("Unable to load symbols, error = %s", qrtr_lib_.Error());
      return-ENOENT;
    }
  } else {
    DLOGE("Unable to load libqrtr.so, error = %s", qrtr_lib_.Error());
    return-ENOENT;
  }

  qrtr_fd_ = QrtrOpen(0);
  if (qrtr_fd_ < 0) {
    DLOGE("Failed to create qrtr socket");
    err = -EINVAL;
    goto cleanup;
  }

  err = QrtrPublish(qrtr_fd_, SDM_COMP_SERVICE_ID, SDM_COMP_SERVICE_VERSION,
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

  init_done_ = true;

  return 0;
cleanup:
  Deinit();

  return err;
}

int SDMCompService::Deinit() {
  std::lock_guard<std::mutex> lock(qrtr_lock_);

  if (destroy_sdm_comp_extn_intf_) {
    destroy_sdm_comp_extn_intf_(sdm_comp_service_extn_intf_);
  }

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
    QrtrBye(qrtr_fd_, SDM_COMP_SERVICE_ID, SDM_COMP_SERVICE_VERSION,
             SDM_COMP_SERVICE_INSTANCE);
    QrtrClose(qrtr_fd_);
  }
  return 0;
}

void SDMCompService::SendResponse(int node, int port, const Response &rsp) {
  int ret = QrtrSendTo(qrtr_fd_, node, port, &rsp, sizeof(rsp));
  if (ret < 0) {
    DLOGE("Failed to send response for command %d ret %d", rsp.id, ret);
  }
  DLOGI("Sent response for the command id %d size %d", rsp.id, sizeof(rsp));
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
  Command *cmd = reinterpret_cast<Command *>(qrtr_pkt.data);
  Response rsp = {};
  rsp.id = cmd->id;
  DemuraMemHandle *demura_mem_hdl = &cmd->cmd_export_demura_buf.demura_mem_handle;

  int error = mem_buf_->Import(demura_mem_hdl->cfg_mem_hdl, &demura_cfg_buf_fd_);
  if (error != 0) {
    DLOGW("Import failed with %d", error);
    rsp.status = error;
    SendResponse(qrtr_pkt.node, qrtr_pkt.port, rsp);
    return;
  }

  error = mem_buf_->Import(demura_mem_hdl->hfc_mem_hdl, &demura_hfc_buf_fd_);
  if (error != 0) {
    DLOGW("Import failed with %d", error);
    close(demura_cfg_buf_fd_);
    demura_cfg_buf_fd_ = -1;
    rsp.status = error;
  }

  SendResponse(qrtr_pkt.node, qrtr_pkt.port, rsp);
}

void SDMCompService::HandleSetBacklight(const struct qrtr_packet &qrtr_pkt) {
  Command *cmd = reinterpret_cast<Command *>(qrtr_pkt.data);
  Response rsp = {};
  rsp.id = cmd->id;
  CmdSetBacklight *cmd_backlight = reinterpret_cast<CmdSetBacklight*>(&cmd->cmd_set_backlight);
  SDMCompDisplayType sdm_comp_disp_type = GetSDMCompDisplayType(cmd_backlight->disp_type);

  if (sdm_comp_disp_type == kSDMCompDisplayTypeMax) {
    DLOGE("Invalid display_type %d", sdm_comp_disp_type);
    rsp.status = -EINVAL;
    SendResponse(qrtr_pkt.node, qrtr_pkt.port, rsp);
    return;
  }
  std::lock_guard<std::mutex> lock(disp_lock_);

  if (display_hnd_[sdm_comp_disp_type]) {
    int err = sdm_comp_intf_->SetPanelBrightness(display_hnd_[sdm_comp_disp_type],
                                                 cmd_backlight->brightness);

    DLOGI("SetPanelBrightness value %f on display type %d is %s", cmd_backlight->brightness,
           cmd_backlight->disp_type, err ? "failed" : "successful");
    rsp.status = err;
  } else {
    cache_panel_brightness_[sdm_comp_disp_type] = true;
    panel_brightness_[sdm_comp_disp_type] = cmd_backlight->brightness;
    DLOGI("Caching panel brightness value %f for display type %d", cmd_backlight->brightness,
           cmd_backlight->disp_type);
    rsp.status = 0;
  }
  SendResponse(qrtr_pkt.node, qrtr_pkt.port, rsp);
}

void SDMCompService::CommandHandler(const struct qrtr_packet &qrtr_pkt) {
  Response rsp = {};
  rsp.status = -EINVAL;
  if (qrtr_pkt.data_len < sizeof(Command)) {
    DLOGW("Invalid packet!! length %zu command buffer size %d", qrtr_pkt.data_len, sizeof(Command));
    SendResponse(qrtr_pkt.node, qrtr_pkt.port, rsp);
    return;
  }
  Command *cmd = reinterpret_cast<Command *>(qrtr_pkt.data);
  if (!cmd) {
    DLOGE("cmd is null!!\n");
    SendResponse(qrtr_pkt.node, qrtr_pkt.port, rsp);
    return;
  }
  rsp.id = cmd->id;

  if (cmd->id < kCmdMax) {
    if (cmd->version != VM_INTF_VERSION) {
      DLOGE("Invalid vm interface client version %x, supported version %x", cmd->version,
            VM_INTF_VERSION);
      SendResponse(qrtr_pkt.node, qrtr_pkt.port, rsp);
      return;
    }
  }

  DLOGI("Received command %d from client fd %d node %d port %d", cmd->id, qrtr_fd_, qrtr_pkt.node,
         qrtr_pkt.port);

  switch (cmd->id) {
    case kCmdExportDemuraBuffers:
      ImportDemuraBuffers(qrtr_pkt);
      break;
    case kCmdSetBacklight: {
      HandleSetBacklight(qrtr_pkt);
    } break;
    default:
      if (sdm_comp_service_extn_intf_) {
        sdm_comp_service_extn_intf_->CommandHandler(qrtr_pkt);
      }
      break;
  }
}

int SDMCompService::QRTREventHandler(SDMCompService *sdm_comp_service) {
  struct sockaddr_qrtr soc_qrtr = {};
  struct qrtr_packet qrtr_pkt = {};
  socklen_t soc_len;
  char buf[4096] = {};
  DLOGI("Start listening to the client request %d", SDM_COMP_SERVICE_ID);
  while(sdm_comp_service->init_done_) {
    int ret = sdm_comp_service->QrtrPoll(sdm_comp_service->qrtr_fd_, -1);
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

      ret = sdm_comp_service->QrtrDecode(&qrtr_pkt, buf, ret, &soc_qrtr);
      if (ret < 0) {
        DLOGE("failed to decode incoming message");
        continue;
      }

      switch (qrtr_pkt.type) {
        case QRTR_TYPE_DEL_CLIENT:
          DLOGI("Client with port %d node %d is disconnected", qrtr_pkt.port, qrtr_pkt.node);
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

SDMCompDisplayType SDMCompService::GetSDMCompDisplayType(DisplayType disp_type) {
  switch(disp_type) {
    case kDisplayTypePrimary:
      return kSDMCompDisplayTypePrimary;
    case kDisplayTypeSecondary1:
      return kSDMCompDisplayTypeSecondary1;
    default:
      return kSDMCompDisplayTypeMax;
  }
}

int SDMCompService::OnDisplayCreate(Handle display_hnd, SDMCompDisplayType disp_type) {
  std::lock_guard<std::mutex> lock(disp_lock_);
  if (disp_type < kSDMCompDisplayTypePrimary && disp_type >= kSDMCompDisplayTypeMax) {
    DLOGE("Invalid display type %d", disp_type);
    return -EINVAL;
  }
  if (!display_hnd) {
    DLOGE("Invalid display handle %d", display_hnd);
    return -EINVAL;
  }
  if (cache_panel_brightness_[disp_type]) {
    int err = sdm_comp_intf_->SetPanelBrightness(display_hnd, panel_brightness_[disp_type]);
    DLOGI("SetPanelBrightness value %f on display type %d is %s", panel_brightness_[disp_type],
           disp_type, err ? "failed" : "successful");
    cache_panel_brightness_[disp_type] = false;
  }

  display_hnd_[disp_type] = display_hnd;
  return 0;
}

int SDMCompService::OnDisplayDestroy(SDMCompDisplayType disp_type) {
  std::lock_guard<std::mutex> lock(disp_lock_);
  if (disp_type < kSDMCompDisplayTypePrimary && disp_type >= kSDMCompDisplayTypeMax) {
    DLOGE("Invalid display type %d", disp_type);
    return -EINVAL;
  }
  display_hnd_[disp_type] = nullptr;
  return 0;
}

}
