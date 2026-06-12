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
#include "Http2Connection.h"

#include <algorithm>
#include <array>
#include <cstring>

#include "A2STR.h"
#include "DlAbortEx.h"
#include "DlRetryEx.h"
#include "HttpHeader.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "Request.h"
#include "LogFactory.h"
#include "SocketCore.h"
#include "a2functional.h"
#include "fmt.h"
#include "message.h"
#include "util.h"

namespace aria2 {

void Http2BodyBuffer::append(const unsigned char* data, size_t len)
{
  if (len == 0) {
    return;
  }
  if (empty()) {
    data_.clear();
    readOffset_ = 0;
  }
  data_.insert(data_.end(), data, data + len);
}

size_t Http2BodyBuffer::pop(void* data, size_t len)
{
  auto n = std::min(len, size());
  if (n == 0) {
    return 0;
  }
  auto out = static_cast<unsigned char*>(data);
  std::copy(data_.begin() + readOffset_, data_.begin() + readOffset_ + n,
            out);
  readOffset_ += n;
  if (empty()) {
    data_.clear();
    readOffset_ = 0;
  }
  return n;
}

namespace {
std::string trimHeaderValue(const std::string& s)
{
  auto first = s.begin();
  while (first != s.end() && (*first == ' ' || *first == '\t')) {
    ++first;
  }
  auto last = s.end();
  while (last != first && (*(last - 1) == '\r' || *(last - 1) == ' ' ||
                           *(last - 1) == '\t')) {
    --last;
  }
  return std::string(first, last);
}

bool isHTTP2ForbiddenHeader(const std::string& name,
                            const std::string& value = A2STR::NIL)
{
  if (name == "connection" || name == "keep-alive" ||
      name == "proxy-connection" || name == "transfer-encoding" ||
      name == "upgrade" || name == "http2-settings" ||
      name == "want-digest") {
    return true;
  }
  return name == "te" && !util::strieq(value, "trailers");
}

void addNV(std::vector<nghttp2_nv>& nva,
           const std::vector<Http2HeaderField>& fields)
{
  for (const auto& field : fields) {
    nghttp2_nv nv;
    nv.name = reinterpret_cast<uint8_t*>(
        const_cast<char*>(field.name.c_str()));
    nv.value = reinterpret_cast<uint8_t*>(
        const_cast<char*>(field.value.c_str()));
    nv.namelen = field.name.size();
    nv.valuelen = field.value.size();
    nv.flags = NGHTTP2_NV_FLAG_NONE;
    nva.push_back(nv);
  }
}
} // namespace

std::vector<Http2HeaderField> createHTTP2HeaderFields(HttpRequest* httpRequest)
{
  auto request = httpRequest->createRequest();
  auto lineEnd = request.find("\r\n");
  if (lineEnd == std::string::npos) {
    throw DL_ABORT_EX("Bad HTTP request: missing request line terminator");
  }

  auto requestLine = request.substr(0, lineEnd);
  auto methodEnd = requestLine.find(' ');
  auto pathEnd = methodEnd == std::string::npos
                     ? std::string::npos
                     : requestLine.find(' ', methodEnd + 1);
  if (methodEnd == std::string::npos || pathEnd == std::string::npos) {
    throw DL_ABORT_EX("Bad HTTP request line");
  }

  auto method = requestLine.substr(0, methodEnd);
  auto path = requestLine.substr(methodEnd + 1, pathEnd - methodEnd - 1);
  std::string authority;
  std::vector<Http2HeaderField> regularHeaders;

  auto pos = lineEnd + 2;
  while (pos < request.size()) {
    auto next = request.find("\r\n", pos);
    if (next == std::string::npos || next == pos) {
      break;
    }
    auto line = request.substr(pos, next - pos);
    auto colon = line.find(':');
    if (colon != std::string::npos) {
      auto name = util::toLower(line.substr(0, colon));
      auto value = trimHeaderValue(line.substr(colon + 1));
      if (name == "host") {
        authority = value;
      }
      else if (!name.empty() && !isHTTP2ForbiddenHeader(name, value)) {
        regularHeaders.push_back({std::move(name), std::move(value)});
      }
    }
    pos = next + 2;
  }

  if (authority.empty()) {
    authority = httpRequest->getURIHost();
    auto port = httpRequest->getPort();
    if (!((httpRequest->getProtocol() == "http" && port == 80) ||
          (httpRequest->getProtocol() == "https" && port == 443))) {
      authority += ":";
      authority += util::uitos(port);
    }
  }

  std::vector<Http2HeaderField> result;
  result.push_back({":method", std::move(method)});
  result.push_back({":scheme", httpRequest->getProtocol()});
  result.push_back({":authority", std::move(authority)});
  result.push_back({":path", std::move(path)});
  result.insert(result.end(), regularHeaders.begin(), regularHeaders.end());
  return result;
}

void applyHTTP2ResponseHeader(HttpHeader* httpHeader, const std::string& name,
                              const std::string& value)
{
  httpHeader->setVersion("HTTP/2");
  if (name == ":status") {
    int statusCode = 0;
    if (!util::parseIntNoThrow(statusCode, value)) {
      throw DL_ABORT_EX("Bad HTTP/2 status code");
    }
    httpHeader->setStatusCode(statusCode);
    return;
  }
  if (name.empty() || name[0] == ':' || isHTTP2ForbiddenHeader(name, value)) {
    return;
  }

  auto hdKey = idInterestingHeader(name.c_str());
  if (hdKey != HttpHeader::MAX_INTERESTING_HEADER) {
    httpHeader->put(hdKey, value);
  }
}

bool shouldWaitForHTTP2FinalResponseHeader(const HttpHeader* httpHeader)
{
  auto statusCode = httpHeader->getStatusCode();
  return statusCode >= 100 && statusCode < 200;
}

Http2Connection::Http2Connection(cuid_t cuid,
                                 std::shared_ptr<SocketCore> socket)
    : cuid_(cuid),
      socket_(std::move(socket)),
      session_(nullptr),
      streamId_(-1),
      streamErrorCode_(0),
      responseReady_(false),
      streamClosed_(false)
{
}

Http2Connection::~Http2Connection()
{
  if (session_) {
    nghttp2_session_del(session_);
  }
}

void Http2Connection::initSession()
{
  if (session_) {
    return;
  }

  nghttp2_session_callbacks* callbacks = nullptr;
  auto rv = nghttp2_session_callbacks_new(&callbacks);
  if (rv != 0) {
    throw DL_ABORT_EX(fmt("nghttp2_session_callbacks_new() failed: %s",
                          nghttp2_strerror(rv)));
  }
  std::unique_ptr<nghttp2_session_callbacks,
                  decltype(&nghttp2_session_callbacks_del)>
      callbacksDeleter(callbacks, nghttp2_session_callbacks_del);

  nghttp2_session_callbacks_set_send_callback(callbacks, sendCallback);
  nghttp2_session_callbacks_set_on_header_callback(callbacks,
                                                   onHeaderCallback);
  nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks,
                                                       onFrameRecvCallback);
  nghttp2_session_callbacks_set_on_data_chunk_recv_callback(
      callbacks, onDataChunkRecvCallback);
  nghttp2_session_callbacks_set_on_stream_close_callback(
      callbacks, onStreamCloseCallback);
  nghttp2_session_callbacks_set_error_callback2(callbacks, errorCallback);

  rv = nghttp2_session_client_new(&session_, callbacks, this);
  if (rv != 0) {
    throw DL_ABORT_EX(
        fmt("nghttp2_session_client_new() failed: %s", nghttp2_strerror(rv)));
  }

  rv = nghttp2_submit_settings(session_, NGHTTP2_FLAG_NONE, nullptr, 0);
  if (rv != 0) {
    throw DL_ABORT_EX(
        fmt("nghttp2_submit_settings() failed: %s", nghttp2_strerror(rv)));
  }
}

void Http2Connection::submitRequest(std::unique_ptr<HttpRequest> httpRequest)
{
  if (httpRequest_) {
    throw DL_ABORT_EX("HTTP/2 multiplexing is not implemented yet");
  }

  initSession();
  auto fields = createHTTP2HeaderFields(httpRequest.get());
  std::vector<nghttp2_nv> nva;
  addNV(nva, fields);

  auto rv = nghttp2_submit_request(session_, nullptr, nva.data(), nva.size(),
                                   nullptr, nullptr);
  if (rv < 0) {
    throw DL_ABORT_EX(
        fmt("nghttp2_submit_request() failed: %s", nghttp2_strerror(rv)));
  }

  streamId_ = rv;
  streamErrorCode_ = 0;
  responseReady_ = false;
  streamClosed_ = false;
  responseHeader_ = make_unique<HttpHeader>();
  responseHeader_->setVersion("HTTP/2");
  request_ = httpRequest->getRequest();
  httpRequest_ = std::move(httpRequest);
  sendPendingData();
}

std::unique_ptr<HttpResponse> Http2Connection::popResponse()
{
  auto httpResponse = make_unique<HttpResponse>();
  httpResponse->setCuid(cuid_);
  httpResponse->setHttpHeader(std::move(responseHeader_));
  httpResponse->setHttpRequest(std::move(httpRequest_));
  responseReady_ = false;
  return httpResponse;
}

std::unique_ptr<HttpResponse> Http2Connection::receiveResponse()
{
  if (!httpRequest_) {
    throw DL_ABORT_EX(EX_NO_HTTP_REQUEST_ENTRY_FOUND);
  }
  if (responseReady_) {
    return popResponse();
  }

  while (!responseReady_) {
    sendPendingData();
    if (responseReady_) {
      break;
    }
    if (!readNetwork()) {
      break;
    }
  }

  if (responseReady_) {
    return popResponse();
  }
  if (streamClosed_) {
    disableHTTP2ForRetry();
    throw DL_RETRY_EX(fmt("HTTP/2 stream closed before response headers "
                          "(error code %u)",
                          streamErrorCode_));
  }
  return nullptr;
}

void Http2Connection::sendPendingData()
{
  initSession();
  auto rv = nghttp2_session_send(session_);
  if (rv < 0 && rv != NGHTTP2_ERR_WOULDBLOCK) {
    if (!callbackError_.empty()) {
      throw DL_RETRY_EX(callbackError_);
    }
    throw DL_ABORT_EX(
        fmt("nghttp2_session_send() failed: %s", nghttp2_strerror(rv)));
  }
}

bool Http2Connection::sendBufferIsEmpty() const
{
  return !session_ || nghttp2_session_want_write(session_) == 0;
}

bool Http2Connection::readNetwork()
{
  std::array<unsigned char, 16_k> buf;
  size_t len = buf.size();
  socket_->readData(buf.data(), len);
  if (len == 0) {
    if (!socket_->wantRead() && !socket_->wantWrite()) {
      throw DL_RETRY_EX(EX_GOT_EOF);
    }
    return false;
  }

  auto rv = nghttp2_session_mem_recv(session_, buf.data(), len);
  if (rv < 0) {
    throw DL_ABORT_EX(
        fmt("nghttp2_session_mem_recv() failed: %s", nghttp2_strerror(rv)));
  }
  sendPendingData();
  return true;
}

void Http2Connection::readData(void* data, size_t& len)
{
  while (bodyBuffer_.empty() && !streamClosed_) {
    sendPendingData();
    if (!readNetwork()) {
      len = 0;
      return;
    }
  }

  len = bodyBuffer_.pop(data, len);

  if (len == 0 && streamClosed_ && streamErrorCode_ != 0) {
    disableHTTP2ForRetry();
    throw DL_RETRY_EX(
        fmt("HTTP/2 stream closed with error code %u", streamErrorCode_));
  }
}

void Http2Connection::disableHTTP2ForRetry()
{
  if (request_) {
    request_->disableHTTP2();
  }
}

ssize_t Http2Connection::sendCallback(nghttp2_session* session,
                                      const uint8_t* data, size_t length,
                                      int flags, void* userData)
{
  (void)session;
  (void)flags;
  auto self = static_cast<Http2Connection*>(userData);
  try {
    auto n = self->socket_->writeData(data, length);
    if (n == 0) {
      return NGHTTP2_ERR_WOULDBLOCK;
    }
    return n;
  }
  catch (const std::exception& e) {
    self->callbackError_ = e.what();
    return NGHTTP2_ERR_CALLBACK_FAILURE;
  }
}

int Http2Connection::onHeaderCallback(nghttp2_session* session,
                                      const nghttp2_frame* frame,
                                      const uint8_t* name, size_t namelen,
                                      const uint8_t* value, size_t valuelen,
                                      uint8_t flags, void* userData)
{
  (void)session;
  (void)flags;
  auto self = static_cast<Http2Connection*>(userData);
  if (frame->hd.stream_id != self->streamId_ ||
      frame->hd.type != NGHTTP2_HEADERS ||
      frame->headers.cat != NGHTTP2_HCAT_RESPONSE) {
    return 0;
  }
  applyHTTP2ResponseHeader(
      self->responseHeader_.get(),
      std::string(reinterpret_cast<const char*>(name), namelen),
      std::string(reinterpret_cast<const char*>(value), valuelen));
  return 0;
}

int Http2Connection::onFrameRecvCallback(nghttp2_session* session,
                                         const nghttp2_frame* frame,
                                         void* userData)
{
  (void)session;
  auto self = static_cast<Http2Connection*>(userData);
  if (frame->hd.stream_id != self->streamId_) {
    return 0;
  }
  if (frame->hd.type == NGHTTP2_HEADERS &&
      frame->headers.cat == NGHTTP2_HCAT_RESPONSE) {
    if (shouldWaitForHTTP2FinalResponseHeader(self->responseHeader_.get())) {
      self->responseHeader_ = make_unique<HttpHeader>();
      self->responseHeader_->setVersion("HTTP/2");
      return 0;
    }
    self->responseReady_ = true;
  }
  if (frame->hd.type == NGHTTP2_RST_STREAM) {
    self->streamErrorCode_ = frame->rst_stream.error_code;
  }
  if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) {
    self->streamClosed_ = true;
  }
  return 0;
}

int Http2Connection::onDataChunkRecvCallback(nghttp2_session* session,
                                             uint8_t flags, int32_t streamId,
                                             const uint8_t* data, size_t len,
                                             void* userData)
{
  (void)flags;
  auto self = static_cast<Http2Connection*>(userData);
  if (streamId != self->streamId_) {
    return 0;
  }
  self->bodyBuffer_.append(data, len);
  return 0;
}

int Http2Connection::onStreamCloseCallback(nghttp2_session* session,
                                           int32_t streamId,
                                           uint32_t errorCode, void* userData)
{
  (void)session;
  auto self = static_cast<Http2Connection*>(userData);
  if (streamId == self->streamId_) {
    self->streamClosed_ = true;
    self->streamErrorCode_ = errorCode;
  }
  return 0;
}

int Http2Connection::errorCallback(nghttp2_session* session, int libErrorCode,
                                   const char* msg, size_t len,
                                   void* userData)
{
  (void)session;
  (void)userData;
  A2_LOG_DEBUG(fmt("HTTP/2 nghttp2 error: code=%d msg=%s", libErrorCode,
                   std::string(msg, len).c_str()));
  return 0;
}

} // namespace aria2
