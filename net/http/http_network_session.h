// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_NETWORK_SESSION_H_
#define NET_HTTP_HTTP_NETWORK_SESSION_H_

#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_auth_cache.h"
#include "net/http/http_stream_factory.h"
#include "net/quic/quic_stream_factory.h"
#include "net/socket/next_proto.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/ssl/ssl_client_auth_cache.h"

namespace base {
class Value;
}

namespace net {

class CertPolicyEnforcer;
class CertVerifier;
class ChannelIDService;
class ClientSocketFactory;
class ClientSocketPoolManager;
class CTVerifier;
class HostResolver;
class HttpAuthHandlerFactory;
class HttpNetworkSessionPeer;
class HttpProxyClientSocketPool;
class HttpResponseBodyDrainer;
class HttpServerProperties;
class NetLog;
class NetworkDelegate;
class ProxyDelegate;
class ProxyService;
class QuicClock;
class QuicCryptoClientStreamFactory;
class QuicServerInfoFactory;
class SocketPerformanceWatcherFactory;
class SOCKSClientSocketPool;
class SSLClientSocketPool;
class SSLConfigService;
class TransportClientSocketPool;
class TransportSecurityState;

// This class holds session objects used by HttpNetworkTransaction objects.
class NET_EXPORT HttpNetworkSession
    : NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  struct NET_EXPORT Params {
    Params();
    ~Params();

    ClientSocketFactory* client_socket_factory;
    HostResolver* host_resolver;
    CertVerifier* cert_verifier;
    CertPolicyEnforcer* cert_policy_enforcer;
    ChannelIDService* channel_id_service;
    TransportSecurityState* transport_security_state;
    CTVerifier* cert_transparency_verifier;
    ProxyService* proxy_service;
    std::string ssl_session_cache_shard;
    SSLConfigService* ssl_config_service;
    HttpAuthHandlerFactory* http_auth_handler_factory;
    NetworkDelegate* network_delegate;
    base::WeakPtr<HttpServerProperties> http_server_properties;
    NetLog* net_log;
    HostMappingRules* host_mapping_rules;
    SocketPerformanceWatcherFactory* socket_performance_watcher_factory;
    bool ignore_certificate_errors;
    uint16 testing_fixed_http_port;
    uint16 testing_fixed_https_port;
    bool enable_tcp_fast_open_for_ssl;

    bool enable_spdy_compression;
    bool enable_spdy_ping_based_connection_checking;
    NextProto spdy_default_protocol;
    // The protocols supported by NPN (next protocol negotiation) during the
    // SSL handshake as well as by HTTP Alternate-Protocol.
    // TODO(mmenke):  This is currently empty by default, and alternate
    //                protocols are disabled.  We should use some reasonable
    //                defaults.
    NextProtoVector next_protos;
    size_t spdy_session_max_recv_window_size;
    size_t spdy_stream_max_recv_window_size;
    size_t spdy_initial_max_concurrent_streams;
    SpdySessionPool::TimeFunc time_func;
    std::string trusted_spdy_proxy;
    // URLs to exclude from forced SPDY.
    std::set<HostPortPair> forced_spdy_exclusions;
    bool use_alternative_services;
    double alternative_service_probability_threshold;

    bool enable_quic;
    bool enable_insecure_quic;
    bool enable_quic_for_proxies;
    bool enable_quic_port_selection;
    bool quic_always_require_handshake_confirmation;
    bool quic_disable_connection_pooling;
    float quic_load_server_info_timeout_srtt_multiplier;
    bool quic_enable_connection_racing;
    bool quic_enable_non_blocking_io;
    bool quic_disable_disk_cache;
    bool quic_prefer_aes;
    int quic_max_number_of_lossy_connections;
    float quic_packet_loss_threshold;
    int quic_socket_receive_buffer_size;
    bool quic_delay_tcp_race;
    bool quic_store_server_configs_in_properties;
    HostPortPair origin_to_force_quic_on;
    QuicClock* quic_clock;  // Will be owned by QuicStreamFactory.
    QuicRandom* quic_random;
    size_t quic_max_packet_length;
    std::string quic_user_agent_id;
    bool enable_user_alternate_protocol_ports;
    QuicCryptoClientStreamFactory* quic_crypto_client_stream_factory;
    QuicVersionVector quic_supported_versions;
    int quic_max_recent_disabled_reasons;
    int quic_threshold_public_resets_post_handshake;
    int quic_threshold_timeouts_streams_open;
    QuicTagVector quic_connection_options;
    ProxyDelegate* proxy_delegate;
  };

  enum SocketPoolType {
    NORMAL_SOCKET_POOL,
    WEBSOCKET_SOCKET_POOL,
    NUM_SOCKET_POOL_TYPES
  };

  explicit HttpNetworkSession(const Params& params);
  ~HttpNetworkSession();

  HttpAuthCache* http_auth_cache() { return &http_auth_cache_; }
  SSLClientAuthCache* ssl_client_auth_cache() {
    return &ssl_client_auth_cache_;
  }

  void AddResponseDrainer(HttpResponseBodyDrainer* drainer);

  void RemoveResponseDrainer(HttpResponseBodyDrainer* drainer);

  TransportClientSocketPool* GetTransportSocketPool(SocketPoolType pool_type);
  SSLClientSocketPool* GetSSLSocketPool(SocketPoolType pool_type);
  SOCKSClientSocketPool* GetSocketPoolForSOCKSProxy(
      SocketPoolType pool_type,
      const HostPortPair& socks_proxy);
  HttpProxyClientSocketPool* GetSocketPoolForHTTPProxy(
      SocketPoolType pool_type,
      const HostPortPair& http_proxy);
  SSLClientSocketPool* GetSocketPoolForSSLWithProxy(
      SocketPoolType pool_type,
      const HostPortPair& proxy_server);

  CertVerifier* cert_verifier() { return cert_verifier_; }
  ProxyService* proxy_service() { return proxy_service_; }
  SSLConfigService* ssl_config_service() { return ssl_config_service_.get(); }
  SpdySessionPool* spdy_session_pool() { return &spdy_session_pool_; }
  QuicStreamFactory* quic_stream_factory() { return &quic_stream_factory_; }
  HttpAuthHandlerFactory* http_auth_handler_factory() {
    return http_auth_handler_factory_;
  }
  NetworkDelegate* network_delegate() {
    return network_delegate_;
  }
  base::WeakPtr<HttpServerProperties> http_server_properties() {
    return http_server_properties_;
  }
  HttpStreamFactory* http_stream_factory() {
    return http_stream_factory_.get();
  }
  HttpStreamFactory* http_stream_factory_for_websocket() {
    return http_stream_factory_for_websocket_.get();
  }
  NetLog* net_log() {
    return net_log_;
  }

  // Creates a Value summary of the state of the socket pools.
  scoped_ptr<base::Value> SocketPoolInfoToValue() const;

  // Creates a Value summary of the state of the SPDY sessions.
  scoped_ptr<base::Value> SpdySessionPoolInfoToValue() const;

  // Creates a Value summary of the state of the QUIC sessions and
  // configuration.
  scoped_ptr<base::Value> QuicInfoToValue() const;

  void CloseAllConnections();
  void CloseIdleConnections();

  // Returns the original Params used to construct this session.
  const Params& params() const { return params_; }

  bool IsProtocolEnabled(AlternateProtocol protocol) const;

  // Populates |*next_protos| with protocols.
  void GetNextProtos(NextProtoVector* next_protos) const;

  // Convenience function for searching through |params_| for
  // |forced_spdy_exclusions|.
  bool HasSpdyExclusion(HostPortPair host_port_pair) const;

 private:
  friend class HttpNetworkSessionPeer;

  ClientSocketPoolManager* GetSocketPoolManager(SocketPoolType pool_type);

  NetLog* const net_log_;
  NetworkDelegate* const network_delegate_;
  const base::WeakPtr<HttpServerProperties> http_server_properties_;
  CertVerifier* const cert_verifier_;
  HttpAuthHandlerFactory* const http_auth_handler_factory_;

  // Not const since it's modified by HttpNetworkSessionPeer for testing.
  ProxyService* proxy_service_;
  const scoped_refptr<SSLConfigService> ssl_config_service_;

  HttpAuthCache http_auth_cache_;
  SSLClientAuthCache ssl_client_auth_cache_;
  scoped_ptr<ClientSocketPoolManager> normal_socket_pool_manager_;
  scoped_ptr<ClientSocketPoolManager> websocket_socket_pool_manager_;
  QuicStreamFactory quic_stream_factory_;
  SpdySessionPool spdy_session_pool_;
  scoped_ptr<HttpStreamFactory> http_stream_factory_;
  scoped_ptr<HttpStreamFactory> http_stream_factory_for_websocket_;
  std::set<HttpResponseBodyDrainer*> response_drainers_;

  NextProtoVector next_protos_;
  bool enabled_protocols_[NUM_VALID_ALTERNATE_PROTOCOLS];

  Params params_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_NETWORK_SESSION_H_
