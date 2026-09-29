#include <cstdint>
#include <memory>

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
};

template<typename T, typename Allocator = std::allocator<T>>
class basic_string_cow
{
public:
	using value_type = T;
	using pointer_type = T*;
	using alloc_type = Allocator;

	basic_string_cow() {
	}


	basic_string_cow(const basic_string_cow& other);
	basic_string_cow(basic_string_cow&& other);
	basic_string_cow& operator=(const basic_string_cow& other);
	basic_string_cow&& operator=(basic_string_cow&& other);
	~basic_string_cow();
};














}
