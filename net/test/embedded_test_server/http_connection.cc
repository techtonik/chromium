// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/embedded_test_server/http_connection.h"

#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"
#include "net/test/embedded_test_server/http_response.h"

namespace net {
namespace test_server {

HttpConnection::HttpConnection(scoped_ptr<StreamSocket> socket,
                               const HandleRequestCallback& callback)
    : socket_(socket.Pass()),
      callback_(callback),
      read_buf_(new IOBufferWithSize(4096)) {}

HttpConnection::~HttpConnection() {
}

void HttpConnection::SendResponse(scoped_ptr<HttpResponse> response,
                                  const base::Closure& callback) {
  const std::string response_string = response->ToResponseString();
  if (response_string.length() > 0) {
    scoped_refptr<DrainableIOBuffer> write_buf(new DrainableIOBuffer(
        new StringIOBuffer(response_string), response_string.length()));

    SendInternal(callback, write_buf);
  } else {
    callback.Run();
  }
}

void HttpConnection::SendInternal(const base::Closure& callback,
                                  scoped_refptr<DrainableIOBuffer> buf) {
  while (buf->BytesRemaining() > 0) {
    int rv = socket_->Write(buf.get(), buf->BytesRemaining(),
                            base::Bind(&HttpConnection::OnSendInternalDone,
                                       base::Unretained(this), callback, buf));
    if (rv == ERR_IO_PENDING)
      return;

    buf->DidConsume(rv);
  }

  // The HttpConnection will be deleted by the callback since we only need to
  // serve a single request.
  callback.Run();
}

void HttpConnection::OnSendInternalDone(const base::Closure& callback,
                                        scoped_refptr<DrainableIOBuffer> buf,
                                        int rv) {
  buf->DidConsume(rv);
  SendInternal(callback, buf);
}

int HttpConnection::ReadData(const CompletionCallback& callback) {
  return socket_->Read(read_buf_.get(), read_buf_->size(), callback);
}

bool HttpConnection::ConsumeData(int size) {
  request_parser_.ProcessChunk(base::StringPiece(read_buf_->data(), size));
  if (request_parser_.ParseRequest() == HttpRequestParser::ACCEPTED) {
    callback_.Run(this, request_parser_.GetRequest());
    return true;
  }
  return false;
}

}  // namespace test_server
}  // namespace net
