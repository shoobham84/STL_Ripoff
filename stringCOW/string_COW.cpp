#include "basic_string_cow.hpp"
#include <iostream>

int main()
{
	moo::basic_string_cow<char> s1("HI");
	moo::basic_string_cow<char> s2 = s1;
	s1 = s2;

	std::string_view sv = s1;
	s1[0] = 'h';
	std::cout << s1.c_str() << '\n';
}
