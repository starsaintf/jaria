#include "Http2Connection.h"

#include <memory>

#include <cppunit/extensions/HelperMacros.h>

#include "AuthConfigFactory.h"
#include "FileEntry.h"
#include "HttpHeader.h"
#include "HttpRequest.h"
#include "Option.h"
#include "Request.h"
#include "prefs.h"

namespace aria2 {

class Http2ConnectionTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(Http2ConnectionTest);
  CPPUNIT_TEST(testCreateHTTP2HeaderFields);
  CPPUNIT_TEST(testApplyHTTP2ResponseHeader);
  CPPUNIT_TEST(testShouldWaitForHTTP2FinalResponseHeader);
  CPPUNIT_TEST(testHTTP2BodyBufferReadsAcrossChunks);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp();
  void testCreateHTTP2HeaderFields();
  void testApplyHTTP2ResponseHeader();
  void testShouldWaitForHTTP2FinalResponseHeader();
  void testHTTP2BodyBufferReadsAcrossChunks();

private:
  std::unique_ptr<Option> option_;
  std::unique_ptr<AuthConfigFactory> authConfigFactory_;
};

CPPUNIT_TEST_SUITE_REGISTRATION(Http2ConnectionTest);

void Http2ConnectionTest::setUp()
{
  option_.reset(new Option());
  option_->put(PREF_HTTP_AUTH_CHALLENGE, A2_V_TRUE);
  authConfigFactory_.reset(new AuthConfigFactory());
}

void Http2ConnectionTest::testCreateHTTP2HeaderFields()
{
  auto request = std::make_shared<Request>();
  request->setUri("https://example.org:8443/download/file.txt?x=1");
  request->supportsPersistentConnection(false);

  auto fileEntry = std::make_shared<FileEntry>("file", 0, 0);

  HttpRequest httpRequest;
  httpRequest.disableContentEncoding();
  httpRequest.setRequest(request);
  httpRequest.setFileEntry(fileEntry);
  httpRequest.setAuthConfigFactory(authConfigFactory_.get());
  httpRequest.setOption(option_.get());
  httpRequest.setNoWantDigest(true);
  httpRequest.addHeader("Connection: close\n"
                        "Upgrade: websocket\n"
                        "Want-Digest: SHA-256;q=1\n"
                        "X-Test: one");

  auto headers = createHTTP2HeaderFields(&httpRequest);

  CPPUNIT_ASSERT_EQUAL(std::string(":method"), headers[0].name);
  CPPUNIT_ASSERT_EQUAL(std::string("GET"), headers[0].value);
  CPPUNIT_ASSERT_EQUAL(std::string(":scheme"), headers[1].name);
  CPPUNIT_ASSERT_EQUAL(std::string("https"), headers[1].value);
  CPPUNIT_ASSERT_EQUAL(std::string(":authority"), headers[2].name);
  CPPUNIT_ASSERT_EQUAL(std::string("example.org:8443"), headers[2].value);
  CPPUNIT_ASSERT_EQUAL(std::string(":path"), headers[3].name);
  CPPUNIT_ASSERT_EQUAL(std::string("/download/file.txt?x=1"),
                       headers[3].value);

  bool sawConnection = false;
  bool sawUpgrade = false;
  bool sawWantDigest = false;
  bool sawCustomHeader = false;
  for (const auto& header : headers) {
    sawConnection |= header.name == "connection";
    sawUpgrade |= header.name == "upgrade";
    sawWantDigest |= header.name == "want-digest";
    sawCustomHeader |= header.name == "x-test" && header.value == "one";
  }
  CPPUNIT_ASSERT(!sawConnection);
  CPPUNIT_ASSERT(!sawUpgrade);
  CPPUNIT_ASSERT(!sawWantDigest);
  CPPUNIT_ASSERT(sawCustomHeader);
}

void Http2ConnectionTest::testApplyHTTP2ResponseHeader()
{
  HttpHeader header;
  applyHTTP2ResponseHeader(&header, ":status", "206");
  applyHTTP2ResponseHeader(&header, "content-length", "10");
  applyHTTP2ResponseHeader(&header, "content-range", "bytes 5-14/20");
  applyHTTP2ResponseHeader(&header, "connection", "close");

  CPPUNIT_ASSERT_EQUAL(206, header.getStatusCode());
  CPPUNIT_ASSERT_EQUAL(std::string("HTTP/2"), header.getVersion());
  CPPUNIT_ASSERT_EQUAL(std::string("10"),
                       header.find(HttpHeader::CONTENT_LENGTH));
  CPPUNIT_ASSERT_EQUAL(std::string("bytes 5-14/20"),
                       header.find(HttpHeader::CONTENT_RANGE));
  CPPUNIT_ASSERT(!header.defined(HttpHeader::CONNECTION));
}

void Http2ConnectionTest::testShouldWaitForHTTP2FinalResponseHeader()
{
  HttpHeader header;
  header.setVersion("HTTP/2");
  header.setStatusCode(100);

  CPPUNIT_ASSERT(shouldWaitForHTTP2FinalResponseHeader(&header));

  header.setStatusCode(103);

  CPPUNIT_ASSERT(shouldWaitForHTTP2FinalResponseHeader(&header));

  header.setStatusCode(200);

  CPPUNIT_ASSERT(!shouldWaitForHTTP2FinalResponseHeader(&header));

  header.setStatusCode(204);

  CPPUNIT_ASSERT(!shouldWaitForHTTP2FinalResponseHeader(&header));
}

void Http2ConnectionTest::testHTTP2BodyBufferReadsAcrossChunks()
{
  Http2BodyBuffer buffer;
  const unsigned char first[] = {'a', 'b', 'c'};
  const unsigned char second[] = {'d', 'e'};
  buffer.append(first, sizeof(first));
  buffer.append(second, sizeof(second));

  unsigned char out[4];
  CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(4), buffer.pop(out, sizeof(out)));
  CPPUNIT_ASSERT_EQUAL(std::string("abcd"),
                       std::string(reinterpret_cast<char*>(out), 4));
  CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), buffer.size());

  CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), buffer.pop(out, sizeof(out)));
  CPPUNIT_ASSERT_EQUAL(static_cast<unsigned char>('e'), out[0]);
  CPPUNIT_ASSERT(buffer.empty());
}

} // namespace aria2
