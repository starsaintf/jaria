#include "TLSSession.h"

#include <cppunit/extensions/HelperMacros.h>

#ifdef HAVE_OPENSSL
#  include "File.h"
#  include "LibsslTLSContext.h"
#  include "TestUtil.h"
#endif // HAVE_OPENSSL

namespace aria2 {

class TLSSessionTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(TLSSessionTest);
  CPPUNIT_TEST(testApplicationProtocolDefaults);
  CPPUNIT_TEST(testEncodeApplicationProtocols);
#ifdef HAVE_OPENSSL
  CPPUNIT_TEST(testFindOpenSSLSystemCAFile);
#endif // HAVE_OPENSSL
  CPPUNIT_TEST_SUITE_END();

public:
  void testApplicationProtocolDefaults();
  void testEncodeApplicationProtocols();
#ifdef HAVE_OPENSSL
  void testFindOpenSSLSystemCAFile();
#endif // HAVE_OPENSSL
};

CPPUNIT_TEST_SUITE_REGISTRATION(TLSSessionTest);

namespace {
class MockTLSSession : public TLSSession {
public:
  virtual int init(sock_t sockfd) CXX11_OVERRIDE { return TLS_ERR_OK; }
  virtual int setSNIHostname(const std::string& hostname) CXX11_OVERRIDE
  {
    return TLS_ERR_OK;
  }
  virtual int closeConnection() CXX11_OVERRIDE { return TLS_ERR_OK; }
  virtual int checkDirection() CXX11_OVERRIDE { return TLS_WANT_READ; }
  virtual ssize_t writeData(const void* data, size_t len) CXX11_OVERRIDE
  {
    return 0;
  }
  virtual ssize_t readData(void* data, size_t len) CXX11_OVERRIDE { return 0; }
  virtual int tlsConnect(const std::string& hostname, TLSVersion& version,
                         std::string& handshakeErr) CXX11_OVERRIDE
  {
    return TLS_ERR_OK;
  }
  virtual int tlsAccept(TLSVersion& version) CXX11_OVERRIDE
  {
    return TLS_ERR_OK;
  }
  virtual std::string getLastErrorString() CXX11_OVERRIDE { return ""; }
  virtual size_t getRecvBufferedLength() CXX11_OVERRIDE { return 0; }
};
} // namespace

void TLSSessionTest::testApplicationProtocolDefaults()
{
  MockTLSSession session;
  std::vector<std::string> protocols;
  protocols.push_back("h2");
  protocols.push_back("http/1.1");

  CPPUNIT_ASSERT_EQUAL(static_cast<int>(TLS_ERR_OK),
                       session.setApplicationProtocols(protocols));
  CPPUNIT_ASSERT_EQUAL(std::string(""),
                       session.getNegotiatedApplicationProtocol());
}

void TLSSessionTest::testEncodeApplicationProtocols()
{
  std::vector<std::string> protocols;
  protocols.push_back("h2");
  protocols.push_back("http/1.1");

  auto encoded = encodeTLSApplicationProtocols(protocols);

  CPPUNIT_ASSERT_EQUAL((size_t)12, encoded.size());
  CPPUNIT_ASSERT_EQUAL((unsigned char)2, encoded[0]);
  CPPUNIT_ASSERT_EQUAL((unsigned char)'h', encoded[1]);
  CPPUNIT_ASSERT_EQUAL((unsigned char)'2', encoded[2]);
  CPPUNIT_ASSERT_EQUAL((unsigned char)8, encoded[3]);
  CPPUNIT_ASSERT_EQUAL((unsigned char)'h', encoded[4]);
  CPPUNIT_ASSERT_EQUAL((unsigned char)'t', encoded[5]);
  CPPUNIT_ASSERT_EQUAL((unsigned char)'t', encoded[6]);
  CPPUNIT_ASSERT_EQUAL((unsigned char)'p', encoded[7]);
  CPPUNIT_ASSERT_EQUAL((unsigned char)'/', encoded[8]);
  CPPUNIT_ASSERT_EQUAL((unsigned char)'1', encoded[9]);
  CPPUNIT_ASSERT_EQUAL((unsigned char)'.', encoded[10]);
  CPPUNIT_ASSERT_EQUAL((unsigned char)'1', encoded[11]);
}

#ifdef HAVE_OPENSSL
void TLSSessionTest::testFindOpenSSLSystemCAFile()
{
  std::string caFile = A2_TEST_OUT_DIR "/aria2_TLSSessionTest_ca_bundle.pem";
  File(caFile).remove();
  createFile(caFile, 1);

  std::vector<std::string> candidates;
  candidates.push_back(A2_TEST_OUT_DIR "/aria2_TLSSessionTest_missing.pem");
  candidates.push_back(A2_TEST_OUT_DIR);
  candidates.push_back(caFile);

  CPPUNIT_ASSERT_EQUAL(caFile, findOpenSSLSystemCAFile(candidates));

  File(caFile).remove();
}
#endif // HAVE_OPENSSL

} // namespace aria2
