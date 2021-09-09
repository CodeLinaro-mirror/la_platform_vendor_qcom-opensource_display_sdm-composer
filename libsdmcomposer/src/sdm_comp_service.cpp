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

#include <errno.h>
#include <unistd.h>

#include <mutex>
#include <algorithm>
#include <thread>
#include <cstring>

#include "sdm_comp_service.h"
#include "sdm_comp_debugger.h"

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

    err = create_sdm_comp_extn_intf_(qrtr_fd_, &sdm_comp_service_extn_intf_);
    if (err != 0) {
      DLOGE("Unable to create sdm comp service extenstion interface");
      goto cleanup;
    }
  } else {
    DLOGW("Unable to load = %s, error = %s", EXTN_LIB_NAME, extension_lib_.Error());
  }

  init_done_ = true;

  std::thread ([=] { SDMCompService::QRTREventHandler(this); }).detach();

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

void SDMCompService::HandleImportDemuraBuffers(const struct qrtr_packet &qrtr_pkt) {
  Command *cmd = reinterpret_cast<Command *>(qrtr_pkt.data);
  Response rsp = {};
  rsp.id = cmd->id;
  DemuraMemInfo *demura_mem_info = &cmd->cmd_export_demura_buf.demura_mem_info;
  SDMCompServiceDemuraBufInfo demura_buf_info = {};

  if (demura_mem_info->calib_mem_hdl != -1) {
    int error = mem_buf_->Import(demura_mem_info->calib_mem_hdl, &demura_buf_info.calib_buf_fd);
    if (error != 0) {
      DLOGW("Import failed with %d", error);
      rsp.status = error;
      SendResponse(qrtr_pkt.node, qrtr_pkt.port, rsp);
      return;
    }
    demura_buf_info.calib_buf_size = demura_mem_info->calib_mem_size;
    demura_buf_info.calib_payload_size = demura_mem_info->calib_payload_size;
    demura_buf_info.panel_id = demura_mem_info->panel_id;
    std::memcpy(demura_buf_info.file_name, demura_mem_info->file_name,
        sizeof demura_mem_info->file_name);

    DLOGI("raw fd:%d and size :%u", demura_buf_info.calib_buf_fd, demura_buf_info.calib_buf_size);
  }

  if (demura_mem_info->hfc_mem_hdl != -1) {
    int error = mem_buf_->Import(demura_mem_info->hfc_mem_hdl, &demura_buf_info.hfc_buf_fd);
    if (error != 0) {
      DLOGW("Import failed with %d", error);
      close(demura_buf_info.calib_buf_fd);
      rsp.status = error;
    }
    demura_buf_info.hfc_buf_size = demura_mem_info->hfc_mem_size;
    demura_buf_info.panel_id = demura_mem_info->panel_id;

    DLOGI("hfc fd:%d and size :%u", demura_buf_info.hfc_buf_fd, demura_buf_info.hfc_buf_size);
  }

  if (callback_) {
    int err = callback_->OnEvent(kEventImportDemuraBuffers, &demura_buf_info);
    DLOGI("ImportDemuraBuffers on panel_id %lu is %s", demura_mem_info->panel_id,
          err ? "failed" : "successful");
    rsp.status = err;
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

  if (callback_) {
    int err = callback_->OnEvent(kEventSetPanelBrightness, sdm_comp_disp_type,
                                 cmd_backlight->brightness);
    rsp.status = err;
  }
  SendResponse(qrtr_pkt.node, qrtr_pkt.port, rsp);
}

void SDMCompService::HandleSetDisplayConfigs(const struct qrtr_packet &qrtr_pkt) {
  Command *cmd = reinterpret_cast<Command *>(qrtr_pkt.data);
  Response rsp = {};
  rsp.id = cmd->id;
  CmdSetDisplayConfigs *cmd_disp_configs =
    reinterpret_cast<CmdSetDisplayConfigs*>(&cmd->cmd_set_disp_configs);
  SDMCompDisplayType sdm_comp_disp_type = GetSDMCompDisplayType(cmd_disp_configs->disp_type);

  if (sdm_comp_disp_type == kSDMCompDisplayTypeMax) {
    DLOGE("Invalid display_type %d", sdm_comp_disp_type);
    rsp.status = -EINVAL;
    SendResponse(qrtr_pkt.node, qrtr_pkt.port, rsp);
    return;
  }

  if (callback_) {
    SDMCompServiceDispConfigs disp_configs = {};
    disp_configs.h_total = cmd_disp_configs->h_total;
    disp_configs.v_total = cmd_disp_configs->v_total;
    disp_configs.fps = cmd_disp_configs->fps;
    disp_configs.smart_panel = cmd_disp_configs->smart_panel;
    int err = callback_->OnEvent(kEventSetDisplayConfig, sdm_comp_disp_type, &disp_configs);
    rsp.status = err;
  }
  SendResponse(qrtr_pkt.node, qrtr_pkt.port, rsp);
}

void SDMCompService::HandleSetProperties(const struct qrtr_packet &qrtr_pkt) {
  Command *cmd = reinterpret_cast<Command *>(qrtr_pkt.data);
  Response rsp = {};
  rsp.id = cmd->id;

  CmdSetProperties *cmd_set_props =
    reinterpret_cast<CmdSetProperties *>(&cmd->cmd_set_properties);

  for (int i = 0; i < cmd_set_props->props.count; i++) {
    SDMCompDebugHandler *sdm_comp_dbg_handler =
                         static_cast<SDMCompDebugHandler *>(SDMCompDebugHandler::Get());
    sdm_comp_dbg_handler->SetProperty(cmd_set_props->props.property_list[i].prop_name,
      cmd_set_props->props.property_list[i].value);
    DLOGI("prop idx : %d, name: %s, value :%s", i, cmd_set_props->props.property_list[i].prop_name,
      cmd_set_props->props.property_list[i].value);
  }

  if (callback_) {
    int err = callback_->OnEvent(kEventSetProperties);
    rsp.status = err;
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
    if (cmd->version > VM_INTF_VERSION) {
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
      HandleImportDemuraBuffers(qrtr_pkt);
      break;
    case kCmdSetBacklight: {
      HandleSetBacklight(qrtr_pkt);
    } break;
    case kCmdSetDisplayConfig: {
      HandleSetDisplayConfigs(qrtr_pkt);
    } break;
    case kCmdSetProperties: {
      HandleSetProperties(qrtr_pkt);
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

}
