# C++ Embedded Library

Embedded systems, at certain extent, are known to have limited resources and it often becomes prohibitive to use the full functionality of C++ and the Standard Library to say the least.

This small library dependends on C++17 standard and does not use any run-time type information. The library, hopefully, provides convenience in daily development where one often needs a variable lenght array or to have a FIFO buffer or a capability to quickly parse string data over UART or other interface.

The library provides a heap mechanism based on static array which provides a full control on how the heap is used. It uses templates to achieve  necessary polymorphism.

In one or the other way the library components use the static heap provided by the library.

The size and efficiency concerns were kept in mind when implementing library components.


## Table of Contents

- [How to use](#how-to-use)
- [Static heap](#static-heap)
  - [Manual heap](#manual-heap)
  - [Auto heap](#auto-heap)
- [Ring buffer](#ring-buffer)
  - [Thread safety](#thread-safety)
- [String parser](#string-parser)

### How to use

The simplest way to use the library is to copy the library files `cpp_emb_lib.hpp/cpp` and `misc.hpp` into the project. The `usage.hpp/cpp` contains only a code example elaborating the library features. The user is encouraged to check the example code and `cpp_emb_lib.hpp` to have a full picture of available functionality.

### Static heap

All the library components are included in `cel` namespace.

The static heap is based on C-like array and is managed by the `static_heap` class located in `cel::buffer` namespace. By default, the heap size is set to 4KB via macro `CEL_STATIC_HEAP_SIZE` defined in `cpp_emb_lib.hpp`.

The maximum allocatable size on static heap is bound to type `heap_sz_t` which is defined in namespace `cel::buffer` as `std::uint16_t`. One is welcome to update the type to the best appropriate.

Similar to C `malloc` and `free` functions, the class `static_heap` also provides static functions `alloc` and `free`. When a buffer is allocated through the member function `alloc`, the class also reserves a few additional service bytes on the heap for proper management. Upon releasing the allocated buffer through member function `free`, the class automatically performs defragmentation of the heap by merging sequential free chunks.

The class `static_heap` contains static function `free_size` which returns the number of free bytes in the heap. Keep in mind, that this number specifies the total number of unused bytes in the heap and not the size of possible allocatable space, since the free memory areas can be separated by areas which are allocated.

At the moment, `static_heap` is not thread safe, so to avoid problems with concurrent access to functions `alloc` and `free` one needs to protect them with critical sections in external code.

Using `static_heap` directly would be like using C `malloc` and `free` functions. However, `manual_heap` and `auto_heap` classes provide more convenient interfaces which are explained next.

### Manual heap

The easiest is to perceive via examples. The following snippet allocates a buffer of 10 `std::uint16_t`.

```cpp
    auto sz = 10;
    auto ptr_buff = cel::buffer::manual_heap::alloc<std::uint16_t>(sz);
```
<span style="color:orange">Example 1.</span>

In the above example `ptr_buff` is an ordinary pointer to type `std::uint16_t`.

When the buffer is no longer needed it must be released like the following:

```cpp
    cel::buffer::manual_heap::free( ptr_buff );
```
<span style="color:orange">Example 2.</span>

### Auto heap

The difference of template class `auto_heap` is that allocation takes place upon creating an instance of this class and the allocated resource is automatically released when leaving the scope where the instance was created. Example:

```cpp
    auto sz = 32;
    if ( some_condition )
    {
        cel::buffer::auto_heap<std::uint16_t> ptr_buff( sz );
        some_function( ptr_buff );
    }
```
<span style="color:orange">Example 3.</span>

In contrast to `manual_heap`, `ptr_buff` is not a pointer to type `std::uint16_t` but is a type of `auto_heap<std::uint16_t>`. Nevertheless, `ptr_buff` is implicitely convertible to `std::uint16_t*`.

Upon leaving the `if { ... }` scope, the memory allocated for `ptr_buff` is freed. 

Objects of type `auto_heap` are not assignable or transferable.

The only member of template `auto_heap` is a private variable of type `T*`. Therefore, the size of `auto_heap` object is the same as of a pointer.

### Ring buffer

The ring buffer base class `ring_base` is responsible for handling all the burden related to pushing to and poping elements from FIFO buffer. Example:

```cpp
    #include "cpp_emb_lib.hpp"

    // Suppose there is a struct 
    struct cmd_t
    {
        bool m_bool;
    };

    void main()
    {
        // Create two cmd_t object on static heap
        cel::buffer::auto_heap<cmd_t> cmd(2);

        // Initialize the members
        cmd[0].m_bool = true;
        cmd[1].m_bool = false;

        // Create a ring buffer that can hold two cmd_t elements
        cel::buffer::ring_maker<cmd_t> ring_cmd(2);

        // push stores copies of the arguments into FIFO and returns true on success
        (void)ring_cmd.push( cmd[0] );
        (void)ring_cmd.push( cmd[1] );

        // A temporary cmd_t object
        cmd_t cmd_new;

        // check number of elements in the ring
        if ( cmd_ring.get_count() > 0u )
        {
            // pop retrieves a copy of the oldest element into cmd_new and 
            // removes the element from FIFO. Returns true on success
            (void)cmd_ring.pop(cmd_new);
        }
    }
```
<span style="color:orange">Example 4.</span>

In the above example, after pushing two `cmd_t` objects into the ring buffer, only one element is then popped out from FIFO before leaving `main`. This, nevertheless, won't cause a memory leak because, by default, the ring buffer  uses an allocator class `ring_heap_allocator` to book necessary space on static heap and which is released in allocator's destructor.

The pop method to read out elements from ring buffer works if there is only one reader. In case of multiple readers we would like to be able to read the element and keep it in the FIFO till all readers access it. For this, the ring buffer offers the function `read_shadow` or `read_shadow_ptr`. These functions do not remove the oldest element but only mark it as `visited`. Once all the readers did their work, the oldest element can be removed with fuction `pop` with no arguments, or with function `pop_if_visited`. Example:

```cpp
    cel::buffer::ring_maker<cmd_t> g_ring_cmd(2);

    bool thread_writer( const cmd_t& cmd )
    {
        return g_ring_cmd.push( cmd );
    }
    
    bool thread_reader_1( cmd_t& cmd )
    {
        // read_shadow does not pop out the oldest element,
        // it only 'marks' the element as 'visited'
        return g_ring_cmd.read_shadow(cmd);
    }

    bool thread_reader_2( cmd_t& cmd )
    {
        // read_shadow does not pop out the oldest element,
        // it only 'marks' the element as 'visited'
        return g_ring_cmd.read_shadow(cmd);
    }

    void main()
    {
        while ( not_all_threads_yet_processed )
        {
            // process thread...
        }

        // OK, all threads including readers had chance to access FIFO

        // We can also directly access a read-only pointer to the oldest element
        // via read_shadow_ptr. For complex types, this is more efficient than 
        // read_shadow as it does not involve extra copying.
        // Upon failure read_shadow_ptr retrns nullptr
        const cmd_t * ptr_elem = cmd_ring.read_shadow_ptr();

        // Once all readers accessed the oldest FIFO element we
        // can remove it from the buffer
        (void)g_ring_cmd.pop_if_visited();
    }

```
<span style="color:orange">Example 5.</span>

A C++ equivalent to function `read_shadow_ptr` which would return `const &` is not provided because upon failure `read_shadow_ptr` returns `nullptr` which is fine, but it would raise an exception at runtime if we try to read a reference value to `nullptr`.

Sometimes, it is needed to temporarily hide the newly pushed element from readers till the right time comes for the readers to access the new element. This is acheived by setting the second parameter of  `push` to `true`. Example:

```cpp
    cel::buffer::ring_maker<cmd_t> ring_cmd(2);

    cmd_t cmd{...};

    // Push new element and keep it hidden
    ring_cmd.push( cmd, true );

    if ( ring_cmd.get_count() > 0u )
    {
        // get_count returns number of all elements in FIFO including hidden.
        if ( !ring_cmd.pop( cmd ) )
        {
            // The above pop fails because the element is hidden. It would 
            // also fail with function read_shadow_ptr which returns nullptr 
            // for hidden element. We need to unhide it explicitely
            if ( cmd_ring.unhide_if_hidden() )
            {
                const cmd_t * ptr_elem = cmd_ring.read_shadow_ptr();
                if (nullptr != ptr_elem)
                {
                    // This time ptr_elem should not be nullptr
                    ...
                }
            }
        }
    }
```
<span style="color:orange">Example 6.</span>

### Thread safety

The ring buffer implementation comes with thread safety feature for ARM Cortex CPUs. To enable the thread safety one needs to uncomment macro `#define ARM_CROSS_COMPILER` in file `misc.hpp`.

For other CPUs one can add a new clause `#elif defined( SOME_OTHER_ARCHITECTURE )` and define there macros `ENABLE_INTERRUPTS`, `DISABLE_INTERRUPTS` and `SOFTWARE_BREAKPOINT` which are used by the library.

If the library detect a serious failure, it will call ASSERT which in turn will call static inline function `failure1` defined in `misc.hpp`. Feel free to adjust it as needed.

### String parser

For quick test of byte-based communcation interfaces such as UART or when a simple communication is needed between embedded device and outer world one often passes an ASCII string with one or more enclosed commands or data which need to be parsed.

The library includes a string parser component in namespace `cel::data`.

Suppose there is a struct `data_t` where the members of the struct need to be updated according to information received over e.g. UART interface. Example:

```cpp
    // The purpose of the example is to deserialize the string containing data to members of the
    // following struct
    struct data_t
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
    };

    void main()
    {

        data_t dat;

        // Suppose we received a string which have the following content
        char str_tmp[] = "motors_id_present:1,speed:120,param:3.14,sensor_id:3,string:Hello World!";

        // A string parsing object needs to be created like the following
        // The first argument is the delimiter used to separate one field from another in the string
        // The second argument can be ignored for now and just assumed to be nullptr
        // The remaining arguments are of type str_param each of which is responsible to find and convert
        // the value of "key" substring and assign the result to provided variable
        cel::data::str_parser sp { ',', nullptr,

                        cel::data::str_param(dat.m_bool, "motors_id_present:"),
                        cel::data::str_param(dat.m_u32, "speed:"),
                        cel::data::str_param(dat.m_float, "param:"),
                        cel::data::str_param(dat.m_arr, "string:"),

                                };

        // The following parse function returns true if at least one key is found in the string
        if ( sp.parse( str_tmp ) )
        {
            // At this point the members of dat should be initialized according to provided
            // information in the string str_tmp, that is:
            // m_bool = true
            // m_u32 = 120
            // m_float = 3.14
            // m_arr = "Hello Wor\0" (truncated to fit null terminated substring of the input)
        }

    }
```
<span style="color:orange">Example 7.</span>

As described in the comments of the example after calling `sp.parse` the members of variable `dat` get assigned with respective values from the input string.

It is also possible to parse input string for custom types like `enum class id_t` present in `data_t`. For that, a conversion function needs provided to `str_param` constructor. It is also possible to pass a string as a deliminter. 

```cpp
    // The purpose of the example is to deserialize the string containing data to members of the
    // following struct
    struct data_t
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
    };

    bool str_to_id(const char* ptr_str, data::len_t len, cmd_t::id_t& id)
    {
        char* end_c = nullptr;
        id = static_cast<cmd_t::id_t>( std::strtoul(ptr_str, &end_c, 10) );
        return true;
    }

    void main()
    {

        data_t dat;

        // Suppose we received a string which have the following content
        char str_tmp[] = "motors_id_present:1#abc#speed:120#abc#param:3.14#abc#sensor_id:3#abc#string:Hello World!";

        // A string parsing object needs to be created like the following
        // The first argument is the delimiter used to separate one field from another in the string
        // The second argument is the substring "keys" which should be searched for to find 
        // respective values
        cel::data::str_parser sp { "#abc#", nullptr,

                        cel::data::str_param(dat.m_bool, "motors_id_present:"),
                        cel::data::str_param(dat.m_u32, "speed:"),
                        cel::data::str_param(dat.m_float, "param:"),
                        cel::data::str_param(dat.m_arr, "string:"),
                        cel::data::str_param(dat.m_id, str_to_id, "sensor_id:"),

                                };

        // The following parse function returns true if at least one key is found in the string
        if ( sp.parse( str_tmp ) )
        {
            // At this point the members of dat should be initialized according to provided
            // information in the string str_tmp, that is:
            // m_bool = true
            // m_u32 = 120
            // m_float = 3.14
            // m_arr = "Hello Wor\0" (truncated to fit null terminated substring of the input)
            // Custom type m_id = ID_3
        }

    }
```
<span style="color:orange">Example 8.</span>

This example is similar to Example 7 but is supplemented with new function `str_to_id` which is used to convert string value to custom type as well as the `str_parser` constructor is supplemented with additional argument `cel::data::str_param(dat.m_id, str_to_id, "sensor_id:")` where the custom function is provided to `str_param` constructor to get the value for `m_id` member.

For custom type `T`, the conversion function signature must be `bool (const char*, data::len_t, T&)`. When this function is called by `str_parser` class, the first and second parameters contain the string value and the length which the function needs to convert and store the result of the conversion in the third parameter.

In Example 7 and 8 the second parameter to `str_parser` constructor was `nullptr`. This parameter is of type `const char*` and can be used to provide a guarding string. In that case, the string parser will parse the input string only when the guarding sub-string is found in the input string. Otherwise, the input string will not be processed. The input string is always process when the guarding parameter is `nullptr`.