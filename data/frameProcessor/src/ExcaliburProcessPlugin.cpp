/*
 * ExcaliburProcessPlugin.cpp
 *
 *  Created on: 6 Jun 2016
 *      Author: gnx91527
 */

#include <ExcaliburProcessPlugin.h>
#include "version.h"
namespace FrameProcessor
{

  const std::string ExcaliburProcessPlugin::CONFIG_ASIC_COUNTER_DEPTH = "bitdepth";
  const std::string ExcaliburProcessPlugin::CONFIG_IMAGE_WIDTH = "width";
  const std::string ExcaliburProcessPlugin::CONFIG_IMAGE_HEIGHT = "height";
  const std::string ExcaliburProcessPlugin::BIT_DEPTH[4] = {"1-bit", "6-bit", "12-bit", "24-bit"};

  /**
   * The constructor sets up logging used within the class.
   */
  ExcaliburProcessPlugin::ExcaliburProcessPlugin() :
      asic_counter_depth_(DEPTH_12_BIT),
      image_width_(2048),
      image_height_(256),
      image_pixels_(image_width_ * image_height_),
      packets_lost_(0)
  {
    // Setup logging for the class
    logger_ = Logger::getLogger("FP.ExcaliburProcessPlugin");
    logger_->setLevel(Level::getAll());
    LOG4CXX_INFO(logger_, "ExcaliburProcessPlugin version " << this->get_version_long() << " loaded");
  }

  /**
   * Destructor.
   */
  ExcaliburProcessPlugin::~ExcaliburProcessPlugin()
  {
    LOG4CXX_TRACE(logger_, "ExcaliburProcessPlugin destructor.");
  }

  /**
   * Get the plugin major version number.
   * 
   * \return major version number as an integer
   */ 
  int ExcaliburProcessPlugin::get_version_major()
  {
    return ODIN_DATA_VERSION_MAJOR;
  }

  /**
   * Get the plugin minor version number.
   * 
   * \return minor version number as an integer
   */ 
  int ExcaliburProcessPlugin::get_version_minor()
  {
    return ODIN_DATA_VERSION_MINOR;
  }

  /**
   * Get the plugin patch version number.
   * 
   * \return patch version number as an integer
   */ 
  int ExcaliburProcessPlugin::get_version_patch()
  {
    return ODIN_DATA_VERSION_PATCH;
  }

  /**
   * Get the plugin short version (e.g. x.y.z) string.
   * 
   * \return short version as a string
   */ 
  std::string ExcaliburProcessPlugin::get_version_short()
  {
    return ODIN_DATA_VERSION_STR_SHORT;
  }

  /**
   * Get the plugin long version (e.g. x.y.z-qualifier) string.
   * 
   * \return long version as a string
   */ 
  std::string ExcaliburProcessPlugin::get_version_long()
  {
    return ODIN_DATA_VERSION_STR;
  }

  /**
   * Configure the Excalibur plugin.  This receives an IpcMessage which should be processed
   * to configure the plugin, and any response can be added to the reply IpcMessage.  This
   * plugin supports the following configuration parameters:
   * - bitdepth
   *
   * \param[in] config - Reference to the configuration IpcMessage object.
   * \param[out] reply - Reference to the reply IpcMessage object.
   */
  void ExcaliburProcessPlugin::configure(OdinData::IpcMessage& config, OdinData::IpcMessage& reply)
  {
    if (config.has_param(ExcaliburProcessPlugin::CONFIG_ASIC_COUNTER_DEPTH))
    {
      std::string bit_depth_str =
          config.get_param<std::string>(ExcaliburProcessPlugin::CONFIG_ASIC_COUNTER_DEPTH);

      if (bit_depth_str == BIT_DEPTH[DEPTH_1_BIT])
      {
        asic_counter_depth_ = DEPTH_1_BIT;
      }
      else if (bit_depth_str == BIT_DEPTH[DEPTH_6_BIT])
      {
        asic_counter_depth_ = DEPTH_6_BIT;
      }
      else if (bit_depth_str == BIT_DEPTH[DEPTH_12_BIT])
      {
        asic_counter_depth_ = DEPTH_12_BIT;
      }
      else if (bit_depth_str == BIT_DEPTH[DEPTH_24_BIT])
      {
        asic_counter_depth_ = DEPTH_24_BIT;
      }
      else
      {
        std::stringstream ss;
        ss << "Invalid bit depth requested: " << bit_depth_str;
        this->set_error(ss.str());
        LOG4CXX_ERROR(logger_, "Invalid bit depth requested: " << bit_depth_str);
        throw std::runtime_error("Invalid bit depth requested");
      }
    }

    if (config.has_param(ExcaliburProcessPlugin::CONFIG_IMAGE_WIDTH))
    {
      image_width_ = config.get_param<int>(ExcaliburProcessPlugin::CONFIG_IMAGE_WIDTH);
    }

    if (config.has_param(ExcaliburProcessPlugin::CONFIG_IMAGE_HEIGHT))
    {
      image_height_ = config.get_param<int>(ExcaliburProcessPlugin::CONFIG_IMAGE_HEIGHT);
    }

    image_pixels_ = image_width_ * image_height_;

  }

  void ExcaliburProcessPlugin::requestConfiguration(OdinData::IpcMessage& reply)
  {
    // Return the configuration of the process plugin
    std::string base_str = get_name() + "/";
    reply.set_param(base_str + ExcaliburProcessPlugin::CONFIG_ASIC_COUNTER_DEPTH, BIT_DEPTH[asic_counter_depth_]);
    reply.set_param(base_str + ExcaliburProcessPlugin::CONFIG_IMAGE_WIDTH, image_width_);
    reply.set_param(base_str + ExcaliburProcessPlugin::CONFIG_IMAGE_HEIGHT, image_height_);
  }

  /**
   * Collate status information for the plugin.  The status is added to the status IpcMessage object.
   *
   * \param[out] status - Reference to an IpcMessage value to store the status.
   */
  void ExcaliburProcessPlugin::status(OdinData::IpcMessage& status)
  {
    // Record the plugin's status items
    LOG4CXX_DEBUG(logger_, "Status requested for Excalibur plugin");
    status.set_param(get_name() + "/bitdepth", BIT_DEPTH[asic_counter_depth_]);
    status.set_param(get_name() + "/packets_lost", packets_lost_);
  }

  /**
   * Reset process plugin statistics, i.e. counter of packets lost
   */
  bool ExcaliburProcessPlugin::reset_statistics(void)
  {
    LOG4CXX_DEBUG(logger_, "Statistics reset requested for Excalibur plugin")
    
    // Reset packets lost counter
    packets_lost_ = 0;

    return true;
  }

  /**
   * Process and report lost UDP packets for the frame
   *
   * \param[in] frame - Pointer to a Frame object.
   */
  boost::shared_ptr<Frame> ExcaliburProcessPlugin::process_lost_packets(boost::shared_ptr<Frame> frame)
  {
    boost::shared_ptr<Frame> verified_frame = frame;
    const Excalibur::FrameHeader* hdr_ptr = static_cast<const Excalibur::FrameHeader*>(frame->get_data());
    Excalibur::AsicCounterBitDepth depth = static_cast<Excalibur::AsicCounterBitDepth>(asic_counter_depth_);
    LOG4CXX_DEBUG(logger_, "Processing lost packets for frame " << hdr_ptr->frame_number);
    LOG4CXX_DEBUG(logger_, "Packets received: " << hdr_ptr->total_packets_received
                                                << " out of a maximum "
                                                << Excalibur::num_fem_frame_packets(depth) * hdr_ptr->num_active_fems);
    if (hdr_ptr->total_packets_received < (Excalibur::num_fem_frame_packets(depth) * hdr_ptr->num_active_fems)){
      // We only need to perform this processing if there are any lost packets
      size_t nbytes = frame->get_data_size();
      void* verified_image = (void*)malloc(nbytes);
      memcpy(verified_image, frame->get_data(), nbytes);

      int packets_lost = (Excalibur::num_fem_frame_packets(depth) * hdr_ptr->num_active_fems) - hdr_ptr->total_packets_received;
      LOG4CXX_ERROR(logger_, "Frame number " << hdr_ptr->frame_number << " has dropped " << packets_lost << " packets");
      packets_lost_ += packets_lost;
      LOG4CXX_ERROR(logger_, "Total packets lost since startup " << packets_lost_);
      // Now loop over the packet state arrays to find out which packets are missing
      // Loop over the number of fems, the number of subframes and the number of packets
      int lost_packet_no = 0;
      for (int fem_no = 0; fem_no < (int)hdr_ptr->num_active_fems; fem_no++){
        for (int subframe = 0; subframe < Excalibur::num_subframes[depth]; subframe++){
          // Get a pointer to the new memory block
          char *packet_ptr = (char *)verified_image;
          // Increment by the buffer header size
          packet_ptr += sizeof(Excalibur::FrameHeader);
          // Increment by the fem and subframe size
          packet_ptr += (((fem_no * Excalibur::num_subframes[depth]) + subframe) * Excalibur::subframe_size(depth));
          // First loop over all of the primary packets
          int packet = 0;
          for (packet = 0; packet < Excalibur::num_primary_packets[depth]; packet++){
            if (hdr_ptr->fem_rx_state[fem_no].packet_state[subframe][packet] == 0){
              LOG4CXX_ERROR(logger_, "Missing packet number " << lost_packet_no);
              // Memset the packet to 0 currently
              memset(packet_ptr, 0, Excalibur::primary_packet_size);
            }
            // Increment by the number of the lost packet x packet size
            packet_ptr += Excalibur::primary_packet_size;
            lost_packet_no++;
          }
          // Now check the tail packet
          packet = Excalibur::num_primary_packets[depth];
          if (hdr_ptr->fem_rx_state[fem_no].packet_state[subframe][packet] == 0){
            LOG4CXX_ERROR(logger_, "Missing packet number " << lost_packet_no);
            // Memset the packet to 0 currently
            memset(packet_ptr, 0, Excalibur::tail_packet_size[depth]);
          }
          lost_packet_no++;
        }
      }
      verified_frame = boost::shared_ptr<Frame>(new Frame("verified"));
      verified_frame->copy_data(verified_image, nbytes);
      free(verified_image);
    }
    return verified_frame;
  }

  /**
   * Perform processing on the frame.  Depending on the selected bit depth
   * the corresponding pixel re-ordering algorithm is executed.
   *
   * \param[in] frame - Pointer to a Frame object.
   */
  void ExcaliburProcessPlugin::process_frame(boost::shared_ptr<Frame> frame)
  {
    LOG4CXX_TRACE(logger_, "Reordering frame.");
    LOG4CXX_TRACE(logger_, "Frame size: " << frame->get_data_size());

    frame = this->process_lost_packets(frame);

    const Excalibur::FrameHeader* hdr_ptr =
        static_cast<const Excalibur::FrameHeader*>(frame->get_data());

    LOG4CXX_TRACE(logger_, "Raw frame number: " << hdr_ptr->frame_number);
    LOG4CXX_TRACE(logger_, "Frame state: " << hdr_ptr->frame_state);
    LOG4CXX_TRACE(logger_, "Packets received: " << hdr_ptr->total_packets_received
        << " SOF markers: "<< (int)hdr_ptr->total_sof_marker_count
        << " EOF markers: "<< (int)hdr_ptr->total_eof_marker_count);

    // Loop over the active FEM list to determine the maximum active FEM index

    unsigned int max_active_fem_idx = 0;
    {
      std::stringstream msg;
      msg << "Number of active FEMs: " << static_cast<int>(hdr_ptr->num_active_fems) << " ids:";
      for (uint8_t idx = 0; idx < hdr_ptr->num_active_fems; idx++)
      {
        if (hdr_ptr->active_fem_idx[idx] > max_active_fem_idx)
        {
          max_active_fem_idx = hdr_ptr->active_fem_idx[idx];
        }
        msg << " " << static_cast<int>(hdr_ptr->active_fem_idx[idx]);
      }
      LOG4CXX_TRACE(logger_, msg.str());
    }

    // Determine the size of the output reordered image based on current bit depth
    const std::size_t output_image_size = reordered_image_size(asic_counter_depth_);
    LOG4CXX_TRACE(logger_, "Output image size: " << output_image_size);

    // Obtain a pointer to the start of the data in the frame
    const void* data_ptr = static_cast<const void*>(
        static_cast<const char*>(frame->get_data()) + sizeof(Excalibur::FrameHeader)
    );

    // Pointers to reordered image buffer - will be allocated on demand
    void* reordered_image = NULL;

    try
    {

      // Check that the pixels from all active FEMs are contained within the dimensions of the
      // specified output image, otherwise throw an error
      if (((max_active_fem_idx +1) * FEM_TOTAL_PIXELS) > image_pixels_)
      {
        std::stringstream msg;
        msg << "Pixel count inferred from active FEMs ("
            << ((max_active_fem_idx + 1) * FEM_TOTAL_PIXELS)
            << ", max FEM idx: " << max_active_fem_idx
            << ") will exceed dimensions of output image (" << image_pixels_ << ")";
        this->set_error(msg.str());
        throw std::runtime_error(msg.str());
      }

      // Allocate buffer to receive reordered image.
      reordered_image = (void*)malloc(output_image_size);
      if (reordered_image == NULL)
      {
        this->set_error("Failed to allocate temporary buffer for reordered image");
        throw std::runtime_error("Failed to allocate temporary buffer for reordered image");
      }

      // Calculate the FEM frame size once so it can be used in the following loop
      // repeatedly
      std::size_t fem_frame_size = (
          Excalibur::num_subframes[asic_counter_depth_] *
          Excalibur::subframe_size(static_cast<Excalibur::AsicCounterBitDepth>(asic_counter_depth_))
      );

      // Loop over active FEMs in the input frame image data, reordering pixels into the output
      // images

      for (uint8_t idx = 0; idx < hdr_ptr->num_active_fems; idx++)
      {
        uint8_t fem_idx = hdr_ptr->active_fem_idx[idx];

        // Calculate pointer into the input image data based on loop index
        void* input_ptr = static_cast<void *>(
            static_cast<char *>(const_cast<void *>(data_ptr)) + (fem_frame_size * idx)
        );

        // Calculate output image pixel offset based on active FEM index
        std::size_t output_offset = fem_idx * FEM_TOTAL_PIXELS;

        // Determine stripe orientation based on FEM index
        bool stripe_is_even = ((fem_idx & 1) == 1);
        LOG4CXX_TRACE(logger_, "Active FEM idx=" << static_cast<int>(fem_idx)
            << ": stripe orientation is " << (stripe_is_even ? "even" : "odd"));

        // Reorder strip according to counter depth
        switch (asic_counter_depth_)
        {
          case DEPTH_1_BIT: // 1-bit counter depth
            reorder_1bit_stripe(static_cast<unsigned int *>(input_ptr),
                                static_cast<unsigned char *>(reordered_image) + output_offset,
                                stripe_is_even);
            break;

          case DEPTH_6_BIT: // 6-bit counter depth
            reorder_6bit_stripe(static_cast<unsigned char *>(input_ptr),
                                static_cast<unsigned char *>(reordered_image) + output_offset,
                                stripe_is_even);
            break;

          case DEPTH_12_BIT: // 12-bit counter depth
            reorder_12bit_stripe(static_cast<unsigned short *>(input_ptr),
                                 static_cast<unsigned short *>(reordered_image) + output_offset,
                                 stripe_is_even);
            break;

          case DEPTH_24_BIT: // 24-bit counter depth needs special handling to merge two counters

            void* c1_input_ptr = input_ptr;
            void* c0_input_ptr =  static_cast<void *>(static_cast<char *>(input_ptr) + fem_frame_size / 2);

            reorder_24bit_stripe(
                static_cast<unsigned short *>(c0_input_ptr),
                static_cast<unsigned short *>(c1_input_ptr),
                static_cast<unsigned int *>(reordered_image) + output_offset,
                stripe_is_even);

            break;
        }
      }

      // Set the frame image to the reordered image buffer if appropriate
      if (reordered_image)
      {
        // Setup the frame dimensions
        dimensions_t dims(2);
        dims[0] = image_height_;
        dims[1] = image_width_;

        boost::shared_ptr<Frame> data_frame;
        data_frame = boost::shared_ptr<Frame>(new Frame("data"));

        if (asic_counter_depth_ == DEPTH_24_BIT)
        {
          // Only every other incoming frame results in a new frame
          data_frame->set_frame_number(hdr_ptr->frame_number/2);
        }
        else
        {
          data_frame->set_frame_number(hdr_ptr->frame_number);
        }
        // set frame data type based on the output image
        switch (asic_counter_depth_)
        {
          case DEPTH_1_BIT:
          case DEPTH_6_BIT:
            data_frame->set_data_type(raw_8bit);
            break;

          case DEPTH_12_BIT:
            data_frame->set_data_type(raw_16bit);
            break;

          case DEPTH_24_BIT:
            data_frame->set_data_type(raw_32bit);
            break;
        }
        data_frame->set_dimensions(dims);
        data_frame->copy_data(reordered_image, output_image_size);

        LOG4CXX_TRACE(logger_, "Pushing data frame.");
        this->push(data_frame);

        free(reordered_image);
        reordered_image = NULL;
      }
    }
    catch (const std::exception& e)
    {
      std::stringstream ss;
      ss << "EXCALIBUR frame decode failed: " << e.what();
      this->set_error(ss.str());
      LOG4CXX_ERROR(logger_, ss.str());
    }
  }

  /**
   * Determine the size of a reordered image size based on the counter depth.
   *
   * \param[in] asic_counter_depth
   * \return size of the reordered image in bytes
   */
  std::size_t ExcaliburProcessPlugin::reordered_image_size(int asic_counter_depth) {

    std::size_t slice_size = 0;

    switch (asic_counter_depth)
    {
      case DEPTH_1_BIT:
      case DEPTH_6_BIT:
        slice_size = image_width_ * image_height_ * sizeof(unsigned char);
        break;

      case DEPTH_12_BIT:
        slice_size = image_width_ * image_height_ * sizeof(unsigned short);
        break;

      case DEPTH_24_BIT:
        slice_size = image_width_ * image_height_ * sizeof(unsigned int);
        break;

      default:
      {
        std::stringstream msg;
        msg << "Invalid bit depth specified for reordered slice size: " << asic_counter_depth;
        this->set_error(msg.str());
        throw std::runtime_error(msg.str());
      }
      break;
    }

    return slice_size;

  }

  /**
   * Reorder an image stripe using 1 bit re-ordering.
   * 1 bit images are captured in raw data mode, i.e. without reordering. In this mode, each
   * 32-bit word contains the current pixel being output on each data line of the group of
   * 4 ASICs, i.e. a supercolumn
   *
   * \param[in] in - Pointer to the incoming image data.
   * \param[out] out - Pointer to the allocated memory where the reordered image is written.
   * \param[in] stripe_is_even - boolean indicating if stripe has even orientation
   */
  void ExcaliburProcessPlugin::reorder_1bit_stripe(unsigned int* in, unsigned char* out,
      bool stripe_is_even)
  {
    int block, y, x, x2, chip, pixel_x, pixel_y, pixel_addr, bit_posn;
    int raw_addr = 0;

    // Loop over two blocks of data
    for (block = 0; block < FEM_BLOCKS_PER_STRIPE_X; block++)
    {
      // Loop over Y axis (rows)
      for (y = 0; y < FEM_PIXELS_PER_CHIP_Y; y++)
      {
        pixel_y = stripe_is_even ? (255 - y) : y;

        // Loop over pixels in a supercolumn
        for (x = 0; x < FEM_PIXELS_PER_SUPERCOLUMN_X; x++)
        {
          // Loop over chips in x per block
          for (chip = 0; chip < FEM_CHIPS_PER_BLOCK_X; chip++)
          {
            // Loop over supercolumns per chip
            for (x2 = 0; x2 < FEM_SUPERCOLUMNS_PER_CHIP; x2++)
            {
              if (stripe_is_even)
              {
                pixel_x = (block*(FEM_PIXELS_PER_CHIP_X*FEM_CHIPS_PER_STRIPE_X/2)) +
                     (chip * FEM_PIXELS_PER_CHIP_X) +
                     (255 - ((x2 * FEM_PIXELS_PER_SUPERCOLUMN_X) + x));
              }
              else
              {
                pixel_x = (FEM_PIXELS_PER_CHIP_X*FEM_CHIPS_PER_STRIPE_X - 1) -
                    ((block*(FEM_PIXELS_PER_CHIP_X*FEM_CHIPS_PER_STRIPE_X/2)) +
                     (chip * FEM_PIXELS_PER_CHIP_X) +
                     (255 - ((x2 * FEM_PIXELS_PER_SUPERCOLUMN_X) + x)));
              }
              pixel_addr = pixel_x + pixel_y*(FEM_PIXELS_PER_CHIP_X*FEM_CHIPS_PER_STRIPE_X);
              bit_posn = (chip * 8) + x2;
              out[pixel_addr] = (in[raw_addr] >> bit_posn) & 0x1;
            }
          }
          raw_addr++;
        }
      }
    }
  }

  /**
   * Reorder an image stripe using 6 bit re-ordering.
   *
   * \param[in] in - Pointer to the incoming image data.
   * \param[out] out - Pointer to the allocated memory where the reordered image is written.
   * \param[in] stripe_is_even - boolean indicating if stripe has even orientation
   */
  void ExcaliburProcessPlugin::reorder_6bit_stripe(unsigned char* in, unsigned char* out,
      bool stripe_is_even)
  {
    int block, y, x, chip, x2, pixel_x, pixel_y, pixel_addr;
    int raw_addr = 0;

    for (block=0; block<FEM_BLOCKS_PER_STRIPE_X; block++)
    {
      for (y=0; y<FEM_PIXELS_PER_CHIP_Y; y+=2)
      {
        for (x=0; x<FEM_PIXELS_PER_CHIP_X/FEM_PIXELS_IN_GROUP_6BIT; x++)
        {
          for (chip=0; chip<FEM_CHIPS_PER_BLOCK_X; chip++)
          {
            for (x2=0; x2<FEM_PIXELS_IN_GROUP_6BIT; x2++)
            {
              if (stripe_is_even)
              {
                pixel_x = (block*(FEM_PIXELS_PER_CHIP_X*FEM_CHIPS_PER_STRIPE_X/2) +
                    chip*FEM_PIXELS_PER_CHIP_X + (255-(x2 + x*FEM_PIXELS_IN_GROUP_6BIT)));
             }
              else
              {
                pixel_x = (FEM_PIXELS_PER_CHIP_X*FEM_CHIPS_PER_STRIPE_X - 1) -
                    ((block*(FEM_PIXELS_PER_CHIP_X*FEM_CHIPS_PER_STRIPE_X/2) +
                     chip*FEM_PIXELS_PER_CHIP_X + (255-(x2 + x*FEM_PIXELS_IN_GROUP_6BIT))));
              }
              pixel_y = stripe_is_even ? (254 - y) : (y+1);
              pixel_addr = pixel_x + pixel_y*(FEM_PIXELS_PER_CHIP_X*FEM_CHIPS_PER_STRIPE_X);
              out[pixel_addr] = in[raw_addr];
              raw_addr++;

              pixel_y = stripe_is_even ? (255 - y) : y;
              pixel_addr = pixel_x + pixel_y*(FEM_PIXELS_PER_CHIP_X*FEM_CHIPS_PER_STRIPE_X);
              out[pixel_addr] = in[raw_addr];
              raw_addr++;
            }
          }
        }
      }
      // Skip over the subframe trailer in the last 8 bytes (4 words) at the end of each block
      raw_addr += 8;
    }
  }

  /**
   * Reorder an image stripe using 12 bit re-ordering.
   *
   * \param[in] in - Pointer to the incoming image data.
   * \param[out] out - Pointer to the allocated memory where the reordered image is written.
   * \param[in] stripe_is_even - boolean indicating if stripe has even orientation
   *
   */
  void ExcaliburProcessPlugin::reorder_12bit_stripe(unsigned short* in, unsigned short* out,
      bool stripe_is_even)
  {
    int block, y, x, chip, x2, pixel_x, pixel_y, pixel_addr;
    int raw_addr = 0;

    for (block=0; block<FEM_BLOCKS_PER_STRIPE_X; block++)
    {
      for (y=0; y<FEM_PIXELS_PER_CHIP_Y; y++)
      {
        pixel_y = stripe_is_even ? (255 - y) : y;

        for (x=0; x<FEM_PIXELS_PER_CHIP_X/FEM_PIXELS_IN_GROUP_12BIT; x++)
        {
          for (chip=0; chip<FEM_CHIPS_PER_BLOCK_X; chip++)
          {
            for (x2=0; x2<FEM_PIXELS_IN_GROUP_12BIT; x2++)
            {
              if (stripe_is_even)
              {
                pixel_x = (block*(FEM_PIXELS_PER_CHIP_X*FEM_CHIPS_PER_STRIPE_X/2) +
                       chip*FEM_PIXELS_PER_CHIP_X + (255-(x2 + x*FEM_PIXELS_IN_GROUP_12BIT)));
              }
              else
              {
                pixel_x = (FEM_PIXELS_PER_CHIP_X*FEM_CHIPS_PER_STRIPE_X - 1) -
                    (block*(FEM_PIXELS_PER_CHIP_X*FEM_CHIPS_PER_STRIPE_X/2) +
                     chip*FEM_PIXELS_PER_CHIP_X + (255-(x2 + x*FEM_PIXELS_IN_GROUP_12BIT)));
              }
              pixel_addr = pixel_x + pixel_y*(FEM_PIXELS_PER_CHIP_X*FEM_CHIPS_PER_STRIPE_X);
              out[pixel_addr] = in[raw_addr];
              raw_addr++;
            }
          }
        }
      }
      // Skip over the subframe trailer in the last 8 bytes (4 words) at the end of each block
      raw_addr += 4;
    }
  }

  /**
     * Reorder an image stripe using 24 bit re-ordering.
     *
     * This method uses the same reordering algorithm as for 12-bit images, but reorders
     * both counters in parallel and builds into the output image.
     *
     * \param[in] in_c0 - Pointer to the incoming counter 0 data.
     * \param[in] in_c1 - Pointer to the incoming counter 1 data.
     * \param[out] out - Pointer to the allocated memory where the reordered image is written.
     * \param[in] stripe_is_even - boolean indicating if stripe has even orientation
     */
    void ExcaliburProcessPlugin::reorder_24bit_stripe(unsigned short* in_c0,
        unsigned short* in_c1, unsigned int* out, bool stripe_is_even)
    {
      int block, y, x, chip, x2, pixel_x, pixel_y, pixel_addr;
      int raw_addr = 0;

      for (block=0; block<FEM_BLOCKS_PER_STRIPE_X; block++)
      {
        for (y=0; y<FEM_PIXELS_PER_CHIP_Y; y++)
        {
          pixel_y = stripe_is_even ? (255 - y) : y;

          for (x=0; x<FEM_PIXELS_PER_CHIP_X/FEM_PIXELS_IN_GROUP_12BIT; x++)
          {
            for (chip=0; chip<FEM_CHIPS_PER_BLOCK_X; chip++)
            {
              for (x2=0; x2<FEM_PIXELS_IN_GROUP_12BIT; x2++)
              {
                if (stripe_is_even)
                {
                  pixel_x = (block*(FEM_PIXELS_PER_CHIP_X*FEM_CHIPS_PER_STRIPE_X/2) +
                         chip*FEM_PIXELS_PER_CHIP_X + (255-(x2 + x*FEM_PIXELS_IN_GROUP_12BIT)));
                }
                else
                {
                  pixel_x = (FEM_PIXELS_PER_CHIP_X*FEM_CHIPS_PER_STRIPE_X - 1) -
                      (block*(FEM_PIXELS_PER_CHIP_X*FEM_CHIPS_PER_STRIPE_X/2) +
                       chip*FEM_PIXELS_PER_CHIP_X + (255-(x2 + x*FEM_PIXELS_IN_GROUP_12BIT)));
                }
                pixel_addr = pixel_x + pixel_y*(FEM_PIXELS_PER_CHIP_X*FEM_CHIPS_PER_STRIPE_X);
                out[pixel_addr] =
                    (((unsigned int)(in_c1[raw_addr] & 0xFFF)) << 12) | (in_c0[raw_addr] & 0xFFF);
                raw_addr++;
              }
            }
          }
        }
        // Skip over the subframe trailer in the last 8 bytes (4 words) at the end of each block
        raw_addr += 4;
      }
    }

} /* namespace FrameProcessor */

