// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/test_chromoting_client.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/thread_task_runner_handle.h"
#include "jingle/glue/thread_wrapper.h"
#include "net/base/request_priority.h"
#include "net/socket/client_socket_factory.h"
#include "remoting/base/url_request_context_getter.h"
#include "remoting/client/audio_player.h"
#include "remoting/client/chromoting_client.h"
#include "remoting/client/client_context.h"
#include "remoting/client/token_fetcher_proxy.h"
#include "remoting/protocol/chromium_port_allocator.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/libjingle_transport_factory.h"
#include "remoting/protocol/negotiating_client_authenticator.h"
#include "remoting/protocol/network_settings.h"
#include "remoting/protocol/session_config.h"
#include "remoting/protocol/third_party_client_authenticator.h"
#include "remoting/signaling/xmpp_signal_strategy.h"
#include "remoting/test/connection_setup_info.h"
#include "remoting/test/test_video_renderer.h"

namespace {
const char kXmppHostName[] = "talk.google.com";
const int kXmppPortNumber = 5222;

// Used as the TokenFetcherCallback for App Remoting sessions.
void FetchThirdPartyToken(
    const std::string& authorization_token,
    const std::string& shared_secret,
    const GURL& token_url,
    const std::string& host_public_key,
    const std::string& scope,
    base::WeakPtr<remoting::TokenFetcherProxy> token_fetcher_proxy) {
  VLOG(2)  << "FetchThirdPartyToken("
           << "token_url: " << token_url << ", "
           << "host_public_key: " << host_public_key << ", "
           << "scope: " << scope << ") Called";

  if (token_fetcher_proxy) {
    token_fetcher_proxy->OnTokenFetched(authorization_token, shared_secret);
    token_fetcher_proxy.reset();
  } else {
    LOG(ERROR) << "Invalid token fetcher proxy passed in";
  }
}

void FetchSecret(
    const std::string& client_secret,
    bool pairing_expected,
    const remoting::protocol::SecretFetchedCallback& secret_fetched_callback) {
  secret_fetched_callback.Run(client_secret);
}

}  // namespace

namespace remoting {
namespace test {

TestChromotingClient::TestChromotingClient()
    : TestChromotingClient(nullptr) {}

TestChromotingClient::TestChromotingClient(
    scoped_ptr<VideoRenderer> video_renderer)
    : connection_to_host_state_(protocol::ConnectionToHost::INITIALIZING),
      connection_error_code_(protocol::OK),
      video_renderer_(video_renderer.Pass()) {}

TestChromotingClient::~TestChromotingClient() {
  // Ensure any connections are closed and the members are destroyed in the
  // appropriate order.
  EndConnection();
}

void TestChromotingClient::StartConnection(
    const ConnectionSetupInfo& connection_setup_info) {
  // Required to establish a connection to the host.
  jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();

  scoped_refptr<URLRequestContextGetter> request_context_getter;
  request_context_getter = new URLRequestContextGetter(
      base::ThreadTaskRunnerHandle::Get(),   // network_runner
      base::ThreadTaskRunnerHandle::Get());  // file_runner

  client_context_.reset(new ClientContext(base::ThreadTaskRunnerHandle::Get()));

  // Check to see if the user passed in a customized video renderer.
  if (!video_renderer_) {
    video_renderer_.reset(new TestVideoRenderer());
  }

  chromoting_client_.reset(new ChromotingClient(client_context_.get(),
                                                this,  // client_user_interface.
                                                video_renderer_.get(),
                                                nullptr));  // audio_player

  if (test_connection_to_host_) {
    chromoting_client_->SetConnectionToHostForTests(
        test_connection_to_host_.Pass());
  }

  XmppSignalStrategy::XmppServerConfig xmpp_server_config;
  xmpp_server_config.host = kXmppHostName;
  xmpp_server_config.port = kXmppPortNumber;
  xmpp_server_config.use_tls = true;
  xmpp_server_config.username = connection_setup_info.user_name;
  xmpp_server_config.auth_token = connection_setup_info.access_token;

  // Set up the signal strategy.  This must outlive the client object.
  signal_strategy_.reset(
      new XmppSignalStrategy(net::ClientSocketFactory::GetDefaultFactory(),
                             request_context_getter, xmpp_server_config));

  protocol::NetworkSettings network_settings(
      protocol::NetworkSettings::NAT_TRAVERSAL_FULL);

  scoped_ptr<protocol::ChromiumPortAllocator> port_allocator(
      protocol::ChromiumPortAllocator::Create(request_context_getter,
                                              network_settings));

  scoped_ptr<protocol::TransportFactory> transport_factory(
      new protocol::LibjingleTransportFactory(
          signal_strategy_.get(), port_allocator.Pass(), network_settings,
          protocol::TransportRole::CLIENT));

  scoped_ptr<protocol::ThirdPartyClientAuthenticator::TokenFetcher>
      token_fetcher(new TokenFetcherProxy(
          base::Bind(&FetchThirdPartyToken,
                     connection_setup_info.authorization_code,
                     connection_setup_info.shared_secret),
          connection_setup_info.public_key));

  protocol::FetchSecretCallback fetch_secret_callback;
  if (!connection_setup_info.pin.empty()) {
    fetch_secret_callback = base::Bind(&FetchSecret, connection_setup_info.pin);
  }

  scoped_ptr<protocol::Authenticator> authenticator(
      new protocol::NegotiatingClientAuthenticator(
          connection_setup_info.pairing_id,
          connection_setup_info.shared_secret,
          connection_setup_info.host_id,
          fetch_secret_callback,
          token_fetcher.Pass(),
          connection_setup_info.auth_methods));

  chromoting_client_->Start(
      signal_strategy_.get(), authenticator.Pass(), transport_factory.Pass(),
      connection_setup_info.host_jid, connection_setup_info.capabilities);
}

void TestChromotingClient::EndConnection() {
  // Clearing out the client will close the connection.
  chromoting_client_.reset();

  // The signal strategy object must outlive the client so destroy it next.
  signal_strategy_.reset();

  // The connection state will be updated when the chromoting client was
  // destroyed if an active connection was established, but not in other cases.
  // We should be consistent in either case so we will set the state if needed.
  if (connection_to_host_state_ != protocol::ConnectionToHost::CLOSED &&
      connection_to_host_state_ != protocol::ConnectionToHost::FAILED &&
      connection_error_code_ == protocol::OK) {
    OnConnectionState(protocol::ConnectionToHost::CLOSED, protocol::OK);
  }
}

void TestChromotingClient::AddRemoteConnectionObserver(
    RemoteConnectionObserver* observer) {
  DCHECK(observer);

  connection_observers_.AddObserver(observer);
}

void TestChromotingClient::RemoveRemoteConnectionObserver(
    RemoteConnectionObserver* observer) {
  DCHECK(observer);

  connection_observers_.RemoveObserver(observer);
}

void TestChromotingClient::SetConnectionToHostForTests(
    scoped_ptr<protocol::ConnectionToHost> connection_to_host) {
  test_connection_to_host_ = connection_to_host.Pass();
}

void TestChromotingClient::OnConnectionState(
    protocol::ConnectionToHost::State state,
    protocol::ErrorCode error_code) {
  VLOG(1) << "TestChromotingClient::OnConnectionState("
          << "state: " << protocol::ConnectionToHost::StateToString(state)
          << ", error_code: " << protocol::ErrorCodeToString(error_code)
          << ") Called";

  connection_error_code_ = error_code;
  connection_to_host_state_ = state;

  FOR_EACH_OBSERVER(RemoteConnectionObserver, connection_observers_,
                    ConnectionStateChanged(state, error_code));
}

void TestChromotingClient::OnConnectionReady(bool ready) {
  VLOG(1) << "TestChromotingClient::OnConnectionReady("
          << "ready:" << ready << ") Called";

  FOR_EACH_OBSERVER(RemoteConnectionObserver, connection_observers_,
                    ConnectionReady(ready));
}

void TestChromotingClient::OnRouteChanged(
    const std::string& channel_name,
    const protocol::TransportRoute& route) {
  VLOG(1) << "TestChromotingClient::OnRouteChanged("
          << "channel_name:" << channel_name << ", "
          << "route:" << protocol::TransportRoute::GetTypeString(route.type)
          << ") Called";

  FOR_EACH_OBSERVER(RemoteConnectionObserver, connection_observers_,
                    RouteChanged(channel_name, route));
}

void TestChromotingClient::SetCapabilities(const std::string& capabilities) {
  VLOG(1) << "TestChromotingClient::SetCapabilities("
          << "capabilities: " << capabilities << ") Called";

  FOR_EACH_OBSERVER(RemoteConnectionObserver, connection_observers_,
                    CapabilitiesSet(capabilities));
}

void TestChromotingClient::SetPairingResponse(
    const protocol::PairingResponse& pairing_response) {
  VLOG(1) << "TestChromotingClient::SetPairingResponse("
          << "client_id: " << pairing_response.client_id() << ", "
          << "shared_secret: " << pairing_response.shared_secret()
          << ") Called";

  FOR_EACH_OBSERVER(RemoteConnectionObserver, connection_observers_,
                    PairingResponseSet(pairing_response));
}

void TestChromotingClient::DeliverHostMessage(
    const protocol::ExtensionMessage& message) {
  VLOG(1) << "TestChromotingClient::DeliverHostMessage("
          << "type: " << message.type() << ", "
          << "data: " << message.data() << ") Called";

  FOR_EACH_OBSERVER(RemoteConnectionObserver, connection_observers_,
                    HostMessageReceived(message));
}

protocol::ClipboardStub* TestChromotingClient::GetClipboardStub() {
  VLOG(1) << "TestChromotingClient::GetClipboardStub() Called";
  return this;
}

protocol::CursorShapeStub* TestChromotingClient::GetCursorShapeStub() {
  VLOG(1) << "TestChromotingClient::GetCursorShapeStub() Called";
  return this;
}

void TestChromotingClient::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  VLOG(1) << "TestChromotingClient::InjectClipboardEvent() Called";
}

void TestChromotingClient::SetCursorShape(
    const protocol::CursorShapeInfo& cursor_shape) {
  VLOG(1) << "TestChromotingClient::SetCursorShape() Called";
}

}  // namespace test
}  // namespace remoting
