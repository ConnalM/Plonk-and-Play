#pragma once
#include <WiFi.h>
#include <esp_http_server.h>
#include <stdlib.h>
#include "noticeboard.h"

namespace pp {
// Stage 8 prototype transport. Browser clients read State/Facts and submit the
// fixed START request; HTTP handling runs in the framework server task.
class BrowserInterface {
public:
  struct ResultView { bool ready=false;RequestResult result=RequestResult::Rejected;RequestRejection rejection=RequestRejection::None; };
  BrowserInterface(Bus& bus,Bus::Endpoint endpoint,const Noticeboard& noticeboard):bus_(bus),endpoint_(endpoint),noticeboard_(noticeboard) {}
  void begin(){
    WiFi.persistent(false);WiFi.mode(WIFI_STA);WiFi.setAutoReconnect(true);
    WiFi.begin("Wokwi-GUEST","",6);started_=true;
  }
  void tick(){
    Message message;while(bus_.receive(endpoint_,message)){
      if(message.type==Type::NoticeboardChanged)++noticeRevision_;
      else if(message.type==Type::LapCompleted){lastFact_=message;hasFact_=true;++factRevision_;}
      else if(message.type==Type::GoScheduled){scheduledGo_=message.relevantTime;}
      else if(message.type==Type::RequestResult)recordResult(message);
    }
    if(started_&&!server_&&WiFi.status()==WL_CONNECTED)startServer();
  }
  // This accepts only a server-resolved trusted context. HTTP request content
  // never carries role/authority; the acceptance harness binds contexts here.
  bool submitStart(uint32_t correlation,ClientContext context){
    if(!correlation)return false;
    Message request{};request.type=Type::StartRequest;request.correlation=correlation;request.clientContext=context;
    return bus_.publish(endpoint_,request)==Delivery::Delivered;
  }
  bool requestResult(uint32_t correlation,ResultView& value)const{
    for(const auto& result:results_)if(result.ready&&result.correlation==correlation){value.ready=true;value.result=result.result;value.rejection=result.rejection;return true;}
    return false;
  }
  NoticeboardState current()const{return noticeboard_.current();}
  uint32_t noticeRevision()const{return noticeRevision_;}
  uint32_t factRevision()const{return factRevision_;}
  bool lastFact(Message& fact)const{if(!hasFact_)return false;fact=lastFact_;return true;}
  Time scheduledGo()const{return scheduledGo_;}
private:
  struct StoredResult { uint32_t correlation=0;RequestResult result=RequestResult::Rejected;RequestRejection rejection=RequestRejection::None;bool ready=false; };
  static BrowserInterface*& instance(){static BrowserInterface* value=nullptr;return value;}
  Bus& bus_;Bus::Endpoint endpoint_;const Noticeboard& noticeboard_;httpd_handle_t server_=nullptr;
  uint32_t noticeRevision_=0,factRevision_=0;Message lastFact_{};bool hasFact_=false,started_=false;Time scheduledGo_=0;
  StoredResult results_[4]{};size_t nextResult_=0;
  static const char* lifecycle(SessionLifecycle value){
    switch(value){case SessionLifecycle::Ready:return "READY";case SessionLifecycle::Starting:return "STARTING";case SessionLifecycle::Racing:return "RACING";case SessionLifecycle::Finished:return "FINISHED";}return "UNKNOWN";
  }
  static const char* rejectionText(RequestRejection value){
    switch(value){
      case RequestRejection::PermissionDenied:return "START permission denied";
      case RequestRejection::LifecycleNotStartable:return "START is not valid in the current lifecycle state";
      case RequestRejection::InvalidRaceSetup:return "invalid race setup: lap target must be at least 1";
      case RequestRejection::RequiredCapabilityUnavailable:return "required Start/Finish capability unavailable";
      case RequestRejection::SessionDefinitionUnavailable:return "unable to establish Session Definition";
      default:return "";
    }
  }
  void recordResult(const Message& message){
    for(auto& result:results_)if(result.ready&&result.correlation==message.correlation){result.result=message.requestResult;result.rejection=message.rejection;return;}
    auto& result=results_[nextResult_++%4];result.correlation=message.correlation;result.result=message.requestResult;result.rejection=message.rejection;result.ready=true;
  }
  static esp_err_t state(httpd_req_t* request){
    const auto value=instance()->current();char json[256];snprintf(json,sizeof(json),"{\"lifecycle\":\"%s\",\"raceEntryId\":%lu,\"laps\":%lu,\"hasLap\":%s,\"lastLapTime\":%llu,\"scheduledGo\":%llu}",lifecycle(value.lifecycle),(unsigned long)value.raceEntryId,(unsigned long)value.laps,value.hasLap?"true":"false",(unsigned long long)value.lastLapTime,(unsigned long long)value.scheduledGo);
    httpd_resp_set_type(request,"application/json");httpd_resp_set_hdr(request,"Cache-Control","no-store");return httpd_resp_sendstr(request,json);
  }
  static esp_err_t notice(httpd_req_t* request){
    char json[96];snprintf(json,sizeof(json),"{\"type\":\"NOTICEBOARD_CHANGED\",\"revision\":%lu}",(unsigned long)instance()->noticeRevision());
    httpd_resp_set_type(request,"application/json");httpd_resp_set_hdr(request,"Cache-Control","no-store");return httpd_resp_sendstr(request,json);
  }
  static esp_err_t fact(httpd_req_t* request){
    Message message{};char json[256];
    if(!instance()->lastFact(message))snprintf(json,sizeof(json),"{\"type\":\"NONE\",\"revision\":%lu}",(unsigned long)instance()->factRevision());
    else snprintf(json,sizeof(json),"{\"type\":\"LAP_COMPLETED\",\"revision\":%lu,\"raceEntryId\":%lu,\"lapNumber\":%lu,\"lapTime\":%llu,\"relevantTime\":%llu}",(unsigned long)instance()->factRevision(),(unsigned long)message.raceEntryId,(unsigned long)message.lapNumber,(unsigned long long)message.lapTime,(unsigned long long)message.relevantTime);
    httpd_resp_set_type(request,"application/json");httpd_resp_set_hdr(request,"Cache-Control","no-store");return httpd_resp_sendstr(request,json);
  }
  static bool correlationFromBody(httpd_req_t* request,uint32_t& correlation){
    if(request->content_len<=0||request->content_len>=80)return false;char body[80]{};int received=httpd_req_recv(request,body,request->content_len);if(received!=request->content_len)return false;
    const char* key=strstr(body,"\"correlationId\"");if(!key)return false;const char* colon=strchr(key,':');if(!colon)return false;
    char* end=nullptr;unsigned long value=strtoul(colon+1,&end,10);if(!end||end==colon+1||value==0||value>UINT32_MAX)return false;correlation=uint32_t(value);return true;
  }
  // Stage 8's deterministic local Browser client is a server-configured Race
  // Director/SMUG context. No client header or JSON value can change this.
  static ClientContext trustedHttpContext(const httpd_req_t*){return ClientContext::RaceDirectorSmug;}
  static esp_err_t start(httpd_req_t* request){
    uint32_t correlation=0;if(!correlationFromBody(request,correlation)){httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"correlationId required");return ESP_FAIL;}
    if(!instance()->submitStart(correlation,trustedHttpContext(request))){httpd_resp_set_status(request,"503 Service Unavailable");httpd_resp_sendstr(request,"request unavailable");return ESP_FAIL;}
    char json[96];snprintf(json,sizeof(json),"{\"submitted\":true,\"correlationId\":%lu}",(unsigned long)correlation);httpd_resp_set_status(request,"202 Accepted");httpd_resp_set_type(request,"application/json");httpd_resp_set_hdr(request,"Cache-Control","no-store");return httpd_resp_sendstr(request,json);
  }
  static esp_err_t result(httpd_req_t* request){
    char query[64]{};char value[16]{};if(httpd_req_get_url_query_str(request,query,sizeof(query))!=ESP_OK||httpd_query_key_value(query,"correlationId",value,sizeof(value))!=ESP_OK){httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"correlationId required");return ESP_FAIL;}
    char* end=nullptr;unsigned long raw=strtoul(value,&end,10);if(!end||*end||raw==0||raw>UINT32_MAX){httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"invalid correlationId");return ESP_FAIL;}
    ResultView found{};if(!instance()->requestResult(uint32_t(raw),found)){httpd_resp_set_status(request,"204 No Content");return httpd_resp_send(request,nullptr,0);}
    char json[192];if(found.result==RequestResult::Accepted)snprintf(json,sizeof(json),"{\"correlationId\":%lu,\"result\":\"ACCEPTED\"}",raw);else snprintf(json,sizeof(json),"{\"correlationId\":%lu,\"result\":\"REJECTED\",\"reason\":\"%s\"}",raw,rejectionText(found.rejection));
    httpd_resp_set_type(request,"application/json");httpd_resp_set_hdr(request,"Cache-Control","no-store");return httpd_resp_sendstr(request,json);
  }
  static esp_err_t page(httpd_req_t* request){
    static const char html[]=R"HTML(<!doctype html><meta charset="utf-8"><title>P&P Stage 8</title><style>body{font:16px system-ui;max-width:44rem;margin:2rem auto;padding:0 1rem}pre{background:#eee;padding:1rem}button{font:inherit;padding:.5rem 1rem}</style><h1>P&amp;P diagnostic Browser</h1><p id="connection">connecting</p><button id="start">START</button><p id="request"></p><pre id="state"></pre><pre id="fact"></pre><script>let syncedNotice=null,hasState=false,nextCorrelation=1;const state=document.querySelector('#state'),fact=document.querySelector('#fact'),connection=document.querySelector('#connection'),request=document.querySelector('#request');async function get(path){let r=await fetch(path,{cache:'no-store'});if(r.status===204)return null;if(!r.ok)throw Error(r.status);return r.json()}async function refresh(revision){let value=await get('/state');state.textContent=JSON.stringify(value,null,2);syncedNotice=revision;hasState=true}async function poll(){try{let n=await get('/noticeboard');if(!hasState||n.revision!==syncedNotice){connection.textContent='unsynchronised';await refresh(n.revision)}connection.textContent='synchronised'}catch(e){connection.textContent='unsynchronised';return}try{fact.textContent=JSON.stringify(await get('/fact'),null,2)}catch(e){}}document.querySelector('#start').onclick=async()=>{let correlation=nextCorrelation++;let r=await fetch('/request/start',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({correlationId:correlation})});if(r.status!==202){request.textContent='submission failed';return}request.textContent='START submitted';let timer=setInterval(async()=>{let result=await get('/request-result?correlationId='+correlation);if(result){request.textContent=JSON.stringify(result);clearInterval(timer)}},100)};poll();setInterval(poll,250)</script>)HTML";
    httpd_resp_set_type(request,"text/html; charset=utf-8");httpd_resp_set_hdr(request,"Cache-Control","no-store");return httpd_resp_send(request,html,sizeof(html)-1);
  }
  static esp_err_t health(httpd_req_t* request){httpd_resp_set_type(request,"application/json");return httpd_resp_sendstr(request,"{\"ok\":true,\"browser\":\"stage8\"}");}
  void startServer(){
    instance()=this;httpd_config_t config=HTTPD_DEFAULT_CONFIG();config.server_port=80;config.stack_size=8192;config.max_open_sockets=7;
    if(httpd_start(&server_,&config)!=ESP_OK){server_=nullptr;return;}
    httpd_uri_t routes[]={{.uri="/",.method=HTTP_GET,.handler=page,.user_ctx=nullptr},{.uri="/state",.method=HTTP_GET,.handler=state,.user_ctx=nullptr},{.uri="/noticeboard",.method=HTTP_GET,.handler=notice,.user_ctx=nullptr},{.uri="/fact",.method=HTTP_GET,.handler=fact,.user_ctx=nullptr},{.uri="/health",.method=HTTP_GET,.handler=health,.user_ctx=nullptr},{.uri="/request/start",.method=HTTP_POST,.handler=start,.user_ctx=nullptr},{.uri="/request-result",.method=HTTP_GET,.handler=result,.user_ctx=nullptr}};
    for(auto& route:routes)if(httpd_register_uri_handler(server_,&route)!=ESP_OK){httpd_stop(server_);server_=nullptr;return;}
  }
};
} // namespace pp