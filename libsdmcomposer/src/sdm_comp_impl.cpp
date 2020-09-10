/*
* Copyright (c) 2020, The Linux Foundation. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without modification, are permitted
* provided that the following conditions are met:
*    * Redistributions of source code must retain the above copyright notice, this list of
*      conditions and the following disclaimer.
*    * Redistributions in binary form must reproduce the above copyright notice, this list of
*      conditions and the following disclaimer in the documentation and/or other materials provided
*      with the distribution.
*    * Neither the name of The Linux Foundation nor the names of its contributors may be used to
*      endorse or promote products derived from this software without specific prior written
*      permission.
*
* THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
* NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
* BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
* OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "sdm_comp_impl.h"
#include "core/sdm_types.h"
#include "sdm_comp_display_builtin.h"
#include "debug_handler.h"
#include "sdm_comp_service.h"

#include <thread>

#define __CLASS__ "SDMCompImpl"

namespace sdm {

int SDMCompImpl::Init() {
  lock_guard<recursive_mutex> obj(recursive_mutex_);
  sdm_comp_service_ = new SDMCompService(this);
  int ret = sdm_comp_service_->Init();
  if (ret != 0) {
    DLOGE("SDMCompService Init failed!! %d\n", ret);
    return ret;
  }
  std::thread ([=] { SDMCompService::QRTREventHandler(sdm_comp_service_); }).detach();

  DisplayError error = CoreInterface::CreateCore(&buffer_allocator_, &buffer_sync_handler_,
                                                 NULL, &core_intf_);
  if (error != kErrorNone) {
    DLOGE("Failed to create CoreInterface");
    sdm_comp_service_->Deinit();
    delete sdm_comp_service_;
    sdm_comp_service_ = nullptr;
    return -EINVAL;
  }

  return 0;
}

int SDMCompImpl::Deinit() {
  lock_guard<recursive_mutex> obj(recursive_mutex_);
  if (sdm_comp_service_) {
    sdm_comp_service_->Deinit();
    delete sdm_comp_service_;
    sdm_comp_service_ = nullptr;
  }

  DisplayError error = CoreInterface::DestroyCore();
  if (error != kErrorNone) {
    DLOGE("Display core de-initialization failed. Error = %d", error);
    return -EINVAL;
  }
  return 0;
}

int SDMCompImpl::CreateDisplay(SDMCompDisplayType display_type, CallbackInterface *callback,
                               Handle *disp_hnd) {
  lock_guard<recursive_mutex> obj(recursive_mutex_);
  if (!disp_hnd || display_type >= kSDMCompDisplayTypeMax) {
    return -EINVAL;
  }

  int status = 0;

  HWDisplaysInfo hw_displays_info = {};
  DisplayError error = core_intf_->GetDisplaysStatus(&hw_displays_info);
  if (error != kErrorNone) {
    DLOGE("Failed to get connected display list. Error = %d", error);
    return -EINVAL;
  }

  for (auto &iter : hw_displays_info) {
    auto &info = iter.second;

    if (info.display_type != kBuiltIn) {
      continue;
    }

    if ((display_type == kSDMCompDisplayTypePrimary && !info.is_primary) ||
        (display_type != kSDMCompDisplayTypePrimary && info.is_primary)) {
      continue;
    }

    if (!info.is_connected) {
      continue;
    }

    DLOGI("Create builtin display, id = %d, type = %d", info.display_id, display_type);
    SDMCompDisplayBuiltIn *display_builtin = new SDMCompDisplayBuiltIn(core_intf_, callback,
                                                               display_type, info.display_id);
    status = display_builtin->Init();
    if (status) {
      delete display_builtin;
      display_builtin = nullptr;
      return status;
    }
    *disp_hnd = display_builtin;
    break;
  }

  return status;
}

int SDMCompImpl::DestroyDisplay(Handle disp_hnd) {
  lock_guard<recursive_mutex> obj(recursive_mutex_);
  if (!disp_hnd) {
    DLOGE("Display handle is NULL");
    return -EINVAL;
  }

  SDMCompDisplayBuiltIn *sdm_comp_display = reinterpret_cast<SDMCompDisplayBuiltIn *>(disp_hnd);
  int status = sdm_comp_display->Deinit();
  if (status != 0) {
    return status;
  }
  delete sdm_comp_display;

  return 0;
}

int SDMCompImpl::GetDisplayAttributes(Handle disp_hnd,
                                          SDMCompDisplayAttributes *display_attributes) {
  lock_guard<recursive_mutex> obj(recursive_mutex_);
  if (!disp_hnd) {
    DLOGE("Invalid input param disp_hnd %d, display_attributes %d", disp_hnd, display_attributes);
    return -EINVAL;
  }

  SDMCompDisplayBuiltIn *sdm_comp_display = reinterpret_cast<SDMCompDisplayBuiltIn *>(disp_hnd);
  return sdm_comp_display->GetDisplayAttributes(display_attributes);
}


int SDMCompImpl::ShowBuffer(Handle disp_hnd, BufferHandle *buf_handle, int32_t *retire_fence) {
  lock_guard<recursive_mutex> obj(recursive_mutex_);
  if (!disp_hnd || !buf_handle || !retire_fence) {
    DLOGE("Invalid input param disp_hnd %d, buf_handle %d, retire_fence %d", disp_hnd, buf_handle,
          retire_fence);
    return -EINVAL;
  }

  SDMCompDisplayBuiltIn *sdm_comp_display = reinterpret_cast<SDMCompDisplayBuiltIn *>(disp_hnd);
  return sdm_comp_display->ShowBuffer(buf_handle, retire_fence);
}

int SDMCompImpl::SetColorModeWithRenderIntent(Handle disp_hnd, struct ColorMode mode) {
  lock_guard<recursive_mutex> obj(recursive_mutex_);
  if (!disp_hnd) {
    DLOGE("Invalid input param disp_hnd %d", disp_hnd);
    return -EINVAL;
  }

  SDMCompDisplayBuiltIn *sdm_comp_display = reinterpret_cast<SDMCompDisplayBuiltIn *>(disp_hnd);
  return sdm_comp_display->SetColorModeWithRenderIntent(mode);
}

int SDMCompImpl::GetColorModes(Handle disp_hnd, uint32_t *out_num_modes,
                               struct ColorMode *out_modes) {
  lock_guard<recursive_mutex> obj(recursive_mutex_);
  if (!disp_hnd || !out_num_modes || !out_modes) {
    DLOGE("Invalid input param disp_hnd %d, out_num_modes %d, out_modes %d", disp_hnd, out_num_modes,
          out_modes);
    return -EINVAL;
  }

  SDMCompDisplayBuiltIn *sdm_comp_display = reinterpret_cast<SDMCompDisplayBuiltIn *>(disp_hnd);
  return sdm_comp_display->GetColorModes(out_num_modes, out_modes);
}

}  // namespace sdm
