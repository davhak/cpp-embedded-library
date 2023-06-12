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

#ifndef CPP_EMB_LIB_HPP_INCLUDED
#define CPP_EMB_LIB_HPP_INCLUDED

#include <cstdint>
#include <array>
#include <cstring>
#include <utility>
#include <cstdlib>
#include <type_traits>
#include <tuple>

// ===================================================================
// Defines
// ===================================================================
#define CEL_STATIC_HEAP_SIZE    (4096u)


namespace cel
{
    namespace buffer
    {
        // ===================================================================
        // Defines, Typedefs
        // ===================================================================
        using heap_sz_t = std::uint16_t;


        // ===================================================================
        // Class for static heap allocation/deallocation
        // ===================================================================
        class static_heap
        {
        public:

            static_heap(const static_heap&)              = delete;
            static_heap(static_heap&&)                   = delete;

            static_heap& operator = (const static_heap&) = delete;
            static_heap& operator = (static_heap&&)      = delete;

            static void* alloc(heap_sz_t size);
            static void free(void *p);

            static std::uint32_t free_size()
            {
                return free_size_;
            }

        protected:

            static_heap()
            {}

            struct page_t
            {
                heap_sz_t size;
                bool free;
                page_t *prev;
            };

        private:

            static void reset();
            static void defragment(page_t * const pg);

            static constexpr std::uint32_t heap_size_ = CEL_STATIC_HEAP_SIZE;
            static constexpr std::uint8_t  page_size_ = sizeof(page_t);

            static inline    std::uint32_t free_size_ = heap_size_ - page_size_;

            alignas(4) static inline std::uint8_t heap_[heap_size_];
            static constexpr std::uint8_t* const heap_start_ = &heap_[0];

            static constexpr std::uint8_t* const heap_end_ = heap_start_ + heap_size_;
        };

        // ===================================================================
        // Helper class for heap allocation of given type
        // ===================================================================
        class manual_heap : public static_heap
        {
        public:
            template <typename T>
            static T* alloc(heap_sz_t size)
            {
                return static_cast<T*>(static_heap::alloc( sizeof(T) * size ));
            }
        
        private:
            
            manual_heap()
            {}
        };

        // ===================================================================
        // Helper class for convenience of heap allocation and
        // automatic heap deallocation on exit of the scope
        // ===================================================================
        template <typename T>
        class auto_heap : private static_heap
        {
        public:
            auto_heap(const auto_heap&)              = delete;
            auto_heap(auto_heap&&)                   = delete;

            auto_heap& operator = (const auto_heap&) = delete;
            auto_heap& operator = (auto_heap&&)      = delete;

            explicit auto_heap(heap_sz_t n = 1u)
            : ptr_ ( static_cast<T*>(static_heap::alloc( (n * sizeof(T)) )) )
            {
            }

            ~auto_heap()
            {
                static_heap::free(ptr_);
            }

            T* operator &() const
            {
                return ptr_;
            }

            T* operator ->() const
            {
                return ptr_;
            }

            operator T* () const
            {
                return ptr_;
            }

            T& operator [](int i)
            {
                return *(ptr_ + i);
            }

        private:
            T* const ptr_;
        };

        // ===================================================================
        // Ring buffer base class
        // ===================================================================
        class ring_base
        {
        public:
            using span_t = std::uint16_t;

            struct ring_info
            {
                ring_info(std::uint8_t* ptr, span_t sz, span_t elem_size, bool infinite = false)
                                                        : ptr_buff(ptr),
                                                        size(sz),
                                                        elem_size(elem_size),
                                                        infinite(infinite)
                {
                    head = 0u;
                    tail = 0u;
                    n = 0u;
                }

                std::uint8_t * const ptr_buff;
                span_t head;
                span_t tail;
                span_t n;
                const span_t size;
                const span_t elem_size;
                const bool infinite;
            };

            struct feature_t
            {
                bool b_visited;
                bool b_hidden;
            };

            ring_base(const ring_base&)              = delete;
            ring_base(ring_base&&)                   = delete;

            ring_base& operator = (const ring_base&) = delete;
            ring_base& operator = (ring_base&&)      = delete;

            static span_t    			get_count        (const ring_info& info);

            static void      			reset            (ring_info& info);

            static bool      			push             (ring_info& info, const std::uint8_t* ptr_data, bool b_hidden = false);

            static bool      			pop              (ring_info& info, std::uint8_t* ptr_data);

            static bool      			read_shadow      (ring_info& info, std::uint8_t* ptr_data);

            static const std::uint8_t*  read_shadow_ptr  (ring_info& info);

            static bool      			pop_if_visited   (ring_info& info);

            static bool      			is_node_visited  (ring_info& info);

            static bool      			unhide_if_hidden (ring_info& info);

        protected:

            explicit ring_base()
            {
            }

        private:

            enum class endpoint : std::uint8_t {Head, Tail};

            static constexpr std::uint8_t feature_size_ = sizeof(feature_t);

            static bool sanity_check(ring_info& info, bool b_read = true);

            static std::uint8_t* ptr_to_end(const ring_info& info, endpoint pnt);

            static feature_t* ptr_elem_feature(const ring_info& info, std::uint8_t* ptr_elem)
            {
                return reinterpret_cast<feature_t*>(ptr_elem + info.elem_size);
            }

        };

        // ===================================================================
        // Ring buffer allocator class. By default uses manual_heap class to 
        // allocate buffer from static heap organized by class static_heap
        // ===================================================================
        template <typename T>
        class ring_heap_allocator
        {
        public:
            ring_heap_allocator(const ring_heap_allocator&)              = delete;
            ring_heap_allocator(ring_heap_allocator&&)                   = delete;

            ring_heap_allocator& operator = (const ring_heap_allocator&) = delete;
            ring_heap_allocator& operator = (ring_heap_allocator&&)      = delete;

            ring_heap_allocator(ring_base::span_t sz, bool infinite = false) :
                                        ring_buff_ ( manual_heap::alloc<T_featured>(sz) ),
                                        info_(reinterpret_cast<std::uint8_t*>(ring_buff_), sz, sizeof(T), infinite)
            {
            }

            ~ring_heap_allocator()
            {
                manual_heap::free(ring_buff_);
            }

        private:

            struct T_featured
            {
                T obj;
                ring_base::feature_t feature;
            };

            T_featured * const ring_buff_;

        protected:

            ring_base::ring_info info_;
        };

        // ===================================================================
        // Ring buffer maker class
        // ===================================================================
        template <typename T, typename allocator = ring_heap_allocator<T>>
        class ring_maker :  private allocator
        {
        public:
            ring_maker(const ring_maker&)              = delete;
            ring_maker(ring_maker&&)                   = delete;

            ring_maker& operator = (const ring_maker&) = delete;
            ring_maker& operator = (ring_maker&&)      = delete;

            explicit ring_maker(ring_base::span_t sz, bool infinite = false) : allocator(sz, infinite)
            {
            }

            bool is_good() const
            {
                return (nullptr != this->info_.ptr_buff ? true : false);
            }

            ring_base::span_t get_count() const
            {
                return ring_base::get_count(this->info_);
            }

            void reset ()
            {
                ring_base::reset(this->info_);
            }

            bool push(const T& t, bool b_hidden = false)
            {
                return ring_base::push(this->info_, reinterpret_cast<const std::uint8_t*>(&t), b_hidden);
            }

            bool pop()
            {
                return ring_base::pop(this->info_, nullptr);
            }

            bool pop(T& t)
            {
                return ring_base::pop(this->info_, reinterpret_cast<std::uint8_t*>(&t));
            }

            bool read_shadow(T& t)
            {
                return ring_base::read_shadow(this->info_, reinterpret_cast<std::uint8_t*>(&t));
            }

            const T* read_shadow_ptr()
            {
                return reinterpret_cast<const T*>( ring_base::read_shadow_ptr(this->info_) );
            }

            bool pop_if_visited()
            {
                return ring_base::pop_if_visited(this->info_);
            }

            bool is_node_visited()
            {
                return ring_base::is_node_visited(this->info_);
            }

            bool unhide_if_hidden()
            {
                return ring_base::unhide_if_hidden(this->info_);
            }

        };
    }

    namespace data
    {
        // ===================================================================
        // Alias declarations, types, constants
        // ===================================================================

        template <class... T>
        constexpr bool always_false = false;

        using len_t = std::uint16_t;


        // ===================================================================
        // Class declaration, implementation
        // ===================================================================

        template <typename T, typename FR_t = void, std::size_t... Sizes>
        class str_param
        {
        public:

            using ext_func_t = bool(&)(const char*, len_t, T);

            str_param(T&& param, ext_func_t f, const char (&...args)[Sizes]) :
                param_{ std::forward<T>(param) }, func_{f}, strs_(args...)
            {
            }

            str_param(T&& param, const char (&...args)[Sizes]) :
                param_{ std::forward<T>(param) }, func_{dummy}, strs_(args...)
            {
            }


            bool check_str(const char* ptr_str, len_t len = 0u)
            {
                bool retval = false;

                if (nullptr != ptr_str)
                {
                    if (0u == len)
                    {
                        len = std::strlen(ptr_str);
                    }
                    else
                    {
                        // do nothing
                    }

                    retval = for_each( ptr_str, len, std::make_index_sequence<sizeof...(Sizes)>{} );
                }
                else
                {
                    // do nothing
                }

                return retval;
            }

        protected:

            std::uint32_t string_to_ul(const char* ptr)
            {
                char* end_c;
                return static_cast<std::uint32_t>( std::strtoul(ptr, &end_c, 10) );
            }

            double string_to_double(const char* ptr)
            {
                char* end_c;
                return std::strtod(ptr, &end_c);
            }

            bool check_str_single(const char* ptr_str, len_t len, const char* arr0, len_t N0)
            {
                bool retval = false;

                if (len > N0)
                {
                    if (0 == strncmp(ptr_str, arr0, N0))
                    {
                        ptr_str += N0;
                        len -= N0;

                        if constexpr (std::is_same_v<FR_t, void>)
                        {
                            buffer::auto_heap<char> ptr_new(len + 1);
                            std::memcpy(ptr_new, ptr_str, len);
                            ptr_new[len] = '\0';

                            using Treal = typename std::remove_reference_t<T>;

                            if constexpr (std::is_integral_v<Treal>)
                            {
                                param_ = static_cast<Treal>( string_to_ul(ptr_new) );
                                retval = true;
                            }
                            else if constexpr (std::is_floating_point_v<Treal>)
                            {
                                param_ = static_cast<Treal>( string_to_double(ptr_new) );
                                retval = true;
                            }
                            else if constexpr (std::is_array_v<Treal>)
                            {
                                len_t min_len = sizeof(param_) < (len + 1u) ? (sizeof(param_) - 1u) : len;
                                std::memcpy(param_, ptr_new, min_len);
                                param_[min_len] = '\0';
                                retval = true;
                            }
                            else
                            {
                                // do not know what to do, so do nothing
                            }
                        }
                        else
                        {
                            retval = func_(ptr_str, len, param_);
                        }
                    }
                    else
                    {

                    }
                }
                else
                {
                    // do nothing
                }

                return retval;
            }

            template<std::size_t N>
            bool f0(const char* ptr_str, len_t len, const char (&arr)[N])
            {
                return check_str_single(ptr_str, len, &arr[0], N-1);
            }

            template <std::size_t... Idx>
            bool for_each(const char* ptr_str, len_t len, std::index_sequence<Idx...>)
            {
                return ( f0(ptr_str, len, std::get<Idx>(strs_)) || ... );
            }

        private:

            static bool dummy(const char*, len_t, T) { return false; }

            T param_;

            ext_func_t func_;

            std::tuple<const char (&)[Sizes]...> strs_;
        };

        template <typename T, typename FR_t, std::size_t... Sizes>
        str_param(T&&, FR_t (&)(const char*, len_t, T), const char (&...args)[Sizes]) -> str_param<T, FR_t, Sizes...>;

        template <typename T, std::size_t... Sizes>
        str_param(T&&, const char (&...args)[Sizes]) -> str_param<T, void, Sizes...>;


        ////////////////////////////////////////////////
        // Class to hold str_params
        ////////////////////////////////////////////////

        template <typename Delimiter, typename... Args>
        class str_parser
        {
        public:

            str_parser(const str_parser&)              = delete;
            str_parser(str_parser&&)                   = delete;

            str_parser& operator = (const str_parser&) = delete;
            str_parser& operator = (str_parser&&)      = delete;

            str_parser(Delimiter&& delim, const char* str_guard, Args&&...args) :
                delim_{ std::forward<Delimiter>(delim) }, str_guard_(str_guard), args_(args...)
            {
            }

            bool parse(const char* ptr_str, len_t len = 0u)
            {
                bool retval = false;

                if (nullptr != ptr_str)
                {
                    if (0u == len)
                    {
                        len = std::strlen(ptr_str);
                    }
                    else
                    {
                        // do nothing
                    }

                    if (nullptr != str_guard_ && nullptr != strstr(ptr_str, str_guard_) )
                    {
                        retval = true;
                    }
                    else
                    {
                        // do nothing
                    }

                    if (retval || nullptr == str_guard_)
                    {
                        const char* ptr_slide = ptr_str;
                        len_t len_left = len;

                        using Delim_real = typename std::remove_reference_t<Delimiter>;

                        len_t delim_len = 0u;
                        if constexpr (std::is_array_v<Delim_real>)
                        {
                            delim_len = std::strlen(delim_);
                        }
                        else if (std::is_same_v<Delim_real, char>)
                        {
                            delim_len = 1u;
                        }

                        while (len_left > 0u && len_left <= len)
                        {
                            if constexpr (std::is_array_v<Delim_real> || std::is_same_v<Delim_real, char>)
                            {
                                const char* ptr_pos = nullptr;

                                if constexpr (std::is_array_v<Delim_real>)
                                {
                                    ptr_pos = std::strstr(ptr_slide, delim_);
                                }
                                else if (std::is_same_v<Delim_real, char>)
                                {
                                    ptr_pos = std::strchr(ptr_slide, delim_);
                                }

                                len_t part_len = (nullptr != ptr_pos) ? (ptr_pos - ptr_slide) : len_left;

                                retval |= for_each( ptr_slide, part_len, std::make_index_sequence<sizeof...(Args)>{} );

                                len_left -= part_len + delim_len;
                                ptr_slide += part_len + delim_len;
                            }
                            else
                            {
                                static_assert(always_false<Delim_real>, "Delimiter type undefined. Must be either char array or single char");
                            }
                        }
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

        private:


            template <std::size_t... Idx>
            bool for_each(const char* ptr_str, len_t len, std::index_sequence<Idx...>)
            {
                return ( std::get<Idx>(args_).check_str(ptr_str, len) || ... );
            }

            Delimiter delim_;

            const char* str_guard_;

            std::tuple<Args...> args_;
        };

        template <typename Delimiter, typename... Args>
        str_parser(Delimiter&&, const char*, Args&&...) -> str_parser<Delimiter, Args...>;
    }

}

#endif // CPP_EMB_LIB_HPP_INCLUDED
