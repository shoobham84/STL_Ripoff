#include <cstdint>
#include <type_traits>
#include <memory>
#include <cstring>
#include <string_view>
#include <compare>
#include <algorithm>
#include <iterator>

namespace moo 
{


template<typename charType>
struct control_block {
	uint32_t size;
	uint32_t capacity;
	uint32_t reference_count;

	charType *data() {
		return reinterpret_cast<charType *>(this + 1);
	}

	const charType *data() const {
		return reinterpret_cast<const charType*>(this + 1);
	}

	static control_block *empty_instance() {
		static struct {
			control_block cb { 0, 0, 0};
			charType null_term = 0;
		} empty_block;
		return &empty_block.cb;
	}

	auto operator<=>(const control_block&) const = default;
};


template<typename T>
concept TCDASL = requires(T) {
	std::is_trivially_copyable_v<T> && std::is_trivially_default_constructible_v<T> && std::is_standard_layout_v<T>;
};

template<TCDASL T, typename Allocator = std::allocator<T>>
class basic_string_cow
{

public:
	using value_type = T;
	using pointer_type = T*;
	using const_pointer = const T*;
	using alloc_type = Allocator;
	using size_type = std::size_t;
	using const_reference = const T&;
	using reference_type = T&;
	using iterator = pointer_type;
	using const_iterator = const_pointer;
	
	static const size_type npos = static_cast<size_type>(-1);

private:
	using byte_alloc_type = typename std::allocator_traits<Allocator>::template rebind_alloc<std::byte>;
	using byte_alloc_traits = std::allocator_traits<byte_alloc_type>;

public:
	basic_string_cow() {
		m_cb = control_block<value_type>::empty_instance();
	}

	basic_string_cow(const basic_string_cow& other) 
	:m_cb(other.m_cb) {
		if (m_cb != control_block<value_type>::empty_instance()) {
			m_cb->reference_count++;
		}
	}

	basic_string_cow(basic_string_cow&& other) noexcept
	: m_cb(other.m_cb)
	{
		other.m_cb = control_block<value_type>::empty_instance(); 
	}

	basic_string_cow& operator=(const basic_string_cow& other) {
		if (this != &other && m_cb!= other.m_cb) {
			if (m_cb != control_block<value_type>::empty_instance()) {
				if (--m_cb->reference_count == 0) deallocate_block(m_cb);
			}

			m_cb = other.m_cb;
			if (m_cb != control_block<value_type>::empty_instance()) {
				m_cb->reference_count++;
			}
		}
		return *this;
	}

	basic_string_cow& operator=(basic_string_cow&& other) noexcept {
		std::swap(m_cb, other.m_cb);
		return *this;
	}

	~basic_string_cow() {
		if (m_cb != control_block<value_type>::empty_instance()) {
			if (--m_cb->reference_count == 0) {
				deallocate_block(m_cb);
			}
		}
	}

	basic_string_cow(const T* str) {
		if (str == nullptr || std::strcmp(str, "") == 0) {
			m_cb = control_block<value_type>::empty_instance();
			return;
		}

		size_type str_size = std::strlen(str);
		m_cb = allocate_block(str_size);
		m_cb->size = static_cast<uint32_t>(str_size);
		std::memcpy(m_cb->data(), str, str_size);
		m_cb->data()[str_size] = static_cast<value_type>(0);
	}

	// =========== Iterators =========
	constexpr const_iterator cbegin() const {
		return m_cb->data();
	}

	constexpr iterator begin() {
		return m_cb->data();
	}

	constexpr const_iterator cend() const {
		return m_cb->data[m_cb->size];
	}

	constexpr const_iterator end() {
		return m_cb->data[m_cb->size];
	}

	// ========== Operator Overloads ==========
	[[nodiscard]] constexpr const_reference operator[](size_type pos) const noexcept {
		return m_cb->data()[pos];
	}

	[[nodiscard]] constexpr reference_type operator[](size_type pos) {
		detach();
		return m_cb->data()[pos];
	}

	[[nodiscard]] friend constexpr auto operator<=>(const basic_string_cow& lhs, const basic_string_cow& rhs) {
		if (lhs.m_cb == rhs.m_cb) return std::strong_ordering::equal;
		return std::basic_string_view<value_type>(lhs) <=> std::basic_string_view<value_type>(rhs);
	}

	[[nodiscard]] friend constexpr auto operator==(const basic_string_cow& lhs, const basic_string_cow& rhs) {
		if (lhs.m_cb == rhs.m_cb) return true;
		return std::basic_string_view<value_type>(lhs) == std::basic_string_view<value_type>(rhs);
	}

	constexpr operator std::basic_string_view<value_type>() const noexcept {
		return std::basic_string_view<value_type>(m_cb->data(), m_cb->size);
	}
	
	// ========== Methods ==========
	[[nodiscard]] constexpr size_type size() const noexcept {
		return m_cb->size;
	}

	[[nodiscard]] constexpr size_type capacity() const noexcept {
		return m_cb->capacity;
	}

	[[nodiscard]] constexpr value_type *c_str() const {
		return m_cb->data();
	}

	bool empty() const {
		return (begin() == end());
	}

	constexpr size_type length() const {
		return std::distance(begin(), end());
	}

	void clear() noexcept {
		if (m_cb != control_block<value_type>::empty_instance()) {
			if (--m_cb->reference_count == 0) deallocate_block(m_cb);
		}
		m_cb = control_block<value_type>::empty_instance();
	}


	void reserve(size_type newCap) {
		if (newCap > m_cb->capacity || m_cb->reference_count > 1) 
			detach(newCap);
	}


private:
	control_block<value_type> *m_cb;

private:
	void detach(size_type m_minCapacity = 0) {
		if (m_cb->reference_count == 1 && m_cb->capacity >= m_minCapacity) return;

		size_type new_capacity = std::max(m_minCapacity, static_cast<size_type>(m_cb->size));
		auto *new_cb = allocate_block(new_capacity);
		new_cb->size = m_cb->size;

		std::memcpy(new_cb->data(), m_cb->data(), (m_cb->size() + 1) * sizeof(value_type));

		if (m_cb != control_block<value_type>::empty_instance()) {
			if (--m_cb->reference_count == 0) deallocate_block(m_cb);
		}

		m_cb = new_cb;
	}

	static constexpr size_type cb_size(size_type capacity) noexcept {
		return (sizeof(control_block<value_type>) + (capacity + 1) * sizeof(value_type));
	}

	static control_block<value_type>* allocate_block(size_type capacity) {
		byte_alloc_type alloc;
		size_type bytes = cb_size(capacity);

		std::byte *raw_mem = byte_alloc_traits::allocate(alloc, bytes);

		auto *cb = reinterpret_cast<control_block<value_type>*>(raw_mem);
		cb->capacity = capacity;
		cb->reference_count = 1;
		return cb;
	}


	static void deallocate_block(control_block<value_type>* cb) {
		byte_alloc_type alloc;
		size_type bytes = cb_size(cb->capacity);

		byte_alloc_traits::deallocate(alloc, reinterpret_cast<std::byte *>(cb), bytes);
	}
};




}
