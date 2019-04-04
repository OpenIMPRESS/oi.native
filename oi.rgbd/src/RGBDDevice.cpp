/*
This file is part of the OpenIMPRESS project.

OpenIMPRESS is free software: you can redistribute it and/or modify
it under the terms of the Lesser GNU Lesser General Public License as published
by the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

OpenIMPRESS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with OpenIMPRESS. If not, see <https://www.gnu.org/licenses/>.
*/


#include <string>
#include <sstream>
#include <algorithm>
#include <iterator>
#include "RGBDDevice.hpp"

using namespace oi::core::rgbd;
using namespace oi::core::worker;
using namespace oi::core::network;

RGBDDevice::RGBDDevice(RGBDDeviceInterface& device, RGBDStreamIO& io) {
    this->_device = &device;
	this->_io = &io;
    
    _stream_config.header.packageFamily = OI_LEGACY_MSG_FAMILY_RGBD;
    _stream_config.header.packageType = OI_MSG_TYPE_RGBD_CONFIG;
    _stream_config.header.partsTotal = 1;
    _stream_config.header.currentPart = 1;
    

    _stream_config.frameWidth = (uint16_t) device.frame_width();
    _stream_config.frameHeight = (uint16_t) device.frame_height();
    _stream_config.maxLines = (uint16_t) 1;
    _stream_config.Cx = device.device_cx();
    _stream_config.Cy = device.device_cy();
    _stream_config.Fx = device.device_fx();
    _stream_config.Fy = device.device_fy();
    _stream_config.DepthScale = device.device_depth_scale();
    
    _stream_config.dataFlags = 0;
    
    if (_device->supports_hd()) {
        _stream_config.dataFlags |= HD_DATA;
    }
    
    if (_device->supports_bidx()) {
        _stream_config.dataFlags |= BIDX_DATA;
    }
    
    if (_device->supports_audio()) {
        _stream_config.dataFlags |= AUDIO_DATA;
    }
    
    if (_device->supports_audio()) {
        _stream_config.dataFlags |= BODY_DATA;
    }
    
    _stream_config.dataFlags |= RGBD_DATA;
    _stream_config.dataFlags |= LIVE_DATA;
    
    std::string guid = device.device_guid();
    memcpy(&(_stream_config.guid[0]), guid.c_str(), guid.length()+1);
    
    handleStreamThread = new std::thread(&RGBDDevice::HandleStream, this);
}

int RGBDDevice::HandleStream() {
    //worker::ObjectPool<UDPMessageObject> bufferPool(32 , 4096);
    //worker::WorkerQueue<UDPMessageObject> cmdqueue(&bufferPool);
    //_udpc->RegisterQueue(OI_LEGACY_MSG_FAMILY_RGBD_CMD, &cmdqueue, Q_IO_IN);
    while (true) {
		fps_counter = 0;
        std::this_thread::sleep_for(_config_send_interval);
        printf("SENT CONFIG: %d bytes. FPS: %d\n", SendConfig(), fps_counter/(int)(_config_send_interval.count() / 1000));
        //DataObjectAcquisition<UDPMessageObject> data_in(&cmdqueue, W_TYPE_QUEUED, W_FLOW_NONBLOCKING);
        //data_in.release();
    }
    return 1;
}

int RGBDDevice::SendConfig() {
    _stream_config.header.sequence = _io->next_sequence_id();
    _stream_config.header.timestamp = NOW().count();
    
    DataObjectAcquisition<UDPMessageObject> data_out(_io->empty_frame(), W_FLOW_BLOCKING);
    if (!data_out.data) {
        std::cout << "\nERROR: No free buffers available" << std::endl;
        return -1;
    }
    
    int data_len = sizeof(CONFIG_STRUCT);
    memcpy(&(data_out.data->buffer[0]), (unsigned char *) &_stream_config, data_len);
    data_out.data->data_end = data_len;
    data_out.enqueue(_io->live_frame_queue());
    return data_len;
}

int RGBDDevice::QueueAudioFrame(uint32_t sequence, float * samples, size_t n_samples, uint16_t freq, uint16_t channels, std::chrono::milliseconds timestamp) {
    _audio_samples_counter += n_samples;
    
    DataObjectAcquisition<UDPMessageObject> data_out(_io->empty_frame(), W_FLOW_BLOCKING);
    if (!data_out.data) {
        std::cout << "\nERROR: No free buffers available" << std::endl;
        return -1;
    }
    
    data_out.data->data_start = 0;
    AUDIO_HEADER_STRUCT * audio_header = (AUDIO_HEADER_STRUCT *) &(data_out.data->buffer[0]);
    size_t header_size = sizeof(AUDIO_HEADER_STRUCT);
    
    audio_header->header.packageFamily = OI_LEGACY_MSG_FAMILY_AUDIO;
    audio_header->header.packageType = OI_MSG_TYPE_AUDIO_DEFAULT_FRAME;
    audio_header->header.partsTotal = 1;
    audio_header->header.currentPart = 1;
    audio_header->header.sequence = _io->next_sequence_id();
    audio_header->header.timestamp = timestamp.count();
    
    audio_header->channels = channels;
    audio_header->frequency = freq;
    audio_header->samples = n_samples;
    
    size_t audio_block_size = sizeof(float) * n_samples;
    uint8_t * data = &(data_out.data->buffer[header_size]);
    size_t writeOffset = 0;
    
    memcpy(data, samples, audio_block_size);
    writeOffset += audio_block_size;
    
    /*
    for (int i = 0; i < n_samples; i++) {
        unsigned short sample = (unsigned short) (samples[i] * 32767);
        memcpy(&(dc->dataBuffer[writeOffset]), &sample, sizeof(sample));
        writeOffset += sizeof(sample);
    }
    */
    
    size_t data_len = header_size + writeOffset;
    data_out.data->data_end = data_len;
    data_out.enqueue(_io->live_frame_queue());
    return data_len;
}

int RGBDDevice::QueueBodyFrame(oi::core::BODY_STRUCT * bodies, uint16_t n_bodies, std::chrono::milliseconds timestamp) {
	int res = 0;
	DataObjectAcquisition<UDPMessageObject> data_out(_io->empty_frame(), W_FLOW_BLOCKING);
	if (!data_out.data) {
		std::cout << "\nERROR: No free buffers available" << std::endl;
		return -1;
	}
	data_out.data->data_start = 0;
	BODY_HEADER_STRUCT * body_header = (BODY_HEADER_STRUCT *) &(data_out.data->buffer[0]);
	static size_t header_size = sizeof(BODY_HEADER_STRUCT);
	body_header->header.timestamp = timestamp.count();
	body_header->header.packageFamily = OI_LEGACY_MSG_FAMILY_MOCAP;
	body_header->header.packageType = OI_MSG_TYPE_MOCAP_BODY_FRAME_KINECTV2;
	body_header->header.partsTotal = 1;
	body_header->header.currentPart = 1;
	body_header->header.sequence = _io->next_sequence_id();
	body_header->n_bodies = n_bodies;
	uint8_t * data = &(data_out.data->buffer[header_size]);
	size_t data_size = sizeof(BODY_STRUCT) * n_bodies;
	memcpy(data, bodies, data_size);

	int d_data_len = header_size + data_size;
	data_out.data->data_end = d_data_len;
	res += d_data_len;
	data_out.enqueue(_io->live_frame_queue());
	return res;
}

int RGBDDevice::QueueRGBDFrame(uint64_t sequence, uint8_t * rgbdata, uint8_t * depthdata, std::chrono::milliseconds timestamp) {
    return QueueRGBDFrame(sequence, rgbdata, depthdata, NULL, timestamp);
}

int RGBDDevice::QueueRGBDFrame(uint64_t sequence, uint8_t * rgbdata, uint16_t * depthdata, std::chrono::milliseconds timestamp) {
    return QueueRGBDFrame(sequence, rgbdata, NULL, depthdata, timestamp);
}

int RGBDDevice::QueueRGBDFrame(uint64_t sequence, uint8_t * rgbdata, uint8_t * depth_any, uint16_t * depth_ushort, std::chrono::milliseconds timestamp) {
    int res = 0;
    std::chrono::milliseconds delta = timestamp - _prev_frame;
    _prev_frame = timestamp;
    
    unsigned short deltaValue = (unsigned short) delta.count();
    if (delta.count() >= 60000) deltaValue = 0; // Just to be sure that we don't overflow...
    
    
    int frame_width =_device->frame_width();
    int frame_height =_device->frame_height();
    
    { // Scope buffer access
        DataObjectAcquisition<UDPMessageObject> data_out(_io->empty_frame(), W_FLOW_BLOCKING);
        if (!data_out.data) {
            std::cout << "\nERROR: No free buffers available" << std::endl;
            return -1;
        }
        data_out.data->data_start = 0;
        RGBD_HEADER_STRUCT * rgbd_header = (RGBD_HEADER_STRUCT *) &(data_out.data->buffer[0]);
        static size_t header_size = sizeof(RGBD_HEADER_STRUCT);
    
        rgbd_header->header.timestamp = timestamp.count();
        rgbd_header->delta_t = deltaValue;
    
        // Send color data first...
        rgbd_header->header.packageFamily = OI_LEGACY_MSG_FAMILY_RGBD;
        rgbd_header->header.packageType = OI_MSG_TYPE_RGBD_COLOR;
        rgbd_header->header.partsTotal = 1;
        rgbd_header->header.currentPart = 1;
        rgbd_header->header.sequence = _io->next_sequence_id();
        rgbd_header->header.timestamp = timestamp.count();
        rgbd_header->startRow = (uint16_t) 0;             // ... we can fit the whole...
        rgbd_header->endRow = (uint16_t) frame_height; //...RGB data in one packet
    
        uint8_t * data = &(data_out.data->buffer[header_size]);
    
        // COMPRESS COLOR
        long unsigned int _jpegSize = MAX_UDP_PACKET_SIZE - header_size;
        unsigned char* _compressedImage = (unsigned char*) data;
    
        tjhandle _jpegCompressor = tjInitCompress();
        tjCompress2(_jpegCompressor, rgbdata, frame_width, 0, frame_height, _device->color_pixel_format(),
                    &_compressedImage, &_jpegSize, TJSAMP_444, JPEG_QUALITY,
                    TJFLAG_FASTDCT);
        int c_data_len = header_size + _jpegSize;
        data_out.data->data_end = c_data_len;
        res += c_data_len;
        data_out.enqueue(_io->live_frame_queue());
        tjDestroy(_jpegCompressor);
    }
    
    
    uint16_t linesPerMessage = (uint16_t)(MAX_UDP_PACKET_SIZE - sizeof(RGBD_HEADER_STRUCT)) / (2 * frame_width);
    for (uint16_t startRow = 0; startRow < frame_height; startRow += linesPerMessage) {
        uint16_t endRow = startRow + linesPerMessage;
        if (startRow >= endRow) break;
        if (endRow >= frame_height) endRow = frame_height;
        DataObjectAcquisition<UDPMessageObject> data_out(_io->empty_frame(), W_FLOW_BLOCKING);
        if (!data_out.data) {
            std::cout << "\nERROR: No free buffers available" << std::endl;
            return -1;
        }
        data_out.data->data_start = 0;
        RGBD_HEADER_STRUCT * rgbd_header = (RGBD_HEADER_STRUCT *) &(data_out.data->buffer[0]);
        static size_t header_size = sizeof(RGBD_HEADER_STRUCT);
        rgbd_header->header.timestamp = timestamp.count();
        rgbd_header->delta_t = deltaValue;
        rgbd_header->header.packageFamily = OI_LEGACY_MSG_FAMILY_RGBD;
        rgbd_header->header.packageType = OI_MSG_TYPE_RGBD_DEPTH_BLOCK;
        rgbd_header->header.partsTotal = 1;
        rgbd_header->header.currentPart = 1;
        rgbd_header->header.sequence = _io->next_sequence_id();
        rgbd_header->header.timestamp = timestamp.count();
        rgbd_header->startRow = startRow;             // ... we can fit the whole...
        rgbd_header->endRow = endRow; //...RGB data in one packet
        
        size_t writeOffset = header_size;
        
        if (depth_any != NULL) {
            size_t depthLineSizeR = frame_width * _device->raw_depth_stride();
            size_t depthLineSizeW = frame_width * 2;
            size_t readOffset = startRow*depthLineSizeR;
            for (int line = startRow; line < endRow; line++) {
                for (int i = 0; i < frame_width; i++) {
                    float depthValue = 0;
                    memcpy(&depthValue, &depth_any[readOffset + i * 4], sizeof(depthValue));
                    unsigned short depthValueShort = (unsigned short)(depthValue);
                    memcpy(&(data_out.data->buffer[writeOffset + i * 2]), &depthValueShort, sizeof(depthValueShort));
                }
                writeOffset += depthLineSizeW;
                readOffset += depthLineSizeR;
            }
        } else if (depth_ushort != NULL) {
            size_t startRowStart = startRow * frame_width;
            // for pixel (in all lines), two bytes:
            size_t bytesToCopy = (endRow - startRow) * frame_width * sizeof(depth_ushort[0]);
            memcpy(&(data_out.data->buffer[writeOffset]), &(depth_ushort[startRowStart]), bytesToCopy);
            writeOffset += bytesToCopy;
        }
        
        int d_data_len = writeOffset;
        data_out.data->data_end = d_data_len;
        res += d_data_len;
        data_out.enqueue(_io->live_frame_queue());
    }
    
    fps_counter++;
    return res;
}




int RGBDDevice::QueueHDFrame(unsigned char * rgbdata, int width, int height, TJPF pix_fmt, std::chrono::milliseconds timestamp) {
    return -1;
}

int RGBDDevice::QueueBodyIndexFrame(unsigned char * bidata, int width, int height, TJPF pix_fmt, std::chrono::milliseconds timestamp) {
	std::chrono::milliseconds delta = timestamp - _prev_body_frame;
	_prev_body_frame = timestamp;
	unsigned short deltaValue = (unsigned short)delta.count();
	int res = 0;

	DataObjectAcquisition<UDPMessageObject> data_out(_io->empty_frame(), W_FLOW_BLOCKING);
	if (!data_out.data) {
		std::cout << "\nERROR: No free buffers available" << std::endl;
		return -1;
	}
	data_out.data->data_start = 0;
	RGBD_HEADER_STRUCT * rgbd_header = (RGBD_HEADER_STRUCT *) &(data_out.data->buffer[0]);
	static size_t header_size = sizeof(RGBD_HEADER_STRUCT);

	rgbd_header->header.packageFamily = OI_LEGACY_MSG_FAMILY_RGBD;
	rgbd_header->header.packageType = OI_MSG_TYPE_RGBD_BODY_ID_TEXTURE_BLOCK; // _BLOCK?, even though its a whole one?
	rgbd_header->header.partsTotal = 1;
	rgbd_header->header.currentPart = 1;
	rgbd_header->header.sequence = _io->next_sequence_id();
	rgbd_header->header.timestamp = timestamp.count();
	rgbd_header->delta_t = deltaValue;
	rgbd_header->startRow = 0;
	rgbd_header->endRow = _device->frame_height();
	uint8_t * data = &(data_out.data->buffer[header_size]);

	// COMPRESS COLOR
	long unsigned int _jpegSize = MAX_UDP_PACKET_SIZE - header_size;
	unsigned char* _compressedImage = (unsigned char*)data;

	tjhandle _jpegCompressor = tjInitCompress();
	tjCompress2(_jpegCompressor, bidata, width, 0, height, pix_fmt,
		&_compressedImage, &_jpegSize, TJSAMP_GRAY, 75,
		TJFLAG_FASTDCT);
	int c_data_len = header_size + _jpegSize;
	data_out.data->data_end = c_data_len;
	res += c_data_len;
	data_out.enqueue(_io->live_frame_queue());
	tjDestroy(_jpegCompressor);

	return res;
}

