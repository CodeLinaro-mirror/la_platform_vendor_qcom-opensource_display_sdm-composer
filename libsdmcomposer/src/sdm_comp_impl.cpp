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
#include "debug_handler.h"

#define __CLASS__ "SDMCompImpl"

namespace sdm {

SDMCompImpl *SDMCompImpl::sdm_comp_impl_ = nullptr;
SDMCompDisplayBuiltIn *SDMCompImpl::display_builtin_[kSDMCompDisplayTypeMax] = { nullptr };
uint32_t SDMCompImpl::ref_count_ = 0;
uint32_t SDMCompImpl::disp_ref_count_[kSDMCompDisplayTypeMax] = { 0 };

SDMCompImpl *SDMCompImpl::GetInstance() {
  if (!sdm_comp_impl_) {
    sdm_comp_impl_= new SDMCompImpl();
  }

  return sdm_comp_impl_;
}

int SDMCompImpl::Init() {
  lock_guard<recursive_mutex> obj(recursive_mutex_);
  if (ref_count_) {
    ref_count_++;
    return 0;
  }
  int ret = SDMCompServiceIntf::Create(this, &sdm_comp_service_intf_);
  if (ret != 0) {
    DLOGW("SDMCompServiceIntf create failed!! %d\n", ret);
    return ret;
  }

  DisplayError error = CoreInterface::CreateCore(&buffer_allocator_, &buffer_sync_handler_,
                                                 NULL, NULL, &core_intf_);
  if (error != kErrorNone) {
    DLOGE("Failed to create CoreInterface");
    SDMCompServiceIntf::Destroy(sdm_comp_service_intf_);
    return -EINVAL;
  }
  ref_count_++;

  return 0;
}

int SDMCompImpl::Deinit() {
  lock_guard<recursive_mutex> obj(recursive_mutex_);
  if (ref_count_) {
    ref_count_--;
    if (!ref_count_) {
      if (sdm_comp_service_intf_) {
        SDMCompServiceIntf::Destroy(sdm_comp_service_intf_);
      }

      DisplayError error = CoreInterface::DestroyCore();
      if (error != kErrorNone) {
        DLOGE("Display core de-initialization failed. Error = %d", error);
        return -EINVAL;
      }
      delete sdm_comp_impl_;
      sdm_comp_impl_ = nullptr;
    }
  }
  return 0;
}

int SDMCompImpl::CreateDisplay(SDMCompDisplayType display_type, CallbackInterface *callback,
                               Handle *disp_hnd) {
  lock_guard<recursive_mutex> obj(recursive_mutex_);
  if (!disp_hnd || display_type >= kSDMCompDisplayTypeMax) {
    return -EINVAL;
  }

  if (display_builtin_[display_type]) {
    *disp_hnd = display_builtin_[display_type];
    disp_ref_count_[display_type]++;
    return 0;
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
    display_builtin_[display_type] = display_builtin;
    disp_ref_count_[display_type]++;
    HandlePendingEvents();
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
  SDMCompDisplayType disp_type = sdm_comp_display->GetDisplayType();
  if (disp_ref_count_[disp_type]) {
    disp_ref_count_[disp_type]--;
    if (!disp_ref_count_[disp_type]) {
      int status = sdm_comp_display->Deinit();
      if (status != 0) {
        return status;
      }
      DLOGI("Destroying builtin display %d", disp_type);
      delete display_builtin_[disp_type];
      display_builtin_[disp_type] = nullptr;
    }
  }
  return 0;
}

int SDMCompImpl::GetDisplayAttributes(Handle disp_hnd,
                                          SDMCompDisplayAttributes *display_attributes) {
  lock_guard<recursive_mutex> obj(recursive_mutex_);
  if (!disp_hnd || !display_attributes) {
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

int SDMCompImpl::SetPanelBrightness(Handle disp_hnd, float brightness_level) {
  if (!disp_hnd) {
    DLOGE("Invalid input param disp_hnd %d", disp_hnd);
    return -EINVAL;
  }

  SDMCompDisplayBuiltIn *sdm_comp_display = reinterpret_cast<SDMCompDisplayBuiltIn *>(disp_hnd);
  return sdm_comp_display->SetPanelBrightness(brightness_level);
}

int SDMCompImpl::SetMinPanelBrightness(Handle disp_hnd, float min_brightness_level) {
  if (!disp_hnd) {
    DLOGE("Invalid input param disp_hnd %d", disp_hnd);
    return -EINVAL;
  }
  SDMCompDisplayBuiltIn *sdm_comp_display = reinterpret_cast<SDMCompDisplayBuiltIn *>(disp_hnd);
  int err = sdm_comp_display->SetMinPanelBrightness(min_brightness_level);
  if (err == 0) {
    SDMCompDisplayType disp_type = sdm_comp_display->GetDisplayType();
    if (panel_brightness_[disp_type] < min_brightness_level) {
      return SetPanelBrightness(disp_hnd, min_brightness_level);
    }
  }
  return err;
}

int SDMCompImpl::OnEvent(SDMCompServiceEvents event, ...) {
  lock_guard<recursive_mutex> obj(recursive_mutex_);
  int err = 0;
  va_list arguments;
  va_start(arguments, event);
  switch (event) {
  case kEventSetPanelBrightness: {
    SDMCompDisplayType disp_type = (SDMCompDisplayType)(va_arg(arguments, int));
    float panel_brightness = FLOAT(va_arg(arguments, double));
    if (display_builtin_[disp_type]) {
      int err = display_builtin_[disp_type]->SetPanelBrightness(panel_brightness);
      DLOGI("Setting panel brightness value %f on display type %d is %s", panel_brightness,
            disp_type, err ? "failed" : "successful");
    } else {
      pending_events_.emplace(std::make_pair(kEventSetPanelBrightness, disp_type));
      panel_brightness_[disp_type] = panel_brightness;
      DLOGI("Cache panel brightness value %f on display type %d", panel_brightness, disp_type);
    }
  } break;

  case kEventSetDisplayConfig: {
    SDMCompDisplayType disp_type = (SDMCompDisplayType)(va_arg(arguments, int));
    SDMCompServiceDispConfigs *disp_configs =
        reinterpret_cast<SDMCompServiceDispConfigs*>(va_arg(arguments, Handle));
    if (display_builtin_[disp_type]) {
      DLOGW("Setting display config is not supported in the middle of TUI session");
      err = -ENOTSUP;
    } else {
      pending_events_.emplace(std::make_pair(kEventSetDisplayConfig, disp_type));
      disp_configs_[disp_type] = *disp_configs;
      DLOGI("Cache display config idx %d, WxH %dx%d, fps %d, %s panel for display type %d",
            disp_configs->config_idx, disp_configs->x_res, disp_configs->y_res, disp_configs->fps,
            disp_configs->smart_panel ? "cmdmode" : "videomode", disp_type);
    }
  } break;

  case kEventImportDemuraBuffers: {
    SDMCompDisplayType disp_type = (SDMCompDisplayType)(va_arg(arguments, int));
    SDMCompServiceDemuraBufInfo *demura_buf_info =
        reinterpret_cast<SDMCompServiceDemuraBufInfo*>(va_arg(arguments, Handle));
    if (display_builtin_[disp_type]) {
      DLOGW("Import demura buffers is not supported in the middle of TUI session");
      err = -ENOTSUP;
    } else {
      demura_buf_info_[disp_type] = *demura_buf_info;
    }
  } break;

  default:
    err = -EINVAL;
    break;
  }
  va_end(arguments);
  return err;
}

void SDMCompImpl::HandlePendingEvents() {
  for (auto pending_event : pending_events_) {
    SDMCompDisplayType display_type = pending_event.second;
    switch (pending_event.first) {
    case kEventSetDisplayConfig: {
      SDMCompDisplayAttributes disp_attributes = {};
      int config_idx = disp_configs_[display_type].config_idx;
      int err = display_builtin_[display_type]->GetDisplayAttributes(config_idx, &disp_attributes);
      if (err == 0) {
        if (disp_attributes.x_res != disp_configs_[display_type].x_res ||
            disp_attributes.y_res != disp_configs_[display_type].y_res ||
            disp_attributes.fps != disp_configs_[display_type].fps ||
            disp_attributes.smart_panel != disp_configs_[display_type].smart_panel) {
          DLOGW("Invalid display attributes for the given mode");
          continue;
        }
        err = display_builtin_[display_type]->SetDisplayConfig(config_idx);
        if (err != 0) {
          continue;
        }
        DLOGI("Setting display config idx %d, WxH %dx%d, fps %d, %s panel for display type %d",
              disp_configs_[display_type].config_idx, disp_configs_[display_type].x_res,
              disp_configs_[display_type].y_res, disp_configs_[display_type].fps,
              disp_configs_[display_type].smart_panel ? "cmdmode" : "videomode", display_type);
      }
    } break;

    case kEventSetPanelBrightness: {
      int err = display_builtin_[display_type]->SetPanelBrightness(panel_brightness_[display_type]);
      DLOGI("SetPanelBrightness value %f on display type %d is %s", panel_brightness_[display_type],
             display_type, err ? "failed" : "successful");
    } break;

    default:
      break;
    }
  }
  pending_events_.clear();
}

}  // namespace sdm
