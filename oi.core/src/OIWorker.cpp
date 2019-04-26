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

#include "OIWorker.hpp"
#include <type_traits>

namespace oi { namespace core { namespace worker {
    
    DataObject::DataObject(size_t buffer_size, ObjectPool<DataObject> * _pool)
    : buffer_size(buffer_size)
    , buffer(new uint8_t[buffer_size]) {
        reset();
        this->_return_to_pool = _pool;
    }

	int DataObject::setData(std::string data) {
		memcpy(&(this->buffer[0]), data.c_str(), data.length());
		this->data_start = 0;
		this->data_end = data.length();
		return data.length();
	}

    int DataObject::setData(const void * data, size_t len) {
        memcpy(&(this->buffer[0]), data, len);
        this->data_start = 0;
        this->data_end = len;
        return len;
    }
    
    DataObject::~DataObject() {
    };
    
    void DataObject::reset() {
        data_end = 0;
        data_start = 0;
    }
    
    
    

} } }
