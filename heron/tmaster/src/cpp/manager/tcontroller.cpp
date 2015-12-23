#include <iostream>

#include "proto/messages.h"
#include "basics/basics.h"
#include "errors/errors.h"
#include "threads/threads.h"
#include "network/network.h"

#include "manager/tmaster.h"
#include "manager/tcontroller.h"

namespace heron { namespace tmaster {

TController::TController(EventLoop* eventLoop,
                         const NetworkOptions& options,
                         TMaster* tmaster)
  : tmaster_(tmaster)
{
  http_server_ = new HTTPServer(eventLoop, options);
  // Install the handlers
  auto cbActivate = [this] (IncomingHTTPRequest* request) {
    this->HandleActivateRequest(request);
  };

  http_server_->InstallCallBack("/activate", std::move(cbActivate));

  auto cbDeActivate = [this] (IncomingHTTPRequest* request) {
    this->HandleDeActivateRequest(request);
  };

  http_server_->InstallCallBack("/deactivate", std::move(cbDeActivate));
}

TController::~TController()
{
  delete http_server_;
}

sp_int32 TController::Start()
{
  return http_server_->Start();
}

void TController::HandleActivateRequest(IncomingHTTPRequest* request)
{
  LOG(INFO) << "Got a activate topology request from "
            << request->GetRemoteHost() << ":"
            << request->GetRemotePort();
  const sp_string& id = request->GetValue("topologyid");
  if (id == "") {
    LOG(ERROR) << "topologyid not specified in the request";
    http_server_->SendErrorReply(request, 400);
    delete request;
    return;
  }
  if (tmaster_->getTopology() == NULL) {
    LOG(ERROR) << "Tmaster still not initialized";
    http_server_->SendErrorReply(request, 500);
    delete request;
    return;
  }
  if (id != tmaster_->getTopology()->id()) {
    LOG(ERROR) << "Topology id does not match";
    http_server_->SendErrorReply(request, 400);
    delete request;
    return;
  }
  if (tmaster_->getTopology()->state() != proto::api::PAUSED) {
    LOG(ERROR) << "Topology not in paused state";
    http_server_->SendErrorReply(request, 400);
    delete request;
    return;
  }

  auto cb = [request, this] (proto::system::StatusCode status) {
    this->HandleActivateRequestDone(request, status);
  };

  tmaster_->ActivateTopology(std::move(cb));
}

void TController::HandleActivateRequestDone(IncomingHTTPRequest* request,
                                            proto::system::StatusCode _status)
{
  if (_status != proto::system::OK) {
    LOG(ERROR) << "Unable to Activate topology " << _status;
    http_server_->SendErrorReply(request, 500);
  } else {
    sp_string s = "Topology successfully activated";
    LOG(INFO) << s;
    OutgoingHTTPResponse* response = new OutgoingHTTPResponse(request);
    response->AddResponse(s);
    http_server_->SendReply(request, 200, response);
  }
  delete request;
}

void TController::HandleDeActivateRequest(IncomingHTTPRequest* request)
{
  LOG(INFO) << "Got a deactivate topology request from "
            << request->GetRemoteHost() << ":"
            << request->GetRemotePort();
  const sp_string& id = request->GetValue("topologyid");
  if (id == "") {
    LOG(ERROR) << "Request does not contain topology id";
    http_server_->SendErrorReply(request, 400);
    delete request;
    return;
  }
  if (tmaster_->getTopology() == NULL) {
    LOG(ERROR) << "Tmaster still not initialized";
    http_server_->SendErrorReply(request, 500);
    delete request;
    return;
  }
  if (id != tmaster_->getTopology()->id()) {
    LOG(ERROR) << "Topology id does not match";
    http_server_->SendErrorReply(request, 400);
    delete request;
    return;
  }
  if (tmaster_->getTopology()->state() != proto::api::RUNNING) {
    LOG(ERROR) << "Topology not in running state";
    http_server_->SendErrorReply(request, 400);
    delete request;
    return;
  }

  auto cb = [request, this] (proto::system::StatusCode status) {
    this->HandleDeActivateRequestDone(request, status);
  };

  tmaster_->DeActivateTopology(std::move(cb));
}

void TController::HandleDeActivateRequestDone(IncomingHTTPRequest* request,
                                              proto::system::StatusCode _status)
{
  if (_status != proto::system::OK) {
    LOG(ERROR) << "Unable to DeActivate topology " << _status;
    http_server_->SendErrorReply(request, 500);
  } else {
    sp_string s = "Topology successfully deactivated";
    LOG(INFO) << s;
    OutgoingHTTPResponse* response = new OutgoingHTTPResponse(request);
    response->AddResponse(s);
    http_server_->SendReply(request, 200, response);
  }
  delete request;
}

}} // end of namespace