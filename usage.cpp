/*
 * Copyright 2023 Davit Hakobyan
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cpp_emb_lib.hpp"

namespace cel
{
    // The following structure is just for testing the library usage
    // See 'usage' function below for more info
    struct cmd_t
    {
        enum class id_t : std::uint8_t
        {
            ID_1 = 1,
            ID_2 = 2,
            ID_3 = 3,
        };

        bool m_bool;
        std::uint32_t m_u32;
        float m_float;
        id_t m_id;
        char m_arr[10];
        std::array<char, 20> m_arr2;
    };

    // The following function is provided to string parser to convert
    // string to custom type such as cmd_t::id_t
    bool str_to_id(const char* ptr_str, data::len_t len, cmd_t::id_t& id)
    {
        char* end_c = nullptr;
        id = static_cast<cmd_t::id_t>( std::strtoul(ptr_str, &end_c, 10) );
        return true;
    }

    // The following function is provided to string parser to convert
    // string to custom type such as std::array
    bool str_to_std_array(const char* ptr_str, data::len_t len, std::array<char, 20>& arr)
    {
        data::len_t min_len = arr.size() < (len + 1u) ? (arr.size() - 1u) : len;
        std::memcpy(arr.data(), ptr_str, min_len);
        arr[min_len] = '\0';
        return true;
    }

    /*----------------------------------------------------------------------------*/
    /**
    *  Demontrates usage of C++ embedded library components.
    *
    */
    void usage()
    {
        // [[[[   Case 1   ]]]]
        // Create a 10 element array of type std::uint16_t
        // The array is automatically created in the static heap

        auto sz = 10;
        auto ptr_buff = buffer::manual_heap::alloc<std::uint16_t>(sz);

        // Assign Fibonacci numbers to array elements
        auto j = 0u;
        for (auto i = 0; i < sz; ++i)
        {
            ptr_buff[i] = j;
            j += i < 1u ? 1u : ptr_buff[i-1];
        }

        // free allocated buffer
        // not calling free will result in memory leak
        buffer::manual_heap::free( ptr_buff );

        // [[[[   Case 2   ]]]]
        // Create a ring buffer to hold instances of cmd_t
        buffer::ring_maker<cmd_t> cmd_ring(2);

        if ( !cmd_ring.is_good() )
        {
            // Failed to create ring buffer
            return;
        }

        {
            // [[[[   Case 3   ]]]]
            // The above example with allocation of buffer using manual_heap can be
            // done using the auto_heap. The difference from manual_heap is in automatic
            // releasing of allocated buffer upon leaving the scope where the allocation took place
            buffer::auto_heap<cmd_t> ptr_cmd(2);

            // [[[[   Case 4   ]]]]
            // Parse input string and extract needed information accoding to defined string keys
            
            // Define a string to parse for keys
            char str_tmp[] = "some garbage data,motors_id_present:1,speed:120,garbage data in the middle,param:3.14,sensor_id:3,string:Hello World!,trailing garbage data";
            char str_tmp2[] = "some garbage data$abc$motors_id_present:0$abc$speed:40$abc$garbage data in the middle$abc$param:1.27$abc$sensor_id:2$abc$string:Hello 2!$abc$std::array!$abc$trailing garbage data";

            // Provide key information to the parser. The delimiter is comma
            data::str_parser sp { ',', nullptr,

                            data::str_param(ptr_cmd[0].m_bool, "motors_id_present:"),
                            data::str_param(ptr_cmd[0].m_u32, "speed:"),
                            data::str_param(ptr_cmd[0].m_float, "param:"),
                            data::str_param(ptr_cmd[0].m_id, str_to_id, "sensor_id:"),
                            data::str_param(ptr_cmd[0].m_arr, "string:"),

                                    };

            // Provide key information to the parser. The delimiter is a string
            // Instead of the sp initialization above where convertion to C-like array (m_arr)
            // is requested, here we convert to std::array (m_arr2) using a custom conversion
            // function str_to_std_array
            data::str_parser sp2 { "$abc$", nullptr,

                            data::str_param(ptr_cmd[1].m_bool, "motors_id_present:"),
                            data::str_param(ptr_cmd[1].m_u32, "speed:"),
                            data::str_param(ptr_cmd[1].m_float, "param:"),
                            data::str_param(ptr_cmd[1].m_id, str_to_id, "sensor_id:"),
                            data::str_param(ptr_cmd[1].m_arr2, str_to_std_array, "std::"),

                                    };

            if ( sp.parse( str_tmp ) && sp2.parse( str_tmp2 ) )
            {
                // The parse function returns true if at least one key was found 
                // If parsing is successful, the cmd_t objects allocated in static heap should hold the values
                // for those parameters which were provided
                (void)cmd_ring.push( ptr_cmd[0] );
                
                // The second element is marked as hidden
                (void)cmd_ring.push( ptr_cmd[1], true );
            }
            else
            {
                // something went wrong
            }

            // leaving the scope will release the cmd_t resources allocated by buffer::auto_heap
        }

        cmd_t cmd;
        if ( cmd_ring.get_count() > 0u )
        {
            // pop the oldest record and store it into cmd
            (void)cmd_ring.pop(cmd);
        }
        if ( cmd_ring.get_count() > 0u )
        {
            // Get a read-only poiner to the oldest element in buffer
            const cmd_t * ptr_elem = cmd_ring.read_shadow_ptr();

            if (nullptr == ptr_elem)
            {
                // the second oldest element in FIFO is hidden, we need to unhide it first
                (void)cmd_ring.unhide_if_hidden();

                // Check if the element been ever read
                if ( !cmd_ring.is_node_visited() )
                {
                    // The element has not yet been read before

                    // Let's get a read-only poiner to element again, this time it should 
                    // not be nullptr as we already unhidden it
                    ptr_elem = cmd_ring.read_shadow_ptr();
                }
            }

            // Discard the oldest elemenet from FIFO (note calling pop with no argument)
            (void)cmd_ring.pop();

        }

        // We can reset the FIFO to start from clean buffer
        cmd_ring.reset();
    }
}
