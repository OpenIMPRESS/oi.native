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

#include "OICore.hpp"
#include "UDPBase.hpp"

namespace oi { namespace core { namespace rgbd {

    class RGBDStreamer {
    public:
        RGBDStreamer(oi::core::network::UDPBase * oi_data);
        //virtual int HandleData(oi::core::network::DataContainer * dc) = 0;
        virtual int OpenDevice() = 0; // TODO: Pass config parameter
        virtual int CloseDevice() = 0;
    protected:
    private:
        
    };

} } }
