/*__________________________________________________________________________________________
            (c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2014] ++
            (c) Copyright The Nexus Developers 2014 - 2019
            Distributed under the MIT software license, see the accompanying
            file COPYING or http://www.opensource.org/licenses/mit-license.php.
            "ad vocem populi" - To the Voice of the People
____________________________________________________________________________________________*/

#pragma once
#ifndef NEXUS_UTIL_INCLUDE_MEMORY_H
#define NEXUS_UTIL_INCLUDE_MEMORY_H

#include <cstdint>
#include <stdexcept>
#include <vector>

#include <LLC/aes/aes.h>
#include <TAO/Util/mutex.h>

#include <openssl/rand.h>

namespace memory
{

    /** compare
     *
     *  Compares two byte arrays and determines their signed equivalence byte for
     *   byte.
     *
     *  @param[in] a The first byte array to compare.
     *  @param[in] b The second byte array to compare.
     *  @param[in] size The number of bytes to compare.
     *
     *  @return Returns the signed difference of the first different byte value.
     *
     **/
    int32_t compare(const uint8_t* a, const uint8_t* b, const uint64_t size);


    /** copy
     *
     *  Copies from two sets of iteratos and checks the sizes for potential buffer
     *  overflows.
     *
     *  @param[in] src_begin The source beginning iterator
     *  @param[in] src_end The source ending iterator
     *  @param[in] dst_begin The destination beginning iterator
     *  @param[in] dst_end The destination ending iterator
     *
     **/
    template<typename Type>
    void copy(const Type* src_begin, const Type* src_end, const Type* dst_begin, const Type* dst_end)
    {
        /* Check the source iterators. */
        if (src_end < src_begin)
            throw std::domain_error("src end iterator less than src begin");

        /* Check the destination iterators. */
        if (dst_end < dst_begin)
            throw std::domain_error("dst end iterator less than dst begin");

        /* Check the sizes. */
        if (src_end - src_begin != dst_end - dst_begin)
            throw std::domain_error("src size mismatch with dst size");

        /* Copy the data. */
        std::copy((Type*)src_begin, (Type*)src_end, (Type*)dst_begin);
    }


    /** atomic
     *
     *  Protects an object inside with a mutex.
     *
     **/
    template<class TypeName>
    class atomic
    {
        /* The mutex to protect the memory. */
        mutable std::mutex MUTEX;


        /* The internal data. */
        TypeName data;


    public:

        /** Default Constructor. **/
        atomic()
            : MUTEX()
            , data()
        {
        }


        /** Constructor for storing. **/
        atomic(const TypeName& dataIn)
            : MUTEX()
            , data()
        {
            store(dataIn);
        }


        /** Assignment operator.
         *
         *  @param[in] a The atomic to assign from.
         *
         **/
        atomic& operator=(const atomic& a)
        {
            LOCK(MUTEX);

            data = a.data;
            return (*this);
        }


        /** Assignment operator.
         *
         *  @param[in] dataIn The atomic to assign from.
         *
         **/
        atomic& operator=(const TypeName& dataIn)
        {
            LOCK(MUTEX);

            data = dataIn;
            return (*this);
        }


        /** Equivilent operator.
         *
         *  @param[in] a The atomic to compare to.
         *
         **/
        bool operator==(const atomic& a) const
        {
            LOCK(MUTEX);

            return data == a.data;
        }


        /** Equivilent operator.
         *
         *  @param[in] a The data type to compare to.
         *
         **/
        bool operator==(const TypeName& dataIn) const
        {
            LOCK(MUTEX);

            return data == dataIn;
        }


        /** Not equivilent operator.
         *
         *  @param[in] a The atomic to compare to.
         *
         **/
        bool operator!=(const atomic& a) const
        {
            LOCK(MUTEX);

            return data != a.data;
        }


        /** Not equivilent operator.
         *
         *  @param[in] a The data type to compare to.
         *
         **/
        bool operator!=(const TypeName& dataIn) const
        {
            LOCK(MUTEX);

            return data != dataIn;
        }


        /** load
         *
         *  Load the object from memory.
         *
         **/
        const TypeName load() const
        {
            LOCK(MUTEX);

            return data;
        }


        /** store
         *
         *  Stores an object into memory.
         *
         *  @param[in] dataIn The data to into protected memory.
         *
         **/
        void store(const TypeName& dataIn)
        {
            LOCK(MUTEX);

            data = dataIn;
        }
    };


    /** lock_proxy
     *
     *  Temporary class that unlocks a mutex when outside of scope.
     *  Useful for protecting member access to a raw pointer.
     *
     **/
    template <class TypeName>
    class lock_proxy
    {
        /** Reference of the mutex. **/
        std::recursive_mutex& MUTEX;


        /** The pointer being locked. **/
        TypeName* data;


    public:

        /** Basic constructor
         *
         *  Assign the pointer and reference to the mutex.
         *
         *  @param[in] pData The pointer to shallow copy
         *  @param[in] MUTEX_IN The mutex reference
         *
         **/
        lock_proxy(TypeName* pData, std::recursive_mutex& MUTEX_IN)
            : MUTEX(MUTEX_IN)
            , data(pData)
        {
        }


        /** Destructor
        *
        *  Unlock the mutex.
        *
        **/
        ~lock_proxy()
        {
            MUTEX.unlock();
        }


        /** Member Access Operator.
        *
        *  Access the memory of the raw pointer.
        *
        **/
        TypeName* operator->() const
        {
            if (data == nullptr)
                throw std::runtime_error("member access to nullptr");

            return data;
        }
    };


    /** atomic_ptr
     *
     *  Protects a pointer with a mutex.
     *
     **/
    template<class TypeName>
    class atomic_ptr
    {
        /** The internal locking mutex. **/
        mutable std::recursive_mutex MUTEX;


        /** The internal raw poitner. **/
        TypeName* data;


    public:

        /** Default Constructor. **/
        atomic_ptr()
            : MUTEX()
            , data(nullptr)
        {
        }


        /** Constructor for storing. **/
        atomic_ptr(TypeName* dataIn)
            : MUTEX()
            , data(dataIn)
        {
        }


        /** Copy Constructor. **/
        atomic_ptr(const atomic_ptr<TypeName>& pointer) = delete;


        /** Move Constructor. **/
        atomic_ptr(const atomic_ptr<TypeName>&& pointer)
            : data(pointer.data)
        {
        }


        /** Destructor. **/
        ~atomic_ptr()
        {
        }


        /** Assignment operator. **/
        atomic_ptr& operator=(const atomic_ptr<TypeName>& dataIn) = delete;


        /** Assignment operator. **/
        atomic_ptr& operator=(TypeName* dataIn) = delete;


        /** Equivilent operator.
         *
         *  @param[in] a The data type to compare to.
         *
         **/
        bool operator==(const TypeName& dataIn) const
        {
            RLOCK(MUTEX);

            /* Throw an exception on nullptr. */
            if (data == nullptr)
                return false;

            return *data == dataIn;
        }


        /** Equivilent operator.
         *
         *  @param[in] a The data type to compare to.
         *
         **/
        bool operator==(const TypeName* ptr) const
        {
            RLOCK(MUTEX);

            return data == ptr;
        }


        /** Not equivilent operator.
         *
         *  @param[in] a The data type to compare to.
         *
         **/
        bool operator!=(const TypeName* ptr) const
        {
            RLOCK(MUTEX);

            return data != ptr;
        }

        /** Not operator
         *
         *  Check if the pointer is nullptr.
         *
         **/
        bool operator!(void)
        {
            RLOCK(MUTEX);

            return data == nullptr;
        }


        /** Member access overload
         *
         *  Allow atomic_ptr access like a normal pointer.
         *
         **/
        lock_proxy<TypeName> operator->()
        {
            MUTEX.lock();

            return lock_proxy<TypeName>(data, MUTEX);
        }


        /** dereference operator overload
         *
         *  Load the object from memory.
         *
         **/
        TypeName operator*() const
        {
            RLOCK(MUTEX);

            /* Throw an exception on nullptr. */
            if (data == nullptr)
                throw std::runtime_error("dereferencing a nullptr");

            return *data;
        }

        TypeName* load()
        {
            RLOCK(MUTEX);

            return data;
        }


        /** store
         *
         *  Stores an object into memory.
         *
         *  @param[in] dataIn The data into protected memory.
         *
         **/
        void store(TypeName* dataIn)
        {
            RLOCK(MUTEX);

            if (data)
                delete data;

            data = dataIn;
        }


        /** free
         *
         *  Free the internal memory of the encrypted pointer.
         *
         **/
        void free()
        {
            RLOCK(MUTEX);

            if (data)
                delete data;

            data = nullptr;
        }
    };
}

#endif