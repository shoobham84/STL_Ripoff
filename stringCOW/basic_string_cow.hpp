#pragma once

#include <cstdint>
#include <type_traits>
#include <memory>
#include <cstring>
#include <string_view>
#include <compare>
#include <algorithm>
#include <iterator>
#include <atomic>

namespace moo 
{


template<typename charType, bool threadSafe = false>
struct control_block {
	using ref_type = std::conditional_t<threadSafe, std::atomic<uint32_t>, uint32_t>;

	uint32_t size;
	uint32_t capacity;
	ref_type reference_count;

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


	void inc_ref() noexcept {
		if constexpr (threadSafe) {
			reference_count.fetch_add(1, std::memory_order_relaxed);
		}
		else {
			++reference_count;
		}
	}

	bool dec_ref() noexcept {
		if constexpr (threadSafe) {
			return reference_count.fetch_sub(1, std::memory_order_acq_rel) == 1;
		}
		else {
			return --reference_count == 0;
		}
	}

	bool is_shared() const noexcept {
		if constexpr (threadSafe) {
			return reference_count.load(std::memory_order_acquire) > 1;
		}
		else {
			return reference_count > 1;
		}
	}
};


template<typename T>
concept CharacterType = std::is_trivially_copyable_v<T> && std::is_trivially_default_constructible_v<T> && std::is_standard_layout_v<T>;

template<CharacterType T, typename Allocator = std::allocator<T>, bool threadSafe = false>
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
	using view_type = std::basic_string_view<value_type>;
	using control_block_type = control_block<value_type, threadSafe>;
	
	static const size_type npos = static_cast<size_type>(-1);

private:
	using byte_alloc_type = typename std::allocator_traits<Allocator>::template rebind_alloc<std::byte>;
	using byte_alloc_traits = std::allocator_traits<byte_alloc_type>;

public:
	basic_string_cow() noexcept {
		m_sso = {};
		set_sso_size(0);
	}

	basic_string_cow(const basic_string_cow& other) {
		if (other.is_sso()) {
			std::memcpy(this, &other, sizeof(*this));
		}
		else {
			m_heap = other.m_heap;
			m_heap.m_cb->inc_ref();
		}
	}

	basic_string_cow(basic_string_cow&& other) noexcept
	{
		std::memcpy(this, &other, sizeof(*this)); 
		other.m_sso = {};
		other.set_sso_size(0);
	}

	basic_string_cow& operator=(const basic_string_cow& other) {
		if (this == &other) return *this;

		if (!is_sso()) {
			if (m_heap.m_cb->dec_ref()) {
				deallocate_block(m_heap.m_cb);
			}
		}

		if (other.is_sso()) {
			std::memcpy(this, &other, sizeof(*this));
		}
		else {
			m_heap = other.m_heap;
			m_heap.m_cb->inc_ref();
		}

		return *this;
	}

	basic_string_cow& operator=(basic_string_cow&& other) noexcept {
		if (this == &other) return *this;

		if (!is_sso()) {
			if (m_heap.m_cb->dec_ref()) deallocate_block(m_heap.m_cb);
		}

		std::memcpy(this, &other, sizeof(*this));
		other.m_sso = {};
		other.set_sso_size(0);

		return *this;
	}

	~basic_string_cow() {
		if (!is_sso()) {
			if (m_heap.m_cb->dec_ref()) deallocate_block(m_heap.m_cb);
		}
	}

	basic_string_cow(const value_type* str) {
		if (str == nullptr || str[0] == static_cast<value_type>(0)) {
			m_sso = {};
			set_sso_size(0);
			return;
		}

		size_type len = std::char_traits<value_type>::length(str);
		if (len <= SSO_CAP) {
			set_sso_size(len);
			std::memcpy(m_sso.data, str, len * sizeof(value_type));
		}
		else {
			set_heap_mode();
			auto *cblk = allocate_block(len);
			cblk->size = static_cast<uint32_t>(len);
			std::memcpy(cblk->data(), str, len * sizeof(value_type));
			cblk->data()[len] = static_cast<value_type>(0);

			m_heap.m_cb = cblk;
			m_heap.size = len;
			m_heap.capacity = len;
		}
	}

	// =========== Iterators =========
	constexpr const_iterator cbegin() const noexcept {
		return m_cb->data();
	}

	constexpr iterator begin() const noexcept {
		return m_cb->data();
	}

	constexpr iterator begin() {
		detach();
		return m_cb->data();
	}

	constexpr const_iterator cend() const noexcept {
		return m_cb->data() + m_cb->size;
	}

	constexpr const_iterator end() const noexcept {
		return m_cb->data + m_cb->size;
	}

	constexpr const_iterator end() {
		detach();
		return m_cb->data + m_cb->size;
	}

	view_type view() const noexcept {
		return static_cast<view_type>(*this);
	}

	bool starts_with(view_type pref) const noexcept {
		return view().starts_with(pref);
	}

	bool ends_with(view_type suff) const noexcept {
		return view().ends_with(suff);
	}

	bool contains(view_type sv) const noexcept {
		return view().find(sv) != view_type::npos;
	}

	size_type find(view_type sv, size_type pos = 0) const noexcept {
		return view().find(sv, pos);
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

	friend std::ostream& operator<<(std::ostream& out, const basic_string_cow<value_type>& str) {
		return out << str.m_cb->data();
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

	[[nodiscard]] constexpr bool empty() const noexcept {
		return m_cb->size == 0;
	}

	constexpr size_type length() const noexcept {
		return m_cb->size;
	}

	void clear() noexcept {
		if (m_cb != control_block<value_type>::empty_instance()) {
			if (--m_cb->reference_count == 0) deallocate_block(m_cb);
		}
		m_cb = control_block_type::empty_instance();
	}


	void reserve(size_type newCap) {
		if (newCap > m_cb->capacity || m_cb->is_shared()) 
			detach(newCap);
	}


	void push_back(value_type chr) {
		if (m_cb->size >= m_cb->capacity || m_cb->is_shared()) [[unlikely]] {
			size_type nextCap = m_cb->capacity == 0 ? 4 : m_cb->capacity + (m_cb->capacity)/2;
			detach(nextCap);
		}

		m_cb->data()[m_cb->size] = chr;
		m_cb->size++;
		m_cb->data()[m_cb->size] = static_cast<value_type>(0);
	}

private:
	constexpr static size_type SSO_CAP = 31;

	struct alignas(16) SSO_Layout {
		value_type data[31];
		uint8_t tag;
	};
	static_assert(sizeof(SSO_Layout) == 32);

	struct Heap_Layout {
		control_block_type *m_cb;
		size_type size;
		size_type capacity;
		uint64_t flags;
	};
	static_assert(sizeof(Heap_Layout) == 32);

	union {
		SSO_Layout m_sso;
		Heap_Layout m_heap;
	};

private:
	void detach(size_type m_minCapacity = 0) {
		if (is_sso()) return;

		if (!m_heap.m_cb->is_shared() && m_heap.capacity >= m_minCapacity) return; 

		size_type newCap = std::max(m_minCapacity, m_heap.size);
		auto *new_cblk = allocate_block(newCap);
		new_cblk->size = static_cast<uint32_t>(newCap);

		std::memcpy(new_cblk->data(), m_heap.m_cb->data(), (m_heap.size + 1) * sizeof(value_type));

		if (m_heap.m_cb->dec_ref()) deallocate_block(m_heap.m_cb);

		m_heap.m_cb = new_cblk;
		m_heap.capacity = newCap;
	}

	static constexpr size_type cb_size(size_type capacity) noexcept {
		return (sizeof(control_block<value_type>) + (capacity + 1) * sizeof(value_type));
	}

	static control_block_type* allocate_block(size_type capacity) {
		byte_alloc_type alloc;
		size_type bytes = cb_size(capacity);

		std::byte *raw_mem = byte_alloc_traits::allocate(alloc, bytes);

		auto *cb = reinterpret_cast<control_block_type*>(raw_mem);
		cb->capacity = capacity;
		cb->reference_count = 1;
		return cb;
	}


	static void deallocate_block(control_block_type* cb) {
		byte_alloc_type alloc;
		size_type bytes = cb_size(cb->capacity);

		byte_alloc_traits::deallocate(alloc, reinterpret_cast<std::byte *>(cb), bytes);
	}

	bool is_sso() const noexcept {
		return (m_sso.tag & 0x80) == 0;
	}

	void set_sso_size(size_type len) noexcept {
		m_sso.tag = static_cast<uint8_t>(len);
		m_sso.data[len] = static_cast<value_type>(0);
	} 

	void get_sso_size() const noexcept { return m_sso.tag; }

	void set_heap_mode() noexcept { m_sso.tag = 0x80; }
};


using string = basic_string_cow<char, std::allocator<char>, false>;
using ts_string = basic_string_cow<char, std::allocator<char>, true>;

using wstring = basic_string_cow<wchar_t, std::allocator<wchar_t>, false>;
using ts_wstring = basic_string_cow<wchar_t, std::allocator<wchar_t>, true>;

using u8string = basic_string_cow<char8_t, std::allocator<char8_t>, false>;
using ts_u8string = basic_string_cow<char8_t, std::allocator<char8_t>, true>;

using u16string = basic_string_cow<char16_t, std::allocator<char16_t>, false>;
using ts_u16string = basic_string_cow<char16_t, std::allocator<char16_t>, true>;
}
