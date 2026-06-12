#include "DownloadCommand.h"

#include <cppunit/extensions/HelperMacros.h>

#include "MockSegment.h"
#include "message.h"

namespace aria2 {

class DownloadCommandTest : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(DownloadCommandTest);
  CPPUNIT_TEST(testCreateUnexpectedEOFMessage);
  CPPUNIT_TEST_SUITE_END();

public:
  void testCreateUnexpectedEOFMessage();
};

CPPUNIT_TEST_SUITE_REGISTRATION(DownloadCommandTest);

namespace {
class EOFMessageSegment : public MockSegment {
public:
  EOFMessageSegment(int64_t length, int64_t writtenLength)
      : length_(length), writtenLength_(writtenLength)
  {
  }

  virtual int64_t getLength() const CXX11_OVERRIDE { return length_; }
  virtual int64_t getWrittenLength() const CXX11_OVERRIDE
  {
    return writtenLength_;
  }

private:
  int64_t length_;
  int64_t writtenLength_;
};
} // namespace

void DownloadCommandTest::testCreateUnexpectedEOFMessage()
{
  CPPUNIT_ASSERT_EQUAL(
      std::string(EX_GOT_EOF),
      createUnexpectedEOFMessage(std::make_shared<EOFMessageSegment>(0, 0)));

  CPPUNIT_ASSERT_EQUAL(
      std::string("Got EOF from the server before completing response body "
                  "(128/1024 bytes received)."),
      createUnexpectedEOFMessage(
          std::make_shared<EOFMessageSegment>(1024, 128)));
}

} // namespace aria2
