// LoomNetworkSimulate.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "../../../src/LoomNetwork.h"
#include "../../../src/CircularBuffer.h"
#include <iostream>
#include <vector>

class Int {
public:
	Int(int i) : m_i(i) {}
	~Int() { std::cout << m_i << " Dtor called!" << std::endl; }

	int m_i;
};

int main()
{

	std::cout << "Hello World!\n"; 

	std::cout << "Begin testing circular buffer!\n";

	// LoomNetwork<128, 128> n;

	{
		std::vector<bool> outputs;
		CircularBuffer<Int, 6> buffer;

		outputs.push_back(buffer.add_front(Int(2)));

		outputs.push_back(buffer.add_back(Int(3)));

		outputs.push_back(buffer.emplace_back(5));

		outputs.push_back(buffer.add_back(Int(4)));

		outputs.push_back(buffer.emplace_front(9));

		outputs.push_back(buffer.add_front(Int(4)));

		outputs.push_back(buffer.add_back(Int(3)));
		outputs.push_back(buffer.add_front(Int(3)));

		for (auto const& i : buffer.crange()) std::cout << i.m_i << ", ";
		std::cout << std::endl;

		for (auto const& i : outputs) std::cout << i << ", ";
		std::cout << std::endl;

		buffer.destroy_back();

		buffer.destroy_front();

		for (auto const& i : buffer.crange()) std::cout << i.m_i << ", ";
		std::cout << std::endl;

		buffer.add_back(Int(30));
		buffer.add_front(Int(90));

		for (auto const& i : buffer.crange()) std::cout << i.m_i << ", ";
		std::cout << std::endl;

		auto iter = buffer.crange().begin();
		++iter;
		++iter;
		++iter;

		buffer.remove(iter);

		for (auto const& i : buffer.crange()) std::cout << i.m_i << ", ";
		std::cout << std::endl;

		for (auto iter = buffer.crange().begin(); iter != buffer.crange().end(); ++iter)  std::cout << (*iter).m_i << ", ";
		std::cout << std::endl;
	}
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
