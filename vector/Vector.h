#include <cstddef>
#include <memory>

namespace Pi {

template<typename T, typename Allocator = std::allocator<T>>
class Vector {
public:
	using value_type = T;
	using const_pointer_type = const T *;
	using m_size_type = std::size_t;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;
	using AllocTraits = std::allocator_traits<Allocator>;
	using pointer = typename AllocTraits::pointer;

public:
	Vector() = default;

	explicit Vector(const Allocator& alloc) noexcept 
	: m_Alloc(alloc) {};

	~Vector() {
		::operator delete(m_Data, m_Capacity * sizeof(T));
	}
	
	void clear() noexcept {
		for (size_type i {0}; i < m_Size; ++i) {
			AllocTraits::destroy(m_Alloc, m_Data+i);
		}
		m_Size = 0;
	}

private:
	void reAllocate(size_type newCapacity) {
		pointer newData = AllocTraits::allocate(m_Alloc, newCapacity);

		for (size_type i{0}; i < m_Size; ++i) 
			AllocTraits::construct(m_Alloc, newData+i, std::move_if_noexcept(m_Data[i]));
		
		if (m_Data) AllocTraits::deallocate(m_Alloc, m_Data, m_Capacity);

		m_Data = newData;
		m_Capacity = newCapacity;
	}

private:
	value_type *m_Data = nullptr;
	m_size_type m_Size{ 0 };
	m_size_type m_Capacity{ 0 };
	[[no_unique_address]] Allocator m_Alloc{};
};

} // namespace Pi
