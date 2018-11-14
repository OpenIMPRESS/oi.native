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

#pragma once

#include "OIIO.hpp"
#include "OIWorker.hpp"

namespace oi { namespace core {
    
    // Some config parser?
    
    std::chrono::milliseconds NOW();
    std::chrono::microseconds NOWu();
    
    void debugMemory(unsigned char * loc, size_t len);
    
} }
