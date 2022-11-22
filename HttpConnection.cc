/*
 * Copyright ©2022 Hal Perkins.  All rights reserved.  Permission is
 * hereby granted to students registered for University of Washington
 * CSE 333 for use solely during Spring Quarter 2022 for purposes of
 * the course.  No other use, copying, distribution, or modification
 * is permitted without prior written consent. Copyrights for
 * third-party components of this work must be honored.  Instructors
 * interested in reusing these course materials should contact the
 * author.
 */

#include <stdint.h>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <map>
#include <string>
#include <vector>

#include "./HttpRequest.h"
#include "./HttpUtils.h"
#include "./HttpConnection.h"

using std::map;
using std::string;
using std::vector;

namespace hw4 {

static const char* kHeaderEnd = "\r\n\r\n";
static const int kHeaderEndLen = 4;

bool HttpConnection::GetNextRequest(HttpRequest* const request) {
  // Use WrappedRead from HttpUtils.cc to read bytes from the files into
  // private buffer_ variable. Keep reading until:
  // 1. The connection drops
  // 2. You see a "\r\n\r\n" indicating the end of the request header.
  //
  // Hint: Try and read in a large amount of bytes each time you call
  // WrappedRead.
  //
  // After reading complete request header, use ParseRequest() to parse into
  // an HttpRequest and save to the output parameter request.
  //
  // Important note: Clients may send back-to-back requests on the same socket.
  // This means WrappedRead may also end up reading more than one request.
  // Make sure to save anything you read after "\r\n\r\n" in buffer_ for the
  // next time the caller invokes GetNextRequest()!

  // STEP 1:

  __SIZE_TYPE__ pos = buffer_.find(kHeaderEnd);

  // If buffer_ already contains everything for the next request, parse the request 
  // without reading addtional bytes. Otherwise, read until the connection dropped or 
  // until we see "\r\n\r\n"
  if (pos == string::npos) {
    int read_bytes;
    unsigned char buf[1024];
    while (1) {
      read_bytes = WrappedRead(fd_, buf, 1024);
      if (read_bytes == 0) {  // EOF or connection dropped
        break;
      } else if (read_bytes == -1) {  // read failed
        return false;
      } else {
        buffer_ += string(reinterpret_cast<char*>(buf), read_bytes);

        // stop reading if reached"\r\n\r\n"
        pos = buffer_.find(kHeaderEnd);
        if (pos != string::npos) {
          break;
        }
      }
    }
  }

  // check if the request header ends with "\r\n\r\n"
  if (pos == string::npos) {
    return false;
  }

  // parse the header and store it
  string header = buffer_.substr(0, pos + kHeaderEndLen);
  *request = ParseRequest(header);

  // return false if the request is not well-formatted
  if (request->uri() == "BAD_") {
    return false;
  }

  // perserve everything (if any) after "\r\n\r\n" in buffer_
  buffer_ = buffer_.substr(pos + kHeaderEndLen);

  return true;  // You may want to change this.
}

bool HttpConnection::WriteResponse(const HttpResponse& response) const {
  string str = response.GenerateResponseString();
  int res = WrappedWrite(fd_,
                         reinterpret_cast<const unsigned char*>(str.c_str()),
                         str.length());
  if (res != static_cast<int>(str.length()))
    return false;
  return true;
}

HttpRequest HttpConnection::ParseRequest(const string& request) const {
  HttpRequest req("/");  // by default, get "/".

  // Plan for STEP 2:
  // 1. Split the request into different lines (split on "\r\n").
  // 2. Extract the URI from the first line and store it in req.URI.
  // 3. For the rest of the lines in the request, track the header name and
  //    value and store them in req.headers_ (e.g. HttpRequest::AddHeader).
  //
  // Hint: Take a look at HttpRequest.h for details about the HTTP header
  // format that you need to parse.
  //
  // You'll probably want to look up boost functions for:
  // - Splitting a string into lines on a "\r\n" delimiter
  // - Trimming whitespace from the end of a string
  // - Converting a string to lowercase.
  //
  // Note: If a header is malformed, skip that line.

  // STEP 2:

// split the request into different lines (split on "\r\n")
  vector<string> lines;
  boost::split(lines, request, boost::is_any_of("\r\n"),
               boost::token_compress_on);

  if (lines.size() < 2) {
      req.set_uri("BAD_");
    return req;
  }

  // trim whitespaces from the end
  for (__SIZE_TYPE__ i = 0; i < lines.size(); i++)
    boost::trim(lines[i]);

  // split first line on " "
  vector<string> fst_line;
  boost::split(fst_line, lines[0], boost::is_any_of(" "),
               boost::token_compress_on);

  // check the format the first line in request
  if (fst_line.size() == 1) {
    if (fst_line[0] != "GET") {
      req.set_uri("BAD_");
      return req;
    }
  } else if (fst_line.size() == 2) {
    if (fst_line[0] != "GET" ||
        (fst_line[1][0] != '/' &&
         fst_line[1].find("HTTP/") == string::npos)) {
      req.set_uri("BAD_");
      return req;
    }
  } else if (fst_line.size() == 3) {
    if (fst_line[0] != "GET" ||
        fst_line[1][0] != '/' ||
        fst_line[2].find("HTTP/") == string::npos) {
      req.set_uri("BAD_");
      return req;
    }

    // Extract the URI from the first line and store it in req.URI
    req.set_uri(fst_line[1]);
  } else {
    // If # of tokens in the first line is not two or three, the request is not well-formatted
      req.set_uri("BAD_");
    return req;
  }

  // split the rest of the lines into headername and headervalue on ": "
  vector<string> header;
  for (__SIZE_TYPE__ j = 1; j < lines.size() - 1; j++) {
    __SIZE_TYPE__ fst_col = lines[j].find(": ");

    // check if the line is in correct format
    if (fst_col == string::npos) {
      req.set_uri("BAD_");
      return req;
    }

    // map header_name to header_val in headers
    string header_name = lines[j].substr(0, fst_col);
    boost::to_lower(header_name);
    string header_val = lines[j].substr(fst_col + 2);
    req.AddHeader(header_name, header_val);
  }

  return req;
}

}  // namespace hw4
