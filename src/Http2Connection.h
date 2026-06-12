/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2026 Tatsuhiro Tsujikawa
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 */
/* copyright --> */
#ifndef D_HTTP2_CONNECTION_H
#define D_HTTP2_CONNECTION_H

#include "common.h"
#include "Command.h"
#include "SocketRecvBuffer.h"

#include <memory>
#include <string>
#include <vector>

#include <nghttp2/nghttp2.h>

namespace aria2 {

class HttpHeader;
class HttpRequest;
class HttpResponse;
class Request;
class SocketCore;

struct Http2HeaderField {
  std::string name;
  std::string value;
};

class Http2BodyBuffer {
public:
  void append(const unsigned char* data, size_t len);
  size_t pop(void* data, size_t len);
  bool empty() const { return readOffset_ == data_.size(); }
  size_t size() const { return data_.size() - readOffset_; }

private:
  std::vector<unsigned char> data_;
  size_t readOffset_ = 0;
};

std::vector<Http2HeaderField> createHTTP2HeaderFields(HttpRequest* httpRequest);

void applyHTTP2ResponseHeader(HttpHeader* httpHeader, const std::string& name,
                              const std::string& value);

bool shouldWaitForHTTP2FinalResponseHeader(const HttpHeader* httpHeader);

class Http2Connection : public SocketRecvBufferDataSource {
public:
  Http2Connection(cuid_t cuid, std::shared_ptr<SocketCore> socket);
  ~Http2Connection();

  void submitRequest(std::unique_ptr<HttpRequest> httpRequest);
  std::unique_ptr<HttpResponse> receiveResponse();

  void sendPendingData();
  bool sendBufferIsEmpty() const;

  virtual void readData(void* data, size_t& len) CXX11_OVERRIDE;

private:
  void initSession();
  bool readNetwork();
  std::unique_ptr<HttpResponse> popResponse();
  void disableHTTP2ForRetry();

  static ssize_t sendCallback(nghttp2_session* session, const uint8_t* data,
                              size_t length, int flags, void* userData);
  static int onHeaderCallback(nghttp2_session* session,
                              const nghttp2_frame* frame, const uint8_t* name,
                              size_t namelen, const uint8_t* value,
                              size_t valuelen, uint8_t flags,
                              void* userData);
  static int onFrameRecvCallback(nghttp2_session* session,
                                 const nghttp2_frame* frame, void* userData);
  static int onDataChunkRecvCallback(nghttp2_session* session, uint8_t flags,
                                     int32_t streamId, const uint8_t* data,
                                     size_t len, void* userData);
  static int onStreamCloseCallback(nghttp2_session* session, int32_t streamId,
                                   uint32_t errorCode, void* userData);
  static int errorCallback(nghttp2_session* session, int libErrorCode,
                           const char* msg, size_t len, void* userData);

  cuid_t cuid_;
  std::shared_ptr<SocketCore> socket_;
  nghttp2_session* session_;
  std::unique_ptr<HttpRequest> httpRequest_;
  std::shared_ptr<Request> request_;
  std::unique_ptr<HttpHeader> responseHeader_;
  Http2BodyBuffer bodyBuffer_;
  int32_t streamId_;
  uint32_t streamErrorCode_;
  bool responseReady_;
  bool streamClosed_;
  std::string callbackError_;
};

} // namespace aria2

#endif // D_HTTP2_CONNECTION_H
