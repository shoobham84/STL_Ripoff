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
#include <cassert>

namespace moo 
{


template<typename charType, bool threadSafe = false>
struct control_block {
	using ref_type = std::conditional_t<threadSafe, std::atomic<uint32_t>, uint32_t>;

	uint32_t size;
	uint32_t capacity;
	ref_type reference_count;
	bool unshareable = false;

	constexpr charType *data() {
		return reinterpret_cast<charType *>(this + 1);
	}

	constexpr const charType *data() const {
		return reinterpret_cast<const charType*>(this + 1);
	}


	constexpr void inc_ref() noexcept {
		if constexpr (threadSafe) {
			reference_count.fetch_add(1, std::memory_order_relaxed);
		}
		else {
			++reference_count;
		}
	}

	constexpr bool dec_ref() noexcept {
		if constexpr (threadSafe) {
			return reference_count.fetch_sub(1, std::memory_order_acq_rel) == 1;
		}
		else {
			return --reference_count == 0;
		}
	}

	constexpr bool is_shared() const noexcept {
		if constexpr (threadSafe) {
			return reference_count.load(std::memory_order_acquire) > 1;
		}
		else {
			return reference_count > 1;
		}
	}

	constexpr bool is_shareable() const noexcept {
		return !unshareable;
	}

	constexpr void mark_unshareable() noexcept {
		unshareable = true;
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
		else if (!other.m_heap.m_cb->is_shareable()) {
			set_heap_mode();
			auto *cblk = allocate_block(other.m_heap.size);
			cblk->size = static_cast<uint32_t>(other.m_heap.size);
			std::memcpy(cblk->data(), other.m_heap.m_cb->data(), (other.m_heap.size + 1) * sizeof(value_type));

			m_heap.m_cb = cblk;
			m_heap.size = other.m_heap.size;
			m_heap.capacity = other.m_heap.size;
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
		else if (!other.m_heap.m_cb->is_shareable()) {
			set_heap_mode();
			auto *cblk = allocate_block(other.m_heap.size);
			cblk->size = static_cast<uint32_t>(other.m_heap.size);
			std::memcpy(cblk->data(), other.m_heap.m_cb->data(), (other.m_heap.size + 1) * sizeof(value_type));

			m_heap.m_cb = cblk;
			m_heap.size = other.m_heap.size;
			m_heap.capacity = other.m_heap.size;
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

	basic_string_cow(const value_type* str) {
		if (str == nullptr || str[0] == static_cast<value_type>(0)) {
			m_sso = {};
			set_sso_size(0);
			return;
		}

		size_type len = std::char_traits<value_type>::length(str);
		if (len <= SSO_CAP) {
			set_sso_size(len);
			std::char_traits<value_type>::copy(m_sso.data, str, len);
		}
		else {
			set_heap_mode();
			auto *cblk = allocate_block(len);
			cblk->size = static_cast<uint32_t>(len);
			std::char_traits<value_type>::copy(cblk->data(), str, len);
			cblk->data()[len] = static_cast<value_type>(0);

			m_heap.m_cb = cblk;
			m_heap.size = len;
			m_heap.capacity = len;
		}
	}

	basic_string_cow(size_type count, value_type chr) {
		if (count <= SSO_CAP) {
			std::char_traits<value_type>::assign(m_sso.data, count, chr);
			set_sso_size(count);
		}
		else {
			set_heap_mode();
			auto *cblk = allocate_block(count);
			cblk->size = static_cast<uint32_t>(count);
			std::char_traits<value_type>::assign(cblk->data(), count, chr);
			cblk->data()[count] = static_cast<value_type>(0);

			m_heap.m_cb = cblk;
			m_heap.size = count;
			m_heap.capacity = count;
		}
	}

	basic_string_cow(const value_type *str, size_type count) {
		if (count <= SSO_CAP) {
			std::char_traits<value_type>::copy(m_sso.data, str, count);
			set_sso_size(count);
		}
		else {
			set_heap_mode();
			auto *cblk = allocate_block(count);
			cblk->size = static_cast<uint32_t>(count);
			std::char_traits<value_type>::copy(cblk->data(), str, count);
			cblk->data()[count] = static_cast<value_type>(0);

			m_heap.m_cb = cblk;
			m_heap.size = count;
			m_heap.capacity = count;
		}
	}

	~basic_string_cow() {
		if (!is_sso()) {
			if (m_heap.m_cb->dec_ref()) deallocate_block(m_heap.m_cb);
		}
	}

	// =========== Iterators =========
	constexpr const_iterator cbegin() const noexcept {
		return data();
	}

	constexpr const_iterator begin() const noexcept {
		return data();
	}

	iterator begin() {
		if (!is_sso()) {
			detach();
			m_heap.m_cb->mark_unshareable();
		}
		return is_sso() ? m_sso.data : m_heap.m_cb->data();
	}

	constexpr const_iterator cend() const noexcept {
		return data() + size();
	}

	constexpr const_iterator end() const noexcept {
		return data() + size();
	}

	iterator end() {
		if (!is_sso()) {
			detach();
			m_heap.m_cb->mark_unshareable();
		}
		return (is_sso() ? m_sso.data : m_heap.m_cb->data()) + size();
	}

	constexpr view_type view() const noexcept {
		return static_cast<view_type>(*this);
	}

	constexpr bool starts_with(view_type pref) const noexcept {
		return view().starts_with(pref);
	}

	constexpr bool ends_with(view_type suff) const noexcept {
		return view().ends_with(suff);
	}

	constexpr bool contains(view_type sv) const noexcept {
		return view().find(sv) != view_type::npos;
	}

	constexpr size_type find(view_type sv, size_type pos = 0) const noexcept {
		return view().find(sv, pos);
	}


	// ========== Operator Overloads ==========
	[[nodiscard]] constexpr const_reference operator[](size_type pos) const noexcept {
		return data()[pos];
	}

	[[nodiscard]] reference_type operator[](size_type pos) {
		if (!is_sso()) {
			detach();
			m_heap.m_cb->mark_unshareable();
		}
		return (is_sso() ? m_sso.data : m_heap.m_cb->data())[pos];
	}

	[[nodiscard]] friend constexpr auto operator<=>(const basic_string_cow& lhs, const basic_string_cow& rhs) noexcept {
		if (!lhs.is_sso() && !rhs.is_sso() && lhs.m_heap.m_cb == rhs.m_heap.m_cb) return std::strong_ordering::equal;
		return std::basic_string_view<value_type>(lhs) <=> std::basic_string_view<value_type>(rhs);
	}

	[[nodiscard]] friend constexpr auto operator==(const basic_string_cow& lhs, const basic_string_cow& rhs) noexcept {
		if (!lhs.is_sso() && !rhs.is_sso() && lhs.m_heap.m_cb == rhs.m_heap.m_cb) return true;
		return std::basic_string_view<value_type>(lhs) == std::basic_string_view<value_type>(rhs);
	}

	constexpr operator std::basic_string_view<value_type>() const noexcept {
		return std::basic_string_view<value_type>(data(), size());
	}

	friend std::ostream& operator<<(std::ostream& out, const basic_string_cow& str) {
		return out.write(str.data(), str.size());
	}

	basic_string_cow& operator+=(value_type ch) {
		push_back(ch);
		return *this;
	}

	basic_string_cow& operator+=(view_type sv) {
		return append(sv);
	}
	
	// ========== Methods ==========
	[[nodiscard]] constexpr const value_type* data() const noexcept {
		return is_sso() ? m_sso.data : m_heap.m_cb->data();
	}

	[[nodiscard]] value_type* data() {
		if (!is_sso()) {
			detach();
			m_heap.m_cb->mark_unshareable();
		}
		return is_sso() ? m_sso.data : m_heap.m_cb->data();
	}

	[[nodiscard]] constexpr const value_type* c_str() const noexcept {
		return data();
	}

	[[nodiscard]] constexpr size_type size() const noexcept {
		return is_sso() ? get_sso_size() : m_heap.size;
	}

	[[nodiscard]] constexpr size_type capacity() const noexcept {
		return is_sso() ? SSO_CAP : m_heap.capacity;
	}

	[[nodiscard]] constexpr bool empty() const noexcept {
		return size() == 0;
	}

	[[nodiscard]] constexpr size_type length() const noexcept {
		return size();
	}

	void clear() noexcept {
		if (!is_sso()) {
			if (m_heap.m_cb->dec_ref()) deallocate_block(m_heap.m_cb);
		}
		m_sso = {};
		set_sso_size(0);
	}

	void reserve(size_type newCap) {
		if (newCap <= SSO_CAP) return;

		if (is_sso()) {
			size_type len = get_sso_size();
			value_type tmp[SSO_CAP + 1];
			std::memcpy(tmp, m_sso.data, (len+1) * sizeof(value_type));

			auto *cblk = allocate_block(newCap);
			cblk->size = static_cast<uint32_t>(len);
			std::memcpy(cblk->data(), tmp, (len + 1) * sizeof(value_type));

			set_heap_mode();
			m_heap.m_cb = cblk;
			m_heap.size = len;
			m_heap.capacity = newCap;
		}
		else {
			if (newCap > m_heap.capacity || m_heap.m_cb->is_shared()) 
				detach(newCap);
		}
	}

	void push_back(value_type chr) {
		size_type len = size();

		if (is_sso()) {
			if (len < SSO_CAP) [[likely]] {
				m_sso.data[len] = chr;
				set_sso_size(len + 1);
				return;
			}

			value_type tmp[SSO_CAP + 1];
			std::memcpy(tmp, m_sso.data, (len + 1) * sizeof(value_type));

			size_type newCap = SSO_CAP + SSO_CAP / 2;
			auto *cblk = allocate_block(newCap);
			cblk->size = static_cast<uint32_t>(len + 1);
			std::memcpy(cblk->data(), tmp, len * sizeof(value_type));
			cblk->data()[len] = chr;
			cblk->data()[len + 1] = static_cast<value_type>(0);

			set_heap_mode();
			m_heap.m_cb = cblk;
			m_heap.size = len + 1;
			m_heap.capacity = newCap;
		}
		else {
			if (len >= m_heap.capacity || m_heap.m_cb->is_shared()) [[unlikely]] {
				size_type newCap = m_heap.capacity == 0 ? 4 : m_heap.capacity + (m_heap.capacity / 2);
				newCap = std::max(newCap, len + 1);
				detach(newCap);
			}

			m_heap.m_cb->data()[len] = chr;
			m_heap.m_cb->data()[len + 1] = static_cast<value_type>(0);
			m_heap.size++;
			m_heap.m_cb->size++;
		}
	}

	void pop_back() {
		assert(!empty());
		if (is_sso()) 
			set_sso_size(get_sso_size() - 1);
		else {
			detach();
			m_heap.size--;
			m_heap.m_cb->size--;
			m_heap.m_cb->data()[m_heap.size] = static_cast<value_type>(0);
		}
	}

	basic_string_cow& append(view_type sv) {
		if (sv.empty()) return *this;

		const size_type old_len = size();
		const size_type new_len = old_len + sv.size();

		const bool is_self = (sv.data() >= data() && sv.data() < data() + old_len);
		const size_type self_offset = is_self ? static_cast<size_type>(sv.data() - data()) : 0;
		const size_type sv_len = sv.size();

		if (new_len > capacity() || (!is_sso() && m_heap.m_cb->is_shared())) {
			size_type new_cap = std::max(new_len, capacity() + capacity() / 2);
			reserve(new_cap);
		}

		const value_type *src = is_self ? (data() + self_offset) : sv.data();

		if (is_sso()) {
			std::memcpy(m_sso.data + old_len, src, sv_len * sizeof(value_type));
			set_sso_size(new_len);
		}
		else {
			std::memcpy(m_heap.m_cb->data() + old_len, src, sv_len * sizeof(value_type));
			m_heap.size = new_len;
			m_heap.m_cb->size = static_cast<uint32_t>(new_len);
			m_heap.m_cb->data()[new_len] = static_cast<value_type>(0);
		}

		return *this;
	}

private:
	struct Heap_Layout {
		control_block_type *m_cb;
		size_type size;      // cache
		size_type capacity;  // access
		uint8_t _pad[8];
	};
	static_assert(sizeof(Heap_Layout) == 32);

	static constexpr size_type SSO_BUF_SLOTS = (sizeof(Heap_Layout) - 1) / sizeof(value_type);
	static constexpr size_type SSO_CAP = SSO_BUF_SLOTS > 0 ? SSO_BUF_SLOTS - 1 : 0;

	struct SSO_Layout {
		value_type data[SSO_BUF_SLOTS];
		uint8_t _padding[sizeof(Heap_Layout) - SSO_BUF_SLOTS * sizeof(value_type) - 1];
		uint8_t tag;
	};
	static_assert(sizeof(SSO_Layout) == sizeof(Heap_Layout));

	union {
		SSO_Layout m_sso;
		Heap_Layout m_heap;
	};

private:
	void detach(size_type m_minCapacity = 0) {
		if (is_sso()) return;

		if (!m_heap.m_cb->is_shared() && m_heap.capacity >= m_minCapacity) return; 

		size_type newCap = std::max({m_minCapacity, m_heap.capacity, m_heap.size});
		if (newCap > std::numeric_limits<uint32_t>::max() - 1) {
			throw std::length_error("basic_string_cow: requested capacity exceeds 32-bit maximum");
		}

		auto *new_cblk = allocate_block(newCap);
		new_cblk->size = static_cast<uint32_t>(m_heap.size);

		std::memcpy(new_cblk->data(), m_heap.m_cb->data(), (m_heap.size + 1) * sizeof(value_type));

		if (m_heap.m_cb->dec_ref()) deallocate_block(m_heap.m_cb);

		m_heap.m_cb = new_cblk;
		m_heap.capacity = newCap;
	}

	static constexpr size_type cb_size(size_type capacity) noexcept {
		return (sizeof(control_block_type) + (capacity + 1) * sizeof(value_type));
	}

	static control_block_type* allocate_block(size_type capacity) {
		byte_alloc_type alloc;
		size_type bytes = cb_size(capacity);

		std::byte *raw_mem = byte_alloc_traits::allocate(alloc, bytes);

		auto *cb = std::construct_at(reinterpret_cast<control_block_type *>(raw_mem));
		cb->capacity = static_cast<uint32_t>(capacity);

		if (capacity > std::numeric_limits<uint32_t>::max() - 1) {
			throw std::length_error("basic_string_cow: requested capacity exceeds 32-bit maximum");
		}

		if constexpr (threadSafe) {
			cb->reference_count.store(1, std::memory_order_relaxed);
		} 
		else cb->reference_count = 1;
		return cb;
	}


	static void deallocate_block(control_block_type* cb) {
		byte_alloc_type alloc;
		size_type bytes = cb_size(cb->capacity);
		std::destroy_at(cb);
		byte_alloc_traits::deallocate(alloc, reinterpret_cast<std::byte *>(cb), bytes);
	}

	constexpr bool is_sso() const noexcept {
		return (m_sso.tag & 0x80) == 0;
	}

	constexpr void set_sso_size(size_type len) noexcept {
		m_sso.tag = static_cast<uint8_t>(len);
		m_sso.data[len] = static_cast<value_type>(0);
	} 

	constexpr size_type get_sso_size() const noexcept { return m_sso.tag; }

	constexpr void set_heap_mode() noexcept { m_sso.tag = 0x80; }
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
