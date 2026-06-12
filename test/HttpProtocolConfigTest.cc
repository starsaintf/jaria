#include "HttpProtocolConfig.h"

#include <cstring>
#include <getopt.h>
#include <sstream>
#include <vector>

#include <cppunit/extensions/HelperMacros.h>

#include "Option.h"
#include "OptionHandlerFactory.h"
#include "OptionParser.h"
#include "prefs.h"

namespace aria2 {

class HttpProtocolConfigTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(HttpProtocolConfigTest);
  CPPUNIT_TEST(testGetTLSApplicationProtocols);
  CPPUNIT_TEST(testEnableHTTP2OptionIsParseable);
  CPPUNIT_TEST_SUITE_END();

public:
  void testGetTLSApplicationProtocols();
  void testEnableHTTP2OptionIsParseable();
};

CPPUNIT_TEST_SUITE_REGISTRATION(HttpProtocolConfigTest);

void HttpProtocolConfigTest::testGetTLSApplicationProtocols()
{
  Option option;
  option.put(PREF_ENABLE_HTTP2, "false");

  auto protocols = getHTTPApplicationProtocols(&option);
  CPPUNIT_ASSERT_EQUAL((size_t)1, protocols.size());
  CPPUNIT_ASSERT_EQUAL(std::string("http/1.1"), protocols[0]);

  option.put(PREF_ENABLE_HTTP2, "true");
  protocols = getHTTPApplicationProtocols(&option);

#if defined(ENABLE_SSL) && defined(HAVE_LIBNGHTTP2)
  CPPUNIT_ASSERT_EQUAL((size_t)2, protocols.size());
  CPPUNIT_ASSERT_EQUAL(std::string("h2"), protocols[0]);
  CPPUNIT_ASSERT_EQUAL(std::string("http/1.1"), protocols[1]);
#else  // !(defined(ENABLE_SSL) && defined(HAVE_LIBNGHTTP2))
  CPPUNIT_ASSERT_EQUAL((size_t)1, protocols.size());
  CPPUNIT_ASSERT_EQUAL(std::string("http/1.1"), protocols[0]);
#endif // !(defined(ENABLE_SSL) && defined(HAVE_LIBNGHTTP2))
}

void HttpProtocolConfigTest::testEnableHTTP2OptionIsParseable()
{
  OptionParser parser;
  parser.setOptionHandlers(OptionHandlerFactory::createOptionHandlers());

  char prog[7];
  strncpy(prog, "aria2c", sizeof(prog));
  char optionHTTP2[20];
  strncpy(optionHTTP2, "--enable-http2=true", sizeof(optionHTTP2));
  char* argv[] = {prog, optionHTTP2};
  int argc = 2;

  std::stringstream out;
  std::vector<std::string> nonopts;
  optind = 1;
  parser.parseArg(out, nonopts, argc, argv);
  optind = 1;

  CPPUNIT_ASSERT_EQUAL(std::string("enable-http2=true\n"), out.str());
  CPPUNIT_ASSERT(nonopts.empty());
}

} // namespace aria2
