#include "pch.h"

class Int {
public:
	Int(int i, int& dcount) : m_i(i), m_dcount(dcount) { m_dcount++; }
	Int(const Int& rhs) : m_i(rhs.m_i), m_dcount(rhs.m_dcount) { m_dcount++; }
	Int(Int&& rhs) = delete;
	~Int() { m_dcount--; }

	int m_i;
	int& m_dcount;
};

TEST(CircularBuffer, HandlesInsertion) {
	int dcount = 0;
	{
		CircularBuffer<Int, 8> buf;
		EXPECT_FALSE(buf.full());
		EXPECT_TRUE(buf.empty());

		EXPECT_TRUE(buf.emplace_back(1, dcount));
		EXPECT_TRUE(buf.emplace_back(2, dcount));
		EXPECT_TRUE(buf.add_back(Int(3, dcount)));
		EXPECT_TRUE(buf.add_back(Int(4, dcount)));

		EXPECT_TRUE(buf.emplace_front(5, dcount));
		EXPECT_TRUE(buf.emplace_front(6, dcount));
		EXPECT_TRUE(buf.add_front(Int(7, dcount)));
		EXPECT_TRUE(buf.add_front(Int(8, dcount)));

		EXPECT_TRUE(buf.full());
		EXPECT_FALSE(buf.empty());

		EXPECT_FALSE(buf.emplace_back(0, dcount));
		EXPECT_FALSE(buf.add_back(Int(0, dcount)));
		EXPECT_FALSE(buf.emplace_front(0, dcount));
		EXPECT_FALSE(buf.add_front(Int(0, dcount)));

		EXPECT_EQ(buf.size(), 8) << "miscalculated size!";

		constexpr int answer[] = { 8, 7, 6, 5, 1, 2, 3, 4 };
		for (auto i = 0U; i < buf.size(); i++)
			EXPECT_EQ(answer[i], buf[i].m_i) << "Unexpected insertion value at index" << i;
	}

	ASSERT_EQ(dcount, 0) << "improper cleanup, with " << dcount << " not destructed.";
}


class CircularBufferFixture : public ::testing::Test {
protected:

	void SetUp() override {
		m_buf.emplace_back(1, m_dcount);
		m_buf.emplace_back(2, m_dcount);
		m_buf.add_back(Int(3, m_dcount));
		m_buf.add_back(Int(4, m_dcount));
			
		m_buf.emplace_front(5, m_dcount);
		m_buf.emplace_front(6, m_dcount);
		m_buf.add_front(Int(7, m_dcount));
		m_buf.add_front(Int(8, m_dcount));
	}

	constexpr static int m_answer[] = { 8, 7, 6, 5, 1, 2, 3, 4 };
	CircularBuffer<Int, 8> m_buf;
	int m_dcount = 0;
};

TEST_F(CircularBufferFixture, HandlesIteration) {
	int k = 0;
	for (Int& temp : m_buf) {
		EXPECT_EQ(m_answer[k], temp.m_i) << "Unexpected insertion value at index" << k;
		k++;
	}
	k = 0;
	for (const Int& temp : m_buf.crange()) {
		EXPECT_EQ(m_answer[k], temp.m_i) << "Unexpected insertion value at index" << k;
		k++;
	}
}

TEST_F(CircularBufferFixture, HandlesRemove) {
	auto iter = m_buf.begin();
	++iter; ++iter; ++iter; ++iter;
	m_buf.remove(iter);
	EXPECT_EQ(m_buf.size(), 7);
	constexpr int answer[] = { 8, 7, 6, 5, 2, 3, 4 };
	for (auto i = 0U; i < m_buf.size(); i++)
		EXPECT_EQ(answer[i], m_buf[i].m_i) << "Unexpected insertion value at index" << i;
		
	auto citer = m_buf.crange().begin();
	++citer; ++citer; ++citer;
	m_buf.remove(citer);
	EXPECT_EQ(m_buf.size(), 6);
	constexpr int answer2[] = { 8, 7, 6, 2, 3, 4 };
	for (auto i = 0U; i < m_buf.size(); i++)
		EXPECT_EQ(answer2[i], m_buf[i].m_i) << "Unexpected insertion value at index" << i;

	EXPECT_TRUE(m_buf.destroy_back());
	constexpr int answer3[] = { 8, 7, 6, 2, 3 };
	for (auto i = 0U; i < m_buf.size(); i++)
		EXPECT_EQ(answer3[i], m_buf[i].m_i) << "Unexpected insertion value at index" << i;

	EXPECT_TRUE(m_buf.destroy_front());
	constexpr int answer4[] = { 7, 6, 2, 3 };
	for (auto i = 0U; i < m_buf.size(); i++)
		EXPECT_EQ(answer4[i], m_buf[i].m_i) << "Unexpected insertion value at index" << i;

	m_buf.reset();
	EXPECT_EQ(m_dcount, 0);

	EXPECT_FALSE(m_buf.destroy_back());
	EXPECT_FALSE(m_buf.destroy_front());
}

TEST_F(CircularBufferFixture, HandlesFront) {
	EXPECT_EQ(m_buf.front().m_i, m_answer[0]) << "Unexpected front() return value: " << m_buf.front().m_i;
	EXPECT_EQ(m_buf.back().m_i, m_answer[7]) << "Unexpected back() return value: " << m_buf.back().m_i;
}


