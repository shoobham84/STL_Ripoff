#include <cstdint>
#include <type_traits>
#include <memory>
#include <cstring>

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



template<typename T, typename Allocator = std::allocator<T>>
class basic_string_cow
{
	static_assert(std::is_trivially_copyable_v<T> && std::is_trivially_default_constructible_v<T> && std::is_standard_layout_v<T>);

public:
	using value_type = T;
	using pointer_type = T*;
	using alloc_type = Allocator;
	using size_type = std::size_t;
	using const_reference = const T&;
	using reference_type = T&;

private:
	using byte_alloc_type = typename std::allocator_traits<Allocator>::template rebind_alloc<std::byte>;
	using byte_alloc_traits = std::allocator_traits<byte_alloc_type>;

public:
	basic_string_cow() {
		m_cb = control_block<value_type>::empty_instance();
	}

	basic_string_cow(const basic_string_cow& other) 
	:m_cb(other.m_cb) {}

	basic_string_cow(basic_string_cow&& other) noexcept
	: m_cb(other.m_cb)
	{
		other.m_cb = control_block<value_type>::empty_instance(); 
	}

	basic_string_cow& operator=(const basic_string_cow& other) {
		if (this != other && m_cb!= other.m_cb) {
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

		if (m_cb->capacity < str_size + 1) __builtin_unreachable();  // TODO: Add checks for clang and msvc
	}


	// ========== Operator Overloads ==========
	[[nodiscard]] constexpr const_reference operator[](size_type pos) const noexcept {
		static_assert(pos <= m_cb->size);
		return m_cb->data()[pos];
	}

	[[nodiscard]] constexpr reference_type operator[](size_type pos) {
		static_assert(pos <= m_cb->size);
		return m_cb->data()[pos];
	}

	[[nodiscard]] friend constexpr auto operator<=>(const basic_string_cow& lhs, const basic_string_cow& rhs) {
		return lhs.m_cb <=> rhs.m_cb;
	}
	
	


private:
	control_block<value_type> *m_cb;

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
