#include <cstddef>

namespace pi {

	template<typename _Tp>
	class Vector {
	public:
		using value_type = _Tp;
		using pointer = _Tp *;
		using const_pointer_type = const _Tp *;
		using m_size_type = size_t;
		using size_type = ptrdiff_t;
		using difference_type = ptrdiff_t;

	public:
		Vector() = default;

		~Vector() {
		}
		

	private:
		value_type *m_Data = nullptr;
		m_size_type m_Size{ 0 };
		m_size_type m_Capacity{ 0 };
	};
}
