#include "OIWorker.hpp"
#include <type_traits>

namespace oi { namespace core { namespace worker {
    
    DataObject::DataObject()
    : buffer_size(BUFFER_SIZE)
    , buffer(new uint8_t[BUFFER_SIZE]) {
        data_end = 0;
        data_start = 0;
    }
    

} } }
