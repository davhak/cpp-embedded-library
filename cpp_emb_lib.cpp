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

#include "misc.hpp"
#include "cpp_emb_lib.hpp"

namespace cel
{
    
    namespace buffer
    {
        /*----------------------------------------------------------------------------*/
        /**
        *  Allocates a buffer of requested size or returns nullptr if the space is not enough
        *  to satisfy the request.
        *
        */
        void* static_heap::alloc(heap_sz_t size)
        {
            reset();

            // to be on a safe side lets align to 4-byte boundary
            // instead of just making size even
            if (0 != (size & 3))
            {
                // make requested odd sizes even
                // ++size;

                size += 4 - (size & 3);
            }
            else
            {
                // do nothing
            }

            std::uint8_t *p = heap_start_;
            if (0u == size || (free_size_ < static_cast<std::uint32_t>(size + page_size_)) )
            {
                p = heap_end_;
            }
            else
            {
                while (p < heap_end_)
                {
                    page_t *page = reinterpret_cast<page_t*>(p);
                    if (page->free && (page->size >= size))
                    {
                        if ((page->size - size) <= page_size_)
                        {
                            // the remaining space is 0 or too small
                            // so do not split the available free page
                            // just mark it allocated
                            page->free = false;
                        }
                        else
                        {
                            page_t *page_next_new = reinterpret_cast<page_t*>(p + size + page_size_);
                            page_next_new->size = page->size - (size + page_size_);
                            page_next_new->free = true;
                            page_next_new->prev = page;

                            page_t *page_next = reinterpret_cast<page_t*>(p + page->size + page_size_);
                            if (reinterpret_cast<std::uint8_t*>(page_next) < heap_end_)
                            {
                                page_next->prev = page_next_new;
                            }
                            else
                            {
                                // do nothing
                            }

                            page->size = size;
                            page->free = false;

                            free_size_ -= size + page_size_;
                        }
                        break;
                    }
                    else
                    {
                        p += page->size + page_size_;
                    }
                }
            }

            if (p >= heap_end_)
            {
                p = nullptr;
            }
            else
            {
                p += page_size_;
            }

            return p;
        }

        /*----------------------------------------------------------------------------*/
        /**
        *  Frees the previously allocated buffer.
        *
        */
        void static_heap::free(void *p)
        {
            reset();

            if ((static_cast<std::uint8_t*>(p) > heap_start_) && (static_cast<std::uint8_t*>(p) < heap_end_))
            {
                page_t* page = reinterpret_cast<page_t*>(reinterpret_cast<std::uintptr_t>(p) - page_size_);
                defragment(page);
            }
            else
            {
                // do nothing
            }
        }

        /*----------------------------------------------------------------------------*/
        /**
        *  One-time initializer of the heap
        *
        */
        void static_heap::reset()
        {
            static bool b_once = true;
            if (b_once)
            {
                b_once = false;

                page_t *page = reinterpret_cast<page_t*>(heap_start_);
                page->size = heap_size_ - page_size_;
                page->free = true;
                page->prev = nullptr;
                free_size_ = heap_size_ - page_size_;
            }
            else
            {
                // do nothing
            }
        }

        /*----------------------------------------------------------------------------*/
        /**
        *  Defragments of free separated but continuous spaces into a larger free space.
        *  This function is called by static_free to release the buffer pointer by pg
        *  and to, possibly, defragment free spaces which may appear after releasing of the
        *  current buffer.
        *
        */
        void static_heap::defragment(page_t * const pg)
        {
            bool bfound = false;
            uint8_t *p = heap_start_;
            page_t *page = nullptr;

            // Find the last page as well as
            // check the validity of the requested page
            do
            {
                page = reinterpret_cast<page_t*>(p);
                p += page->size + page_size_;

                if ((pg == page) && (!pg->free))
                {
                    bfound = true;
                }
                else
                {
                    // do nothing
                }

            } while (p < heap_end_);

            if ( bfound )
            {
                // Requested page exist, mark it as free
                pg->free = true;

                if (free_size_ < heap_size_)
                {
                    free_size_ += pg->size + page_size_;
                }
                else
                {
                    // do nothing
                }

                // Last page found, go backward and defragment continuous free pages
                page_t *page_busy = nullptr;
                while (reinterpret_cast<std::uint8_t*>(page) > heap_start_)
                {
                    if ( !page->free )
                    {
                        page_busy = page;
                        page = page->prev;
                        continue;
                    }
                    else
                    {
                        // do nothing
                    }

                    if (page->prev->free)
                    {
                        page->prev->size += page->size + page_size_;

                        if (nullptr != page_busy)
                        {
                            page_busy->prev = page->prev;
                        }
                        else
                        {
                            // do nothing
                        }
                    }
                    else
                    {
                        // do nothing
                    }

                    page = page->prev;
                }
            }
            else
            {
                // do nothing
            }
        }

        /*----------------------------------------------------------------------------*/
        /**
        *  Checks if read or write operation can be successful
        *
        */
        bool ring_base::sanity_check(ring_info& info, bool b_read)
        {
            bool retval = false;
            if (nullptr != info.ptr_buff)
            {
                DISABLE_INTERRUPTS();
                span_t n = info.n;
                ENABLE_INTERRUPTS();

                if (b_read)
                {
                    if (n > 0u)
                    {
                        retval = true;
                    }
                    else
                    {
                        // do nothing
                    }
                }
                else
                {
                    if (info.size > n)
                    {
                        retval = true;
                    }
                    else
                    {
                        if (info.infinite)
                        {
                            // Discard the oldest record to always be able to add a new element
                            // Unconditionally unhide the oldest element if it happens to be hidden
                            ring_base::unhide_if_hidden(info);
                            retval = ring_base::pop(info, nullptr);
                        }
                        else
                        {
                            // do nothing
                        }
                    }
                }
            }
            else
            {
                // do nothing
            }

            return retval;
        }

        /*----------------------------------------------------------------------------*/
        /**
        *  Returns pointer to either start or end of the buffer fifo
        *
        */
        std::uint8_t* ring_base::ptr_to_end(const ring_info& info, ring_base::endpoint pnt)
        {
            const span_t featured_elem_size = info.elem_size + feature_size_;

            DISABLE_INTERRUPTS();
            std::uint8_t* ptr = endpoint::Head == pnt ? (info.ptr_buff + info.head * featured_elem_size) :
                                endpoint::Tail == pnt ? (info.ptr_buff + info.tail * featured_elem_size) :
                                nullptr;
            ENABLE_INTERRUPTS();

            ASSERT(nullptr != ptr);

            return ptr;
        }


        /*----------------------------------------------------------------------------*/
        /**
        *  Returns number of available elements in the ring buffer
        *
        */
        ring_base::span_t ring_base::get_count(const ring_info& info)
        {
            if (nullptr == info.ptr_buff)
            {
                return 0u;
            }
            else
            {
                // do nothing
            }

            DISABLE_INTERRUPTS();
            span_t n = info.n;
            ENABLE_INTERRUPTS();
            return n;
        }

        /*----------------------------------------------------------------------------*/
        /**
        *  Resets ring buffer
        *
        */
        void ring_base::reset(ring_info& info)
        {
            if (nullptr != info.ptr_buff)
            {
                DISABLE_INTERRUPTS();
                info.n = 0;
                info.head = 0;
                info.tail = 0;
                ENABLE_INTERRUPTS();
            }
            else
            {
            }
        }

        /*----------------------------------------------------------------------------*/
        /**
        *  Pushes a new element to the top of ring buffer. The element can be marked as
        *  hidden. Hidden elements cannot be read or removed from the ring buffer, before
        *  they are explicitely unhidden with function unhide_if_hidden
        *
        */
        bool ring_base::push (ring_info& info, const std::uint8_t* ptr_data, bool b_hidden)
        {
            bool retval = false;
            if ( sanity_check(info, false) )
            {
                if (nullptr != ptr_data)
                {
                    DISABLE_INTERRUPTS();
                    std::uint8_t* ptr = ptr_to_end(info, endpoint::Head);
                    feature_t* ptr_prop = ptr_elem_feature(info, ptr);

                    std::memcpy(ptr, ptr_data, info.elem_size);

                    ptr_prop->b_visited = false;
                    ptr_prop->b_hidden = b_hidden;
                    if (++(info.head) >= info.size)
                    {
                        info.head = 0u;
                    }
                    else
                    {
                        // do nothing
                    }

                    ++(info.n);
                    ENABLE_INTERRUPTS();

                    retval = true;
                }
                else
                {
                    // do nothing
                }
            }
            else
            {
                // do nothing
            }
            return retval;
        }

        /*----------------------------------------------------------------------------*/
        /**
        *  Removes and returns the oldest element from buffer.
        *
        */
        bool ring_base::pop (ring_info& info, std::uint8_t *ptr_data)
        {
            bool retval = false;
            if ( sanity_check(info) )
            {
                DISABLE_INTERRUPTS();
                std::uint8_t* ptr = ptr_to_end(info, endpoint::Tail);
                feature_t* ptr_prop = ptr_elem_feature(info, ptr);

                if ( !ptr_prop->b_hidden )
                {
                    if (nullptr != ptr_data)
                    {
                        std::memcpy(ptr_data, ptr, info.elem_size);
                    }
                    else
                    {
                        // just discard the record without returning its copy
                    }

                    ptr_prop->b_visited = false;
                    if (++(info.tail) >= info.size)
                    {
                        info.tail = 0u;
                    }
                    else
                    {
                        // do nothing
                    }

                    --(info.n);
                    retval = true;
                }
                else
                {
                    // do nothing
                }

                ENABLE_INTERRUPTS();
            }
            else
            {
                // do nothing
            }

            return retval;
        }

        /*----------------------------------------------------------------------------*/
        /**
        *  Returns the oldest element of ring buffer but does not remove it from the fifo.
        *  Only marks the element as 'visited'.
        *
        */
        bool ring_base::read_shadow (ring_info& info, std::uint8_t* ptr_data)
        {
            bool retval = false;
            if ( sanity_check(info) && nullptr != ptr_data)
            {
                DISABLE_INTERRUPTS();

                std::uint8_t* ptr = ptr_to_end(info, endpoint::Tail);
                feature_t* ptr_prop = ptr_elem_feature(info, ptr);

                if ( !ptr_prop->b_hidden )
                {
                    memcpy(ptr_data, ptr, info.elem_size);

                    ptr_prop->b_visited = true;

                    retval = true;
                }
                else
                {
                    // do nothing
                }

                ENABLE_INTERRUPTS();
            }
            else
            {
                // do nothing
            }

            return retval;
        }

        /*----------------------------------------------------------------------------*/
        /**
        *  Returns direct pointer to the oldest element of ring buffer without extra copying
        *  the element value to a user provided buffer.
        *  This is more efficient, read-only version of read_shadow function
        *
        */
        const std::uint8_t* ring_base::read_shadow_ptr (ring_info& info)
        {
            const std::uint8_t* ptr_retval = nullptr;

            if ( sanity_check(info))
            {
                DISABLE_INTERRUPTS();

                std::uint8_t* ptr = ptr_to_end(info, endpoint::Tail);
                feature_t* ptr_prop = ptr_elem_feature(info, ptr);

                if ( !ptr_prop->b_hidden )
                {
                    ptr_retval = ptr;
                    ptr_prop->b_visited = true;
                }
                else
                {
                    // do nothing
                }

                ENABLE_INTERRUPTS();
            }
            else
            {
                // do nothing
            }

            return ptr_retval;
        }

        /*----------------------------------------------------------------------------*/
        /**
        *  Removes the oldest element from the ring buffer if the former is marked as 
        *  'visited'
        *
        */
        bool ring_base::pop_if_visited (ring_info& info)
        {
            bool retval = false;
            if ( sanity_check(info) )
            {
                DISABLE_INTERRUPTS();

                std::uint8_t* ptr = ptr_to_end(info, endpoint::Tail);
                feature_t* ptr_prop = ptr_elem_feature(info, ptr);

                if (ptr_prop->b_visited)
                {
                    ptr_prop->b_visited = false;
                    if (++(info.tail) >= info.size)
                    {
                        info.tail = 0u;
                    }
                    else
                    {
                        // do nothing
                    }

                    --(info.n);

                    retval = true;
                }
                else
                {
                    // do nothing
                }

                ENABLE_INTERRUPTS();
            }
            else
            {
                // do nothing
            }

            return retval;
        }

        /*----------------------------------------------------------------------------*/
        /**
        *  Checks if the oldest element in the ring buffer is marked as 'visited'
        *
        */
        bool ring_base::is_node_visited (ring_info& info)
        {
            bool retval = false;
            if ( sanity_check(info) )
            {
                DISABLE_INTERRUPTS();

                std::uint8_t* ptr = ptr_to_end(info, endpoint::Tail);
                feature_t* ptr_prop = ptr_elem_feature(info, ptr);

                retval = ptr_prop->b_visited;

                ENABLE_INTERRUPTS();
            }
            else
            {
                // do nothing
            }

            return retval;
        }

        /*----------------------------------------------------------------------------*/
        /**
        *  Unhides the element if it is marked as 'hidden'
        *
        */
        bool ring_base::unhide_if_hidden (ring_info& info)
        {
            bool retval = false;
            if ( sanity_check(info) )
            {

                DISABLE_INTERRUPTS();

                std::uint8_t* ptr = ptr_to_end(info, endpoint::Tail);
                feature_t* ptr_prop = ptr_elem_feature(info, ptr);

                if (ptr_prop->b_hidden)
                {
                    ptr_prop->b_hidden = false;
                    retval = true;
                }
                else
                {
                    // do nothing
                }
                ENABLE_INTERRUPTS();
            }
            else
            {
                // do nothing
            }

            return retval;
        }
    }

}