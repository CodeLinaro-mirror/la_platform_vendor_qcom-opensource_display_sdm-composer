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

#ifndef __SDM_COMP_INTERFACE_H__
#define __SDM_COMP_INTERFACE_H__

#include "buffer_interface.h"

namespace sdm {

typedef void * Handle;

enum SDMCompDisplayType {
  kSDMCompDisplayTypePrimary,       // Defines the display type for primary display
  kSDMCompDisplayTypeSecondary1,    // Defines the display type for secondary builtin display
  kSDMCompDisplayTypeMax,
};

struct SDMCompDisplayAttributes {
  uint32_t vsync_period = 0;  //!< VSync period in nanoseconds.
  uint32_t x_res = 0;         //!< Total number of pixels in X-direction on the display panel.
  uint32_t y_res = 0;         //!< Total number of pixels in Y-direction on the display panel.
  float x_dpi = 0.0f;         //!< Dots per inch in X-direction.
  float y_dpi = 0.0f;         //!< Dots per inch in Y-direction.
  bool is_yuv = false;        //!< If the display output is in YUV format.
};

class CallbackInterface {
 public:
  /*! @brief Callback method to handle any asyn error.

    @details This function need to be implemented by the client which will be called
             by the sdm composer on any hardware hang etc.
  */
  virtual void OnError() = 0;

 protected:
  virtual ~CallbackInterface() { }

  // callbackdata where client can store the context information about display type
  // for which this callback corresponds to.
  Handle callback_data_ = nullptr;
};

class SDMCompInterface {
 public:
  /*! @brief Method to create display composer interface for TUI service.

    @details This function to be called once per the device life cycle. This function creates
             sdm core and opens up the display driver drm interface.

    @param[out] intf - Populates composer interface pointer.

    @return Returns 0 on sucess otherwise errno
  */
  static int Create(SDMCompInterface **intf);


  /*! @brief Method to destroy display composer interface for TUI service.

    @details This function to be called once per the device life cycle. This function destroys
             sdm core and closes the display driver drm interface.

    @param[in] intf - Composer interface pointer which was populated by Create() function.

    @return Returns 0 on sucess otherwise errno
  */
  static int Destroy(SDMCompInterface *intf);


  /*! @brief Method to create display for composer where the TUI to be rendered.

    @details This function to be called on start of TUI session. This function creates primary or
             secondary display based on the display type passed by the client and intialize the
             display.to its appropriate state This function to be called once per display. This is
             also responsible to create single layer to the created display

    @param[in]  display_type - Specifies the type of a display. \link SDMCompDisplayType \endlink
    @param[in]  callback - Pointer to callback interface which handles the async error.
                \link CallbackInterface \endlink
    @param[out] disp_hnd     - pointer to display handle which stores the context of the client.

    @return Returns 0 on sucess otherwise errno
  */
  virtual int CreateDisplay(SDMCompDisplayType display_type, CallbackInterface *callback,
                            Handle *disp_hnd) = 0;


  /*! @brief Method to destroy display for composer where the TUI to be rendered.

    @details This function to be called on end of TUI session. This function destroys primary or
             secondary display based on the display type passed by the client and relinquish all the
             MDP hw resources. This function to be called once per display.

    @param[in] disp_hnd - pointer to display handle which was created during CreateDisplay()

    @return Returns 0 on sucess otherwise errno
  */
  virtual int DestroyDisplay(Handle disp_hnd) = 0;


  /*! @brief Method to get display attributes for a given display handle.

    @param[in] disp_hnd - pointer to display handle which was created during CreateDisplay()
    @param[out] display_attributes - pointer to display attributes to b populated for a given
                                     display handle. \link SDMCompDisplayAttributes \endlink.

    @return Returns 0 on sucess otherwise errno
  */
  virtual int GetDisplayAttributes(Handle disp_hnd,
                                   SDMCompDisplayAttributes *display_attributes) = 0;


  /*! @brief Method to prepare and render buffer to display

    @param[in] disp_hnd - pointer to display handle which was created during CreateDisplay()
    @param[in] buf_handle - pointer to buffer handle which specifies the attributes of a buffer
    @param[out] retire_fence - pointer to retire fence which will be signaled once display hw
                               picks up the buf_handle for rendering.

    @return Returns 0 on sucess otherwise errno
  */
  virtual int ShowBuffer(Handle disp_hnd, BufferHandle *buf_handle, int32_t *retire_fence) = 0;


 protected:
  virtual ~SDMCompInterface() { }
};

}  // namespace sdm

#endif  // __SDM_COMP_INTERFACE_H__


