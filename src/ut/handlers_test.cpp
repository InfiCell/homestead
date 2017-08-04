/**
 * @file handlers_test.cpp UT for Handlers module.
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

// IMPORTANT for developers.
//
// The test cases in this file use both a real Diameter::Stack and a
// MockDiameterStack. We use the mock stack to catch diameter messages
// as the handlers send them out, and we use the real stack for
// everything else. This makes it difficult to keep track of who owns the
// underlying fd_msg structures and therefore who is responsible for freeing them.
//
// For tests where the handlers initiate the session by sending a request, we have
// to be careful that the request is freed after we catch it. This is sometimes done
// by simply calling fd_msg_free. However sometimes we want to look at the message and
// so we turn it back into a Cx message. This will trigger the caught fd_msg to be
// freed when we are finished with the Cx message.
//
// For tests where we initiate the session by sending in a request, we have to be
// careful that the request is only freed once. This can be an issue because the
// handlers build an answer from the request which references the request, and
// freeDiameter will then try to free the request when it frees the answer. We need
// to make sure that the request has not already been freed.

#define GTEST_HAS_POSIX_RE 0
#include "test_utils.hpp"
#include "test_interposer.hpp"
#include "fakelogger.h"
#include <curl/curl.h>

#include "httpstack_utils.h"

#include "mockdiameterstack.hpp"
#include "mockhttpstack.hpp"
#include "mockcache.hpp"
#include "mockhttpconnection.hpp"
#include "fakehttpresolver.hpp"
#include "handlers.h"
#include "mockstatisticsmanager.hpp"
#include "sproutconnection.h"
#include "mock_health_checker.hpp"
#include "fakesnmp.hpp"
#include "base64.h"

//#include "fakehssconnection.hpp"
#include "mockhssconnection.hpp"
#include "mockhsscacheprocessor.hpp"

using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SetArgReferee;
using ::testing::_;
using ::testing::Invoke;
using ::testing::InvokeArgument;
using ::testing::WithArgs;
using ::testing::NiceMock;
using ::testing::StrictMock;
using ::testing::Mock;
using ::testing::AtLeast;
using ::testing::Field;
using ::testing::AllOf;

const SAS::TrailId FAKE_TRAIL_ID = 0x12345678;

// Fixture for HandlersTest.
class HandlersTest : public testing::Test
{
public:
  static const std::string DEST_REALM;
  static const std::string DEST_HOST;
  static const std::string DEFAULT_SERVER_NAME;
  static const std::string PROVIDED_SERVER_NAME;
  static const std::string SERVER_NAME;
  static const std::string WILDCARD;
  static const std::string NEW_WILDCARD;
  static const std::string IMPI;
  static const std::string IMPU;
  static std::vector<std::string> IMPU_IN_VECTOR;
  static std::vector<std::string> IMPU2_IN_VECTOR;
  static std::vector<std::string> IMPU3_IN_VECTOR;
  static std::vector<std::string> IMPI_IN_VECTOR;
  static const std::string IMS_SUBSCRIPTION;
  static const std::string REGDATA_RESULT;
  static const std::string REGDATA_RESULT_INCLUDES_BARRING;
  static const std::string REGDATA_RESULT_UNREG;
  static const std::string REGDATA_RESULT_DEREG;
  static const std::string REGDATA_BLANK_RESULT_DEREG;
  static const std::string VISITED_NETWORK;
  static const std::string AUTH_TYPE_DEREG;
  static const std::string AUTH_TYPE_CAPAB;
  static const ServerCapabilities CAPABILITIES;
  static const ServerCapabilities NO_CAPABILITIES;
  static const ServerCapabilities CAPABILITIES_WITH_SERVER_NAME;
  static const int32_t AUTH_SESSION_STATE;
  static const std::string ASSOCIATED_IDENTITY1;
  static const std::string ASSOCIATED_IDENTITY2;
  static std::vector<std::string> ASSOCIATED_IDENTITIES;
  static const std::string IMPU2;
  static const std::string IMPU3;
  static const std::string IMPU4;
  static const std::string IMPU5;
  static const std::string IMPU6;
  static std::vector<std::string> IMPU_TEST;
  static std::vector<std::string> IMPUS;
  static std::vector<std::string> IMPU_LIST;
  static std::vector<std::string> THREE_DEFAULT_IMPUS;
  static std::vector<std::string> THREE_DEFAULT_IMPUS2;
  static std::vector<std::string> ASSOCIATED_IDENTITY1_IN_VECTOR;
  static std::vector<std::string> IMPU_REG_SET;
  static std::vector<std::string> IMPU_REG_SET2;
  static std::vector<std::string> IMPU3_REG_SET;
  static std::vector<std::string> IMPU5_REG_SET;
  static const std::string IMPU_IMS_SUBSCRIPTION;
  static const std::string IMPU_IMS_SUBSCRIPTION2;
  static const std::string IMPU_IMS_SUBSCRIPTION_INVALID;
  static const std::string IMPU3_IMS_SUBSCRIPTION;
  static const std::string IMPU5_IMS_SUBSCRIPTION;
  static const std::string IMPU_IMS_SUBSCRIPTION_WITH_BARRING;
  static const std::string IMPU_IMS_SUBSCRIPTION_WITH_BARRING2;
  static const std::string IMPU_IMS_SUBSCRIPTION_WITH_BARRING3;
  static const std::string IMPU_IMS_SUBSCRIPTION_BARRING_INDICATION;
  static const std::string SCHEME_UNKNOWN;
  static const std::string SCHEME_DIGEST;
  static const std::string SCHEME_AKA;
  static const std::string SCHEME_AKAV2;
  static const std::string SIP_AUTHORIZATION;
  static const std::string HTTP_PATH_REG_TRUE;
  static const std::string HTTP_PATH_REG_FALSE;
  static std::vector<std::string> EMPTY_VECTOR;
  static const std::string DEREG_BODY_PAIRINGS;
  static const std::string DEREG_BODY_PAIRINGS2;
  static const std::string DEREG_BODY_PAIRINGS3;
  static const std::string DEREG_BODY_PAIRINGS4;
  static const std::string DEREG_BODY_PAIRINGS5;
  static const std::string DEREG_BODY_LIST;
  static const std::string DEREG_BODY_LIST2;
  static const std::string DEREG_BODY_LIST3;
  static const std::deque<std::string> NO_CFS;
  static const std::deque<std::string> CCFS;
  static const std::deque<std::string> ECFS;
  static const ChargingAddresses NO_CHARGING_ADDRESSES;
  static const ChargingAddresses FULL_CHARGING_ADDRESSES;
  static const std::string TEL_URI;
  static const std::string TEL_URI2;
  static const std::string TEL_URIS_IMS_SUBSCRIPTION;
  static const std::string TEL_URIS_IMS_SUBSCRIPTION_WITH_BARRING;
  static std::vector<std::string> TEL_URIS_IN_VECTOR;


  static HttpResolver* _mock_resolver;
  static MockHssCacheProcessor* _cache;
  static MockHttpStack* _httpstack;
  static MockHttpConnection* _mock_http_conn;
  static SproutConnection* _sprout_conn;
  //static FakeHssConnection* _hss;
  static MockHssConnection* _hss;


  // Two mock stats managers, so we can choose whether to ignore stats or not.
  static NiceMock<MockStatisticsManager>* _nice_stats;
  static StrictMock<MockStatisticsManager>* _stats;

  static SNMP::CxCounterTable* _mar_results_table;
  static SNMP::CxCounterTable* _sar_results_table;
  static SNMP::CxCounterTable* _uar_results_table;
  static SNMP::CxCounterTable* _lir_results_table;
  static SNMP::CxCounterTable* _ppr_results_table;
  static SNMP::CxCounterTable* _rtr_results_table;

  // Used to catch diameter messages and transactions on the MockDiameterStack
  // so that we can inspect them.
  static struct msg* _caught_fd_msg;
  static Diameter::Transaction* _caught_diam_tsx;

  std::string test_str;
  int32_t test_i32;
  uint32_t test_u32;

  HandlersTest() {}
  virtual ~HandlersTest()
  {
    Mock::VerifyAndClear(_httpstack);
  }

  static void SetUpTestCase()
  {
    _cache = new MockHssCacheProcessor();
    _hss = new MockHssConnection();
    //_hss = new FakeHssConnection();
    _httpstack = new MockHttpStack();
    _mock_resolver = new FakeHttpResolver("1.2.3.4");
    _mock_http_conn = new MockHttpConnection(_mock_resolver);
    _sprout_conn = new SproutConnection(_mock_http_conn);

    HssCacheTask::configure_hss_connection(_hss, DEFAULT_SERVER_NAME);
    HssCacheTask::configure_cache(_cache);
  
    cwtest_completely_control_time();
  }

  static void TearDownTestCase()
  {
    cwtest_reset_time();

    delete _cache; _cache = NULL;
    delete _hss; _hss = NULL;
    delete _httpstack; _httpstack = NULL;
    delete _sprout_conn; _sprout_conn = NULL;
    delete _mock_resolver; _mock_resolver = NULL;
  }

   // Helper functions to build the expected json responses in our tests.
  static std::string build_digest_json(DigestAuthVector digest)
  {
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    writer.StartObject();
    writer.String(JSON_DIGEST_HA1.c_str());
    writer.String(digest.ha1.c_str());
    writer.EndObject();
    return sb.GetString();
  }

  static std::string build_av_json(DigestAuthVector av)
  {
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    // The qop value can be empty - in this case it should be replaced
    // with 'auth'.
    std::string qop_value = (!av.qop.empty()) ? av.qop : JSON_AUTH;

    writer.StartObject();
    {
      writer.String(JSON_DIGEST.c_str());
      writer.StartObject();
      {
        writer.String(JSON_HA1.c_str());
        writer.String(av.ha1.c_str());
        writer.String(JSON_REALM.c_str());
        writer.String(av.realm.c_str());
        writer.String(JSON_QOP.c_str());
        writer.String(qop_value.c_str());
      }
      writer.EndObject();
    }
    writer.EndObject();
    return sb.GetString();
  }

  static std::string build_aka_json(AKAAuthVector av)
  {
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();
    {
      writer.String(JSON_AKA.c_str());
      writer.StartObject();
      {
        writer.String(JSON_CHALLENGE.c_str());
        writer.String(av.challenge.c_str());
        writer.String(JSON_RESPONSE.c_str());
        writer.String(av.response.c_str());
        writer.String(JSON_CRYPTKEY.c_str());
        writer.String(av.crypt_key.c_str());
        writer.String(JSON_INTEGRITYKEY.c_str());
        writer.String(av.integrity_key.c_str());
        writer.String(JSON_VERSION.c_str());
        writer.Int(av.version);
      }
      writer.EndObject();
    }
    writer.EndObject();
    return sb.GetString();
  }

  static std::string build_icscf_json(int32_t rc,
                                      std::string scscf,
                                      ServerCapabilities capabs,
                                      std::string wildcard)
  {
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    writer.StartObject();
    writer.String(JSON_RC.c_str());
    writer.Int(rc);
    if (!scscf.empty())
    {
      writer.String(JSON_SCSCF.c_str());
      writer.String(scscf.c_str());
    }
    else
    {
      if (!capabs.server_name.empty())
      {
        writer.String(JSON_SCSCF.c_str());
        writer.String(capabs.server_name.c_str());
      }
      writer.String(JSON_MAN_CAP.c_str());
      writer.StartArray();
      if (!capabs.mandatory_capabilities.empty())
      {
        for (std::vector<int>::const_iterator it = capabs.mandatory_capabilities.begin();
             it != capabs.mandatory_capabilities.end();
             ++it)
        {
          writer.Int(*it);
        }
      }
      writer.EndArray();
      writer.String(JSON_OPT_CAP.c_str());
      writer.StartArray();
      if (!capabs.optional_capabilities.empty())
      {
        for (std::vector<int>::const_iterator it = capabs.optional_capabilities.begin();
            it != capabs.optional_capabilities.end();
            ++it)
        {
          writer.Int(*it);
        }
      }
      writer.EndArray();
    }

    if (!wildcard.empty())
    {
      writer.String(JSON_WILDCARD.c_str());
      writer.String(wildcard.c_str());
    }

    writer.EndObject();
    return sb.GetString();
  }

  MockHttpStack::Request make_request(const std::string& req_type,
                                      bool use_impi,
                                      bool use_server_name,
                                      bool use_wildcard)
  {
    std::string parameters = "";
    std::string server_name = "";
    std::string wildcard = "";

    if (use_impi)
    {
      parameters = "?private_id=" + IMPI;
    }

    if (use_server_name)
    {
      server_name = ", \"server_name\": \"" + SERVER_NAME + "\"";
    }

    if (use_wildcard)
    {
      wildcard = ", \"wildcard_identity\": \"" + WILDCARD + "\"";
    }

    return MockHttpStack::Request(_httpstack,
                                  "/impu/" + IMPU + "/reg-data",
                                  "",
                                  parameters,
                                  "{\"reqtype\": \"" + req_type +"\"" + server_name + wildcard + "}",
                                  htp_method_PUT);
  }
};

const std::string HandlersTest::DEST_REALM = "dest-realm";
const std::string HandlersTest::DEST_HOST = "dest-host";
const std::string HandlersTest::DEFAULT_SERVER_NAME = "sprout";
const std::string HandlersTest::PROVIDED_SERVER_NAME = "sprout-site2";
const std::string HandlersTest::SERVER_NAME = "scscf";
const std::string HandlersTest::WILDCARD = "sip:im!.*!@scscf";
const std::string HandlersTest::NEW_WILDCARD = "sip:newim!.*!@scscf";
const std::string HandlersTest::IMPI = "_impi@example.com";
const std::string HandlersTest::IMPU = "sip:impu@example.com";
const std::string HandlersTest::IMPU2 = "sip:impu2@example.com";
const std::string HandlersTest::IMPU3 = "sip:impu3@example.com";
const std::string HandlersTest::IMPU4 = "sip:impu4@example.com";
const std::string HandlersTest::IMPU5 = "sip:impu5@example.com";
const std::string HandlersTest::IMPU6 = "sip:impu6@example.com";
const std::string HandlersTest::IMS_SUBSCRIPTION = "<?xml version=\"1.0\"?><IMSSubscription><PrivateID>" + IMPI + "</PrivateID><ServiceProfile><PublicIdentity><Identity>" + IMPU + "</Identity></PublicIdentity></ServiceProfile></IMSSubscription>";
const std::string HandlersTest::REGDATA_RESULT = "<ClearwaterRegData>\n\t<RegistrationState>REGISTERED</RegistrationState>\n\t<IMSSubscription>\n\t\t<PrivateID>" + IMPI + "</PrivateID>\n\t\t<ServiceProfile>\n\t\t\t<PublicIdentity>\n\t\t\t\t<Identity>" + IMPU + "</Identity>\n\t\t\t</PublicIdentity>\n\t\t\t<PublicIdentity>\n\t\t\t\t<Identity>" + IMPU4 + "</Identity>\n\t\t\t</PublicIdentity>\n\t\t</ServiceProfile>\n\t</IMSSubscription>\n</ClearwaterRegData>\n\n";
const std::string HandlersTest::REGDATA_RESULT_INCLUDES_BARRING = "<ClearwaterRegData>\n\t<RegistrationState>REGISTERED</RegistrationState>\n\t<IMSSubscription>\n\t\t<PrivateID>" + IMPI + "</PrivateID>\n\t\t<ServiceProfile>\n\t\t\t<PublicIdentity>\n\t\t\t\t<Identity>" + IMPU + "</Identity>\n\t\t\t\t<BarringIndication>1</BarringIndication>\n\t\t\t</PublicIdentity>\n\t\t\t<PublicIdentity>\n\t\t\t\t<Identity>" + IMPU2 + "</Identity>\n\t\t\t</PublicIdentity>\n\t\t</ServiceProfile>\n\t</IMSSubscription>\n</ClearwaterRegData>\n\n";
const std::string HandlersTest::REGDATA_RESULT_DEREG = "<ClearwaterRegData>\n\t<RegistrationState>NOT_REGISTERED</RegistrationState>\n\t<IMSSubscription>\n\t\t<PrivateID>" + IMPI + "</PrivateID>\n\t\t<ServiceProfile>\n\t\t\t<PublicIdentity>\n\t\t\t\t<Identity>" + IMPU + "</Identity>\n\t\t\t</PublicIdentity>\n\t\t\t<PublicIdentity>\n\t\t\t\t<Identity>" + IMPU4 + "</Identity>\n\t\t\t</PublicIdentity>\n\t\t</ServiceProfile>\n\t</IMSSubscription>\n</ClearwaterRegData>\n\n";
const std::string HandlersTest::REGDATA_BLANK_RESULT_DEREG = "<ClearwaterRegData>\n\t<RegistrationState>NOT_REGISTERED</RegistrationState>\n</ClearwaterRegData>\n\n";
const std::string HandlersTest::REGDATA_RESULT_UNREG = "<ClearwaterRegData>\n\t<RegistrationState>UNREGISTERED</RegistrationState>\n\t<IMSSubscription>\n\t\t<PrivateID>" + IMPI + "</PrivateID>\n\t\t<ServiceProfile>\n\t\t\t<PublicIdentity>\n\t\t\t\t<Identity>" + IMPU + "</Identity>\n\t\t\t</PublicIdentity>\n\t\t\t<PublicIdentity>\n\t\t\t\t<Identity>" + IMPU4 + "</Identity>\n\t\t\t</PublicIdentity>\n\t\t</ServiceProfile>\n\t</IMSSubscription>\n</ClearwaterRegData>\n\n";
const std::string HandlersTest::VISITED_NETWORK = "visited-network.com";
const std::string HandlersTest::AUTH_TYPE_DEREG = "DEREG";
const std::string HandlersTest::AUTH_TYPE_CAPAB = "CAPAB";
const std::vector<int32_t> mandatory_capabilities = {1, 3};
const std::vector<int32_t> optional_capabilities = {2, 4};
const std::vector<int32_t> no_capabilities = {};
const ServerCapabilities HandlersTest::CAPABILITIES(mandatory_capabilities, optional_capabilities, "");
const ServerCapabilities HandlersTest::NO_CAPABILITIES(no_capabilities, no_capabilities, "");
const ServerCapabilities HandlersTest::CAPABILITIES_WITH_SERVER_NAME(no_capabilities, no_capabilities, SERVER_NAME);
const int32_t HandlersTest::AUTH_SESSION_STATE = 1;
const std::string HandlersTest::ASSOCIATED_IDENTITY1 = "associated_identity1@example.com";
const std::string HandlersTest::ASSOCIATED_IDENTITY2 = "associated_identity2@example.com";
std::vector<std::string> HandlersTest::ASSOCIATED_IDENTITIES = {ASSOCIATED_IDENTITY1, ASSOCIATED_IDENTITY2};
std::vector<std::string> HandlersTest::IMPU_TEST = {IMPU};
std::vector<std::string> HandlersTest::IMPUS = {IMPU, IMPU2};
std::vector<std::string> HandlersTest::IMPU_LIST = {IMPU2, IMPU};
std::vector<std::string> HandlersTest::THREE_DEFAULT_IMPUS = {IMPU, IMPU2, IMPU3};
std::vector<std::string> HandlersTest::THREE_DEFAULT_IMPUS2 = {IMPU, IMPU3, IMPU5};
std::vector<std::string> HandlersTest::IMPU_IN_VECTOR = {IMPU};
std::vector<std::string> HandlersTest::IMPU2_IN_VECTOR = {IMPU2};
std::vector<std::string> HandlersTest::IMPU3_IN_VECTOR = {IMPU3};
std::vector<std::string> HandlersTest::IMPI_IN_VECTOR = {IMPI};
std::vector<std::string> HandlersTest::ASSOCIATED_IDENTITY1_IN_VECTOR = {ASSOCIATED_IDENTITY1};
std::vector<std::string> HandlersTest::IMPU_REG_SET = {IMPU, IMPU4};
std::vector<std::string> HandlersTest::IMPU_REG_SET2 = {IMPU, IMPU2};
std::vector<std::string> HandlersTest::IMPU3_REG_SET = {IMPU3, IMPU2};
std::vector<std::string> HandlersTest::IMPU5_REG_SET = {IMPU5, IMPU6};
const std::string HandlersTest::IMPU_IMS_SUBSCRIPTION = "<?xml version=\"1.0\"?><IMSSubscription><PrivateID>" + IMPI + "</PrivateID><ServiceProfile><PublicIdentity><Identity>" + IMPU + "</Identity></PublicIdentity><PublicIdentity><Identity>" + IMPU4 + "</Identity></PublicIdentity></ServiceProfile></IMSSubscription>";
const std::string HandlersTest::IMPU_IMS_SUBSCRIPTION2 = "<?xml version=\"1.0\"?><IMSSubscription><PrivateID>" + IMPI + "</PrivateID><ServiceProfile><PublicIdentity><Identity>" + IMPU + "</Identity></PublicIdentity><PublicIdentity><Identity>" + IMPU2 + "</Identity></PublicIdentity></ServiceProfile></IMSSubscription>";
const std::string HandlersTest::IMPU_IMS_SUBSCRIPTION_INVALID = "<?xml version=\"1.0\"?><IMSSubscriptio></IMSSubscriptio>";
const std::string HandlersTest::IMPU3_IMS_SUBSCRIPTION = "<?xml version=\"1.0\"?><IMSSubscription><PrivateID>" + IMPI + "</PrivateID><ServiceProfile><PublicIdentity><Identity>" + IMPU3 + "</Identity></PublicIdentity><PublicIdentity><Identity>" + IMPU2 + "</Identity></PublicIdentity></ServiceProfile></IMSSubscription>";
const std::string HandlersTest::IMPU5_IMS_SUBSCRIPTION = "<?xml version=\"1.0\"?><IMSSubscription><PrivateID>" + IMPI + "</PrivateID><ServiceProfile><PublicIdentity><Identity>" + IMPU5 + "</Identity></PublicIdentity><PublicIdentity><Identity>" + IMPU6 + "</Identity></PublicIdentity></ServiceProfile></IMSSubscription>";
const std::string HandlersTest::IMPU_IMS_SUBSCRIPTION_WITH_BARRING = "<?xml version=\"1.0\"?><IMSSubscription><PrivateID>" + IMPI + "</PrivateID><ServiceProfile><PublicIdentity><Identity>" + IMPU + "</Identity><BarringIndication>1</BarringIndication></PublicIdentity><PublicIdentity><Identity>" + IMPU2 + "</Identity></PublicIdentity></ServiceProfile></IMSSubscription>";
const std::string HandlersTest::IMPU_IMS_SUBSCRIPTION_WITH_BARRING2 = "<?xml version=\"1.0\"?><IMSSubscription><PrivateID>" + IMPI + "</PrivateID><ServiceProfile><PublicIdentity><Identity>" + IMPU + "</Identity><BarringIndication>1</BarringIndication></PublicIdentity><PublicIdentity><Identity>" + IMPU4 + "</Identity></PublicIdentity></ServiceProfile></IMSSubscription>";
const std::string HandlersTest::IMPU_IMS_SUBSCRIPTION_WITH_BARRING3 = "<?xml version=\"1.0\"?><IMSSubscription><PrivateID>" + IMPI + "</PrivateID><ServiceProfile><PublicIdentity><Identity>" + IMPU + "</Identity></PublicIdentity><PublicIdentity><Identity>" + IMPU4 + "</Identity><BarringIndication>1</BarringIndication></PublicIdentity></ServiceProfile></IMSSubscription>";
const std::string HandlersTest::IMPU_IMS_SUBSCRIPTION_BARRING_INDICATION = "<?xml version=\"1.0\"?><IMSSubscription><PrivateID>" + IMPI + "</PrivateID><ServiceProfile><PublicIdentity><Identity>" + IMPU + "</Identity><BarringIndication>0</BarringIndication></PublicIdentity><PublicIdentity><Identity>" + IMPU2 + "</Identity></PublicIdentity></ServiceProfile></IMSSubscription>";
std::vector<std::string> HandlersTest::EMPTY_VECTOR = {};
const std::string HandlersTest::DEREG_BODY_PAIRINGS = "{\"registrations\":[{\"primary-impu\":\"" + IMPU3 + "\",\"impi\":\"" + IMPI +
                                                                      "\"},{\"primary-impu\":\"" + IMPU3 + "\",\"impi\":\"" + ASSOCIATED_IDENTITY1 +
                                                                      "\"},{\"primary-impu\":\"" + IMPU3 + "\",\"impi\":\"" + ASSOCIATED_IDENTITY2 +
                                                                      "\"},{\"primary-impu\":\"" + IMPU + "\",\"impi\":\"" + IMPI +
                                                                      "\"},{\"primary-impu\":\"" + IMPU + "\",\"impi\":\"" + ASSOCIATED_IDENTITY1 +
                                                                      "\"},{\"primary-impu\":\"" + IMPU + "\",\"impi\":\"" + ASSOCIATED_IDENTITY2 + "\"}]}";
const std::string HandlersTest::DEREG_BODY_LIST = "{\"registrations\":[{\"primary-impu\":\"" + IMPU3 + "\"},{\"primary-impu\":\"" + IMPU + "\"}]}";
// These are effectively the same as above, but depending on the exact code path the ordering of IMPUS can be different.
const std::string HandlersTest::DEREG_BODY_PAIRINGS2 = "{\"registrations\":[{\"primary-impu\":\"" + IMPU + "\",\"impi\":\"" + IMPI +
                                                                       "\"},{\"primary-impu\":\"" + IMPU + "\",\"impi\":\"" + ASSOCIATED_IDENTITY1 +
                                                                       "\"},{\"primary-impu\":\"" + IMPU + "\",\"impi\":\"" + ASSOCIATED_IDENTITY2 +
                                                                       "\"},{\"primary-impu\":\"" + IMPU3 + "\",\"impi\":\"" + IMPI +
                                                                       "\"},{\"primary-impu\":\"" + IMPU3 + "\",\"impi\":\"" + ASSOCIATED_IDENTITY1 +
                                                                       "\"},{\"primary-impu\":\"" + IMPU3 + "\",\"impi\":\"" + ASSOCIATED_IDENTITY2 + "\"}]}";
const std::string HandlersTest::DEREG_BODY_LIST2 = "{\"registrations\":[{\"primary-impu\":\"" + IMPU + "\"},{\"primary-impu\":\"" + IMPU3 + "\"}]}";
const std::string HandlersTest::SCHEME_UNKNOWN = "Unknwon";
const std::string HandlersTest::DEREG_BODY_PAIRINGS3 = "{\"registrations\":[{\"primary-impu\":\"" + IMPU2 + "\",\"impi\":\"" + IMPI +
                                                                       "\"},{\"primary-impu\":\"" + IMPU2 + "\",\"impi\":\"" + ASSOCIATED_IDENTITY1 +
                                                                       "\"},{\"primary-impu\":\"" + IMPU2 + "\",\"impi\":\"" + ASSOCIATED_IDENTITY2 + "\"}]}";
const std::string HandlersTest::DEREG_BODY_LIST3 = "{\"registrations\":[{\"primary-impu\":\"" + IMPU2 + "\"}]}";
const std::string HandlersTest::DEREG_BODY_PAIRINGS4 = "{\"registrations\":[{\"primary-impu\":\"" + IMPU + "\",\"impi\":\"" + IMPI +
                                                                       "\"},{\"primary-impu\":\"" + IMPU + "\",\"impi\":\"" + ASSOCIATED_IDENTITY1 +
                                                                       "\"},{\"primary-impu\":\"" + IMPU + "\",\"impi\":\"" + ASSOCIATED_IDENTITY2 + "\"}]}";
const std::string HandlersTest::DEREG_BODY_PAIRINGS5 = "{\"registrations\":[{\"primary-impu\":\"" + IMPU4 + "\",\"impi\":\"" + IMPI +
                                                                       "\"},{\"primary-impu\":\"" + IMPU4 + "\",\"impi\":\"" + ASSOCIATED_IDENTITY1 +
                                                                       "\"},{\"primary-impu\":\"" + IMPU4 + "\",\"impi\":\"" + ASSOCIATED_IDENTITY2 +
                                                                       "\"},{\"primary-impu\":\"" + IMPU3 + "\",\"impi\":\"" + IMPI +
                                                                       "\"},{\"primary-impu\":\"" + IMPU3 + "\",\"impi\":\"" + ASSOCIATED_IDENTITY1 +
                                                                       "\"},{\"primary-impu\":\"" + IMPU3 + "\",\"impi\":\"" + ASSOCIATED_IDENTITY2 + "\"}]}";
const std::string HandlersTest::SCHEME_DIGEST = "SIP Digest";
const std::string HandlersTest::SCHEME_AKA = "Digest-AKAv1-MD5";
const std::string HandlersTest::SCHEME_AKAV2 = "Digest-AKAv2-SHA-256";
const std::string HandlersTest::SIP_AUTHORIZATION = "Authorization";
const std::deque<std::string> HandlersTest::NO_CFS = {};
const std::deque<std::string> HandlersTest::ECFS = {"ecf1", "ecf"};
const std::deque<std::string> HandlersTest::CCFS = {"ccf1", "ccf2"};
const ChargingAddresses HandlersTest::NO_CHARGING_ADDRESSES(NO_CFS, NO_CFS);
const ChargingAddresses HandlersTest::FULL_CHARGING_ADDRESSES(CCFS, ECFS);
const std::string HandlersTest::TEL_URI = "tel:123";
const std::string HandlersTest::TEL_URI2 = "tel:321";
const std::string HandlersTest::TEL_URIS_IMS_SUBSCRIPTION = "<?xml version=\"1.0\"?><IMSSubscription><PrivateID>" + IMPI + "</PrivateID><ServiceProfile><PublicIdentity><Identity>" + TEL_URI + "</Identity></PublicIdentity><PublicIdentity><Identity>" + TEL_URI2 + "</Identity></PublicIdentity></ServiceProfile></IMSSubscription>";
const std::string HandlersTest::TEL_URIS_IMS_SUBSCRIPTION_WITH_BARRING = "<?xml version=\"1.0\"?><IMSSubscription><PrivateID>" + IMPI + "</PrivateID><ServiceProfile><PublicIdentity><Identity>" + TEL_URI + "</Identity><BarringIndication>1</BarringIndication></PublicIdentity><PublicIdentity><Identity>" + TEL_URI2 + "</Identity></PublicIdentity></ServiceProfile></IMSSubscription>";
std::vector<std::string> HandlersTest::TEL_URIS_IN_VECTOR = {TEL_URI, TEL_URI2};

const std::string HandlersTest::HTTP_PATH_REG_TRUE = "/registrations?send-notifications=true";
const std::string HandlersTest::HTTP_PATH_REG_FALSE = "/registrations?send-notifications=false";


HttpResolver* HandlersTest::_mock_resolver = NULL;
MockHssCacheProcessor* HandlersTest::_cache = NULL;
//FakeHssConnection* HandlersTest::_hss = NULL;
MockHssConnection* HandlersTest::_hss = NULL;
MockHttpStack* HandlersTest::_httpstack = NULL;
MockHttpConnection* HandlersTest::_mock_http_conn = NULL;
SproutConnection* HandlersTest::_sprout_conn = NULL;

//
// Digest and AV tests
//

TEST_F(HandlersTest, ImpiDigestMainline)
{
  // Test that an IMPI Digest Task requests the AV from the HSS and returns it
  // on the response
  MockHttpStack::Request req(_httpstack,
                             "/impi/" + IMPI,
                             "digest",
                             "?public_id=" + IMPU);

  ImpiTask::Config cfg(SCHEME_UNKNOWN, SCHEME_DIGEST, SCHEME_AKA, SCHEME_AKAV2);
  ImpiDigestTask* task = new ImpiDigestTask(req, &cfg, FAKE_TRAIL_ID);

  // Create a fake digest to be returned from the HSS
  DigestAuthVector* digest = new DigestAuthVector();
  digest->ha1 = "ha1";
  digest->realm = "realm";
  digest->qop = "qop";

  // Create an MAA* to return
  HssConnection::MultimediaAuthAnswer* answer =
    new HssConnection::MultimediaAuthAnswer(HssConnection::ResultCode::SUCCESS,
                                            digest,
                                            SCHEME_DIGEST);

  // Expect that the MAR has the correct IMPI, IMPU and scheme
  EXPECT_CALL(*_hss, send_multimedia_auth_request(_,
    AllOf(Field(&HssConnection::MultimediaAuthRequest::impi, IMPI),
          Field(&HssConnection::MultimediaAuthRequest::impu, IMPU),
          Field(&HssConnection::MultimediaAuthRequest::scheme, SCHEME_DIGEST))))
    .WillOnce(InvokeArgument<0>(answer));

  // Expect a 200 OK
  EXPECT_CALL(*_httpstack, send_reply(_, 200, _));

  task->run();

  // Build the expected response and check it's correct.
  EXPECT_EQ(build_digest_json(*digest), req.content());
}

TEST_F(HandlersTest, ImpuReadRegDataMainline)
{
  MockHttpStack::Request req(_httpstack,
                             "/impu/" + IMPU + "/reg-data",
                             "",
                             "",
                             "",
                             htp_method_GET);
  ImpuRegDataTask::Config cfg(true, 3600);
  ImpuReadRegDataTask* task = new ImpuReadRegDataTask(req, &cfg, FAKE_TRAIL_ID);
  
  ImplicitRegistrationSet* irs = new ImplicitRegistrationSet();
  irs->set_associated_impis({ IMPI });

  EXPECT_CALL(*_cache, get_implicit_registration_set_for_impu(_, _, IMPU, FAKE_TRAIL_ID))
    .WillOnce(InvokeArgument<0>(irs));

  // HTTP response is sent straight back - no state is changed.
  EXPECT_CALL(*_httpstack, send_reply(_, 200, _));

  task->run();
  // Build the expected response and check it's correct
  //EXPECT_EQ(REGDATA_RESULT, req.content());

}