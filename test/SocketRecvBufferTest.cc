#include "SocketRecvBuffer.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <utility>

#include <cppunit/extensions/HelperMacros.h>

#include "SocketCore.h"

namespace aria2 {

class SocketRecvBufferTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(SocketRecvBufferTest);
  CPPUNIT_TEST(testRecvFromDataSource);
  CPPUNIT_TEST_SUITE_END();

public:
  void testRecvFromDataSource();
};

CPPUNIT_TEST_SUITE_REGISTRATION(SocketRecvBufferTest);

namespace {
class FixedDataSource : public SocketRecvBufferDataSource {
public:
  explicit FixedDataSource(std::string data)
      : data_(std::move(data)), offset_(0)
  {
  }

  virtual void readData(void* data, size_t& len) CXX11_OVERRIDE
  {
    auto n = std::min(len, data_.size() - offset_);
    memcpy(data, data_.data() + offset_, n);
    offset_ += n;
    len = n;
  }

private:
  std::string data_;
  size_t offset_;
};
} // namespace

void SocketRecvBufferTest::testRecvFromDataSource()
{
  auto socket = std::make_shared<SocketCore>();
  SocketRecvBuffer buffer(socket);
  buffer.setDataSource(std::make_shared<FixedDataSource>("decoded"));

  CPPUNIT_ASSERT_EQUAL((ssize_t)7, buffer.recv());
  CPPUNIT_ASSERT_EQUAL((size_t)7, buffer.getBufferLength());
  CPPUNIT_ASSERT_EQUAL(std::string("decoded"),
                       std::string(reinterpret_cast<const char*>(
                                       buffer.getBuffer()),
                                   buffer.getBufferLength()));

  buffer.drain(7);
  CPPUNIT_ASSERT(buffer.bufferEmpty());
  CPPUNIT_ASSERT_EQUAL((ssize_t)0, buffer.recv());
}

} // namespace aria2
