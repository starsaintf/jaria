#include "DownloadEngine.h"

#include <cppunit/extensions/HelperMacros.h>

#include "Request.h"
#include "SelectEventPoll.h"

namespace aria2 {

class DownloadEngineTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(DownloadEngineTest);
  CPPUNIT_TEST(testHTTP2OriginDisableIsScopedToHTTPSOrigin);
  CPPUNIT_TEST(testHTTP3OriginDisableIsScopedToHTTPSOrigin);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp();
  void testHTTP2OriginDisableIsScopedToHTTPSOrigin();
  void testHTTP3OriginDisableIsScopedToHTTPSOrigin();

private:
  std::unique_ptr<DownloadEngine> e_;
};

CPPUNIT_TEST_SUITE_REGISTRATION(DownloadEngineTest);

void DownloadEngineTest::setUp()
{
  e_ = make_unique<DownloadEngine>(make_unique<SelectEventPoll>());
}

void DownloadEngineTest::testHTTP2OriginDisableIsScopedToHTTPSOrigin()
{
  auto resetReq = std::make_shared<Request>();
  CPPUNIT_ASSERT(resetReq->setUri("https://example.org:8443/a"));

  auto sameOrigin = std::make_shared<Request>();
  CPPUNIT_ASSERT(sameOrigin->setUri("https://example.org:8443/b"));

  auto differentPort = std::make_shared<Request>();
  CPPUNIT_ASSERT(differentPort->setUri("https://example.org:9443/a"));

  auto differentHost = std::make_shared<Request>();
  CPPUNIT_ASSERT(differentHost->setUri("https://cdn.example.org:8443/a"));

  auto cleartext = std::make_shared<Request>();
  CPPUNIT_ASSERT(cleartext->setUri("http://example.org:8443/a"));

  CPPUNIT_ASSERT(!e_->isHTTP2DisabledForOrigin(sameOrigin.get()));

  e_->disableHTTP2ForOrigin(resetReq.get());

  CPPUNIT_ASSERT(e_->isHTTP2DisabledForOrigin(resetReq.get()));
  CPPUNIT_ASSERT(e_->isHTTP2DisabledForOrigin(sameOrigin.get()));
  CPPUNIT_ASSERT(!e_->isHTTP2DisabledForOrigin(differentPort.get()));
  CPPUNIT_ASSERT(!e_->isHTTP2DisabledForOrigin(differentHost.get()));
  CPPUNIT_ASSERT(!e_->isHTTP2DisabledForOrigin(cleartext.get()));
}

void DownloadEngineTest::testHTTP3OriginDisableIsScopedToHTTPSOrigin()
{
  auto resetReq = std::make_shared<Request>();
  CPPUNIT_ASSERT(resetReq->setUri("https://example.org:8443/a"));

  auto sameOrigin = std::make_shared<Request>();
  CPPUNIT_ASSERT(sameOrigin->setUri("https://example.org:8443/b"));

  auto differentPort = std::make_shared<Request>();
  CPPUNIT_ASSERT(differentPort->setUri("https://example.org:9443/a"));

  auto differentHost = std::make_shared<Request>();
  CPPUNIT_ASSERT(differentHost->setUri("https://cdn.example.org:8443/a"));

  auto cleartext = std::make_shared<Request>();
  CPPUNIT_ASSERT(cleartext->setUri("http://example.org:8443/a"));

  CPPUNIT_ASSERT(!e_->isHTTP3DisabledForOrigin(sameOrigin.get()));

  e_->disableHTTP3ForOrigin(resetReq.get());

  CPPUNIT_ASSERT(e_->isHTTP3DisabledForOrigin(resetReq.get()));
  CPPUNIT_ASSERT(e_->isHTTP3DisabledForOrigin(sameOrigin.get()));
  CPPUNIT_ASSERT(!e_->isHTTP3DisabledForOrigin(differentPort.get()));
  CPPUNIT_ASSERT(!e_->isHTTP3DisabledForOrigin(differentHost.get()));
  CPPUNIT_ASSERT(!e_->isHTTP3DisabledForOrigin(cleartext.get()));
}

} // namespace aria2
