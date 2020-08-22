// Tencent is pleased to support the open source community by making ncnn available.
//
// Copyright (C) 2020 THL A29 Limited, a Tencent company. All rights reserved.
//
// Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
// https://opensource.org/licenses/BSD-3-Clause
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#ifndef NCNN_SIMPLESTL_H
#define NCNN_SIMPLESTL_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if !NCNN_SIMPLESTL

#include <new>

#else

// operator new and delete
void* operator new(size_t sz) noexcept;
void* operator new(size_t sz, void*) noexcept;
void* operator new[](size_t sz) noexcept;
void* operator new[](size_t sz, void*) noexcept;
void operator delete(void *ptr) noexcept;
void operator delete[](void *ptr) noexcept;

#endif

// minimal stl data structure implementation
namespace std {

template<class T>
const T& max(const T& a, const T& b)
{
    return (a < b) ? b : a;
}

template<class T>
const T& min(const T& a, const T& b)
{
    return (a > b) ? b : a;
}

template<class T>
void swap(T& a, T& b)
{
    T temp(a);
    a = b;
    b = temp;
}

template<typename T1, typename T2>
struct pair
{
    pair()
        : first(), second()
    {
    }
    pair(const T1& t1, const T2& t2)
        : first(t1), second(t2)
    {
    }

    T1 first;
    T2 second;
};

template <class T1, class T2>
pair<T1, T2> make_pair(const T1& t1, const T2& t2)
{
    return pair<T1, T2>(t1, t2);
}

template<typename T>
struct node
{
    node* prev_ = nullptr;
    node* next_ = nullptr;
    T data_;

    node()
        : data_()
    {
    }
    node(const T& t)
        : data_(t)
    {
    }
};

template<typename T>
struct iter_list
{
    iter_list()
    {
    }
    iter_list(node<T>* n)
        : curr_(n)
    {
    }
    iter_list(const iter_list& i)
        : curr_(i.curr_)
    {
    }
    ~iter_list()
    {
    }

    iter_list& operator=(const iter_list& i)
    {
        curr_ = i.curr_;
        return *this;
    }

    T& operator*()
    {
        return curr_->data_;
    }
    T* operator->()
    {
        return &(curr_->data_);
    }

    bool operator==(const iter_list& i)
    {
        return curr_ == i.curr_;
    }
    bool operator!=(const iter_list& i)
    {
        return curr_ != i.curr_;
    }

    iter_list& operator++()
    {
        curr_ = curr_->next_;
        return *this;
    }
    iter_list& operator--()
    {
        curr_ = curr_->prev_;
        return *this;
    }

    node<T>* curr_ = nullptr;
};

template<typename T>
struct list
{
    list()
    {
        head_ = new node<T>();
        tail_ = head_;
    }
    ~list()
    {
        clear();
        delete head_;
    }
    list(const list& l)
    {
        head_ = new node<T>();
        tail_ = head_;

        for (iter_list<T> i = l.begin(); i != l.end(); ++i)
        {
            push_back(*i);
        }
    }

    list& operator=(const list& l)
    {
        if (this == &l)
        {
            return *this;
        }
        clear();

        for (iter_list<T> i = l.begin(); i != l.end(); ++i)
        {
            push_back(*i);
        }
        return *this;
    }

    void clear()
    {
        while (count_ > 0)
        {
            pop_front();
        }
    }

    void pop_front()
    {
        if (count_ > 0)
        {
            head_ = head_->next_;
            delete head_->prev_;
            head_->prev_ = nullptr;
            --count_;
        }
    }

    size_t size() const
    {
        return count_;
    }
    iter_list<T> begin() const
    {
        return iter_list<T>(head_);
    }
    iter_list<T> end() const
    {
        return iter_list<T>(tail_);
    }
    bool empty() const
    {
        return count_ == 0;
    }

    void push_back(const T& t)
    {
        if (count_ == 0)
        {
            head_ = new node<T>(t);
            head_->prev_ = nullptr;
            head_->next_ = tail_;
            tail_->prev_ = head_;
            count_ = 1;
        }
        else
        {
            node<T>* temp = new node<T>(t);
            temp->prev_ = tail_->prev_;
            temp->next_ = tail_;
            tail_->prev_->next_ = temp;
            tail_->prev_ = temp;
            ++count_;
        }
    }

    iter_list<T> erase(iter_list<T> pos)
    {
        if (pos != end())
        {
            node<T>* temp = pos.curr_;
            if (temp == head_)
            {
                ++pos;
                temp->next_->prev_ = nullptr;
                head_ = temp->next_;
            }
            else
            {
                --pos;
                temp->next_->prev_ = temp->prev_;
                temp->prev_->next_ = temp->next_;
                ++pos;
            }
            delete temp;
            --count_;
        }
        return pos;
    }

protected:
    node<T>* head_ = nullptr;
    node<T>* tail_ = nullptr;
    size_t count_ = 0;
};

template<typename T>
struct vector
{
    vector()
    {
    }
    vector(const size_t new_size, const T& value = T())
    {
        resize(new_size, value);
    }
    ~vector()
    {
        clear();
    }
    vector(const vector& v)
    {
        resize(v.size());
        for (size_t i = 0; i < size_; i++)
        {
            data_[i] = v.data_[i];
        }
    }

    vector& operator=(const vector& v)
    {
        if (this == &v)
        {
            return *this;
        }
        resize(0);
        resize(v.size());
        for (size_t i = 0; i < size_; i++)
        {
            data_[i] = v.data_[i];
        }
        return *this;
    }

    void resize(const size_t new_size, const T& value = T())
    {
        try_alloc(new_size);
        if (new_size > size_)
        {
            for (size_t i = size_; i < new_size; i++)
            {
                new (&data_[i]) T(value);
            }
        }
        else if (new_size < size_)
        {
            for (size_t i = new_size; i < size_; i++)
            {
                data_[i].~T();
            }
        }
        size_ = new_size;
    }

    void clear()
    {
        for (size_t i = 0; i < size_; i++)
        {
            data_[i].~T();
        }
        delete[](char*) data_;
        data_ = nullptr;
        size_ = 0;
        capacity_ = 0;
    }

    T* data() const
    {
        return data_;
    }
    size_t size() const
    {
        return size_;
    }
    T& operator[](size_t i) const
    {
        return data_[i];
    }
    T* begin() const
    {
        return &data_[0];
    }
    T* end() const
    {
        return &data_[size_];
    }
    bool empty() const
    {
        return size_ == 0;
    }

    void push_back(const T& t)
    {
        try_alloc(size_ + 1);
        new (&data_[size_]) T(t);
        size_++;
    }

    void insert(T* pos, T* b, T* e)
    {
        vector* v = nullptr;
        if (b >= begin() && b < end())
        {
            //the same vector
            v = new vector(*this);
            b = v->begin() + (b - begin());
            e = v->begin() + (e - begin());
        }
        size_t diff = pos - begin();
        try_alloc(size_ + (e - b));
        pos = begin() + diff;
        memmove(pos + (e - b), pos, (end() - pos) * sizeof(T));
        size_t len = e - b;
        size_ += len;
        for (size_t i = 0; i < len; i++)
        {
            *pos = *b;
            pos++;
            b++;
        }
        delete v;
    }

    T* erase(T* pos)
    {
        pos->~T();
        memmove(pos, pos + 1, (end() - pos - 1) * sizeof(T));
        size_--;
        return pos;
    }

protected:
    T* data_ = nullptr;
    size_t size_ = 0;
    size_t capacity_ = 0;
    void try_alloc(size_t new_size)
    {
        if (new_size * 3 / 2 > capacity_ / 2)
        {
            capacity_ = new_size * 2;
            T* new_data = (T*)new char[capacity_ * sizeof(T)];
            memset(new_data, 0, capacity_ * sizeof(T));
            if (data_)
            {
                memmove(new_data, data_, sizeof(T) * size_);
                delete[](char*) data_;
            }
            data_ = new_data;
        }
    }
};

struct string : public vector<char>
{
    string()
    {
    }
    string(const char* str)
    {
        size_t len = strlen(str);
        resize(len);
        memcpy(data_, str, len);
    }
    const char* c_str() const
    {
        return (const char*)data_;
    }
    bool operator==(const string& str2) const
    {
        return strcmp(data_, str2.data_) == 0;
    }
    bool operator==(const char* str2) const
    {
        return strcmp(data_, str2) == 0;
    }
    bool operator!=(const char* str2) const
    {
        return strcmp(data_, str2) != 0;
    }
    string& operator+=(const string& str1)
    {
        insert(end(), str1.begin(), str1.end());
        return *this;
    }
};

inline string operator+(const string& str1, const string& str2)
{
    string str(str1);
    str.insert(str.end(), str2.begin(), str2.end());
    return str;
}

} // namespace std

#endif // NCNN_SIMPLESTL_H
