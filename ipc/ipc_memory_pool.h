
#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include <list>
#include <vector>
#include <iostream>
#include <mutex>
#include <algorithm>

namespace IPC
{

template<class T>
struct TBuffer
{
	TBuffer(size_t size) {
		this->data = new T[size];
		this->size = size;
		this->refCount = 0;
	}

	~TBuffer() {
		if (data) {
			delete[] data;
		}
	}

	T*		data;
	size_t	size;
	int		refCount;
};

template<class T>
class MemoryPool
{
public:
	MemoryPool();
	~MemoryPool();

public:
	static MemoryPool<T>* GetInstance() {
		static MemoryPool<T>* instance = nullptr;
		if (!instance) {
			instance = new MemoryPool<T>();
		}

		return instance;
	}

	T* Allocate(size_t size);
	std::shared_ptr<T> AllocatePtr(size_t size);
	void Free(T*);

private:
	std::list<TBuffer<T>*>	lst_free_;
	std::list<TBuffer<T>*>	lst_use_;

	std::mutex				mtx_free_;
	std::mutex				mtx_use_;

	int						free_count_;

};

template<class T>
MemoryPool<T>::MemoryPool()
	: free_count_(0) {

}

template<class T>
MemoryPool<T>::~MemoryPool() {
	TBuffer<T>* buf = nullptr;
	mtx_free_.lock();
	for (auto iter = lst_free_.begin(); iter != lst_free_.end(); iter++) {
		delete *iter;
	}
	mtx_free_.unlock();

	mtx_use_.lock();
	for (auto iter = lst_use_.begin(); iter != lst_use_.end(); iter++) {
		delete *iter;
	}
	mtx_use_.unlock();
}

template<class T>
T* MemoryPool<T>::Allocate(size_t size) {
	TBuffer<T>* buf = nullptr;
	mtx_free_.lock();
	for (auto iter = lst_free_.begin(); iter != lst_free_.end(); iter++) {
		if ((*iter)->size >= size) {
			if ((*iter)->size < 2*size) {
				buf = *iter;
				lst_free_.erase(iter);
			}

			break;
		}
	}
	mtx_free_.unlock();

	if (!buf) {
		buf = new TBuffer<T>(size);
	}

	mtx_use_.lock();
	lst_use_.push_back(buf);
	buf->refCount++;
	mtx_use_.unlock();

	return buf->data;
}

template<class T>
std::shared_ptr<T> MemoryPool<T>::AllocatePtr(size_t size) {
	return std::shared_ptr<T>(Allocate(size), [&](T *data) { if (data) this->Free(data); });
}

template<class T>
void MemoryPool<T>::Free(T* data) {
	TBuffer<T>* buffer = nullptr;

	mtx_use_.lock();
	auto iter = std::find_if(lst_use_.begin(), lst_use_.end(), [data](TBuffer<T>* buf) { return data == buf->data; });
	if (iter != lst_use_.end()) {
		buffer = *iter;
		lst_use_.erase(iter);
	}
	mtx_use_.unlock();

	if (buffer) {
		mtx_free_.lock();
		for (auto iter = lst_free_.begin(); iter != lst_free_.end(); iter++) {
			if (buffer->size <= (*iter)->size) {
				lst_free_.insert(iter, buffer);
				buffer = nullptr;
				break;
			}
		}

		if (buffer) {
			lst_free_.push_back(buffer);
		}

		free_count_++;
		if (free_count_ >= 1000) {
			for (auto iter = lst_free_.begin(); iter != lst_free_.end();) {
				buffer = *(iter);
				if (buffer->refCount == 0) {
					lst_free_.erase(iter++);
					delete buffer;
				}
				else {
					buffer->refCount = 0;
					iter++;
				}
			}

			free_count_ = 0;
		}
		
		mtx_free_.unlock();
	}
}

}

#endif