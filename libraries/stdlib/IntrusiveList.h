#pragma once

#include <Algorithms.h>
#include <Types.h>
#ifdef __UNIT_TESTS
	#include <assert.h>
	#define ASSERT(x) assert(x)
#else
	#include <Assert.h>
#endif

template <typename T> class IntrusiveList;
// Inherit from this class to use it in IntrusiveList
template <typename T> class IntrusiveListNode
{
	T* next = nullptr;
	T* prev = nullptr;
	IntrusiveList<T>* owner = nullptr;
	friend class IntrusiveList<T>;
};

template <typename T> class IntrusiveList
{
  private:
	T* m_head = nullptr;
	T* m_tail = nullptr;
	size_t m_count = 0;

	void remove_node(T& node);
	void append_node(T& node);

  public:
	class Iterator
	{
	  public:
		Iterator(T* node);
		Iterator(const Iterator& other);
		~Iterator() = default;
		bool operator==(const Iterator& other) const;
		bool operator!=(const Iterator& other) const;
		Iterator& operator++();
		Iterator operator++(int);
		T& operator*();
		T* operator->();

	  private:
		T* m_node = nullptr;
	};
	IntrusiveList() = default;
	~IntrusiveList() = default;
	IntrusiveList(const IntrusiveList&) = delete;
	IntrusiveList(IntrusiveList&&) = delete;
	IntrusiveList& operator=(const IntrusiveList&) = delete;
	IntrusiveList& operator=(IntrusiveList&&) = delete;

	template <typename... U> T& emplace_back(U&&... u);
	T& push_back(T& node);
	T& pop_front();
	T& remove(T& node);
	Iterator begin() const;
	Iterator end() const;
	T& first() const;
	T& tail() const;
	Iterator current(T& node) const;
	bool is_empty() const;
	size_t size() const;
};
template <typename T> IntrusiveList<T>::Iterator::Iterator(T* node) : m_node{node} {}

template <typename T> IntrusiveList<T>::Iterator::Iterator(const Iterator& other) : m_node{other.m_node} {}

template <typename T> bool IntrusiveList<T>::Iterator::operator==(const Iterator& other) const
{
	return m_node == other.m_node;
}

template <typename T> bool IntrusiveList<T>::Iterator::operator!=(const Iterator& other) const
{
	return m_node != other.m_node;
}

template <typename T> typename IntrusiveList<T>::Iterator& IntrusiveList<T>::Iterator::operator++()
{
	m_node = m_node->next;
	return *this;
}

template <typename T> typename IntrusiveList<T>::Iterator IntrusiveList<T>::Iterator::operator++(int)
{
	Iterator old(*this);
	m_node = m_node->next;
	return old;
}

template <typename T> T& IntrusiveList<T>::Iterator::operator*()
{
	return *m_node;
}

template <typename T> T* IntrusiveList<T>::Iterator::operator->()
{
	return m_node;
}

template <typename T> void IntrusiveList<T>::remove_node(T& node)
{
	ASSERT(node.owner == this);
	if ((m_head == &node) && (m_tail == &node)) {
		m_head = m_tail = nullptr;
	} else if (m_head == &node) {
		m_head = node.next;
		node.next->prev = nullptr;
	} else if (m_tail == &node) {
		m_tail = node.prev;
		node.prev->next = nullptr;
	} else {
		node.prev->next = node.next;
		node.next->prev = node.prev;
	}
	node.owner = nullptr;
	m_count--;
}

template <typename T> void IntrusiveList<T>::append_node(T& node)
{
	if (node.owner == nullptr)
		node.owner = this;

	if (!m_head) {
		node.prev = nullptr;
		node.next = nullptr;
		m_head = m_tail = &node;
	} else {
		node.prev = m_tail;
		node.next = nullptr;
		m_tail->next = &node;
		m_tail = &node;
	}
	m_count++;
}

template <typename T> template <typename... U> T& IntrusiveList<T>::emplace_back(U&&... u)
{
	T& node = *new T{forward<U>(u)...};
	append_node(node);
	return node;
}

template <typename T> T& IntrusiveList<T>::push_back(T& node)
{
	append_node(node);
	return node;
}

template <typename T> T& IntrusiveList<T>::pop_front()
{
	ASSERT(m_head);

	T* node = m_head;
	remove_node(*node);
	return *node;
}

template <typename T> T& IntrusiveList<T>::remove(T& node)
{
	ASSERT(m_head);

	remove_node(node);
	return node;
}

template <typename T> typename IntrusiveList<T>::Iterator IntrusiveList<T>::begin() const
{
	return Iterator(m_head);
}

template <typename T> typename IntrusiveList<T>::Iterator IntrusiveList<T>::end() const
{
	return Iterator(nullptr);
}

template <typename T> typename IntrusiveList<T>::Iterator IntrusiveList<T>::current(T& node) const
{
	return Iterator{&node};
}

template <typename T> T& IntrusiveList<T>::first() const
{
	ASSERT(m_head);

	return *m_head;
}

template <typename T> T& IntrusiveList<T>::tail() const
{
	ASSERT(m_head);

	return *m_tail;
}

template <typename T> bool IntrusiveList<T>::is_empty() const
{
	return !m_count;
}

template <typename T> size_t IntrusiveList<T>::size() const
{
	return m_count;
}