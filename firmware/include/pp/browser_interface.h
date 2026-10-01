#pragma once
#include <WiFi.h>
#include <esp_http_server.h>
#include "noticeboard.h"

namespace pp {
// Stage 7 prototype transport. Browser clients only read State/Facts and make
// no Requests. HTTP handling is in the framework server task, not race ticks.
class BrowserInterface {
public:
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
    }
    if(started_&&!server_&&WiFi.status()==WL_CONNECTED)startServer();
  }
  NoticeboardState current()const{return noticeboard_.current();}
  uint32_t noticeRevision()const{return noticeRevision_;}
  uint32_t factRevision()const{return factRevision_;}
  bool lastFact(Message& fact)const{if(!hasFact_)return false;fact=lastFact_;return true;}
  Time scheduledGo()const{return scheduledGo_;}
private:
  static BrowserInterface*& instance(){static BrowserInterface* value=nullptr;return value;}
  Bus& bus_;Bus::Endpoint endpoint_;const Noticeboard& noticeboard_;httpd_handle_t server_=nullptr;
  uint32_t noticeRevision_=0,factRevision_=0;Message lastFact_{};bool hasFact_=false,started_=false;Time scheduledGo_=0;
  static const char* lifecycle(SessionLifecycle value){
    switch(value){case SessionLifecycle::Ready:return "READY";case SessionLifecycle::Starting:return "STARTING";case SessionLifecycle::Racing:return "RACING";case SessionLifecycle::Finished:return "FINISHED";}return "UNKNOWN";
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
  static esp_err_t page(httpd_req_t* request){
    static const char html[]=R"HTML(<!doctype html><meta charset="utf-8"><title>P&P Stage 7</title><style>body{font:16px system-ui;max-width:44rem;margin:2rem auto;padding:0 1rem}pre{background:#eee;padding:1rem}</style><h1>P&amp;P diagnostic Browser</h1><p id="connection">connecting</p><pre id="state"></pre><pre id="fact"></pre><script>let syncedNotice=null,hasState=false;const state=document.querySelector('#state'),fact=document.querySelector('#fact'),connection=document.querySelector('#connection');async function get(path){let r=await fetch(path,{cache:'no-store'});if(!r.ok)throw Error(r.status);return r.json()}async function refresh(revision){let value=await get('/state');state.textContent=JSON.stringify(value,null,2);syncedNotice=revision;hasState=true}async function poll(){try{let n=await get('/noticeboard');if(!hasState||n.revision!==syncedNotice){connection.textContent='unsynchronised';await refresh(n.revision)}connection.textContent='synchronised'}catch(e){connection.textContent='unsynchronised';return}try{fact.textContent=JSON.stringify(await get('/fact'),null,2)}catch(e){}}poll();setInterval(poll,250)</script>)HTML";
    httpd_resp_set_type(request,"text/html; charset=utf-8");httpd_resp_set_hdr(request,"Cache-Control","no-store");return httpd_resp_send(request,html,sizeof(html)-1);
  }
  static esp_err_t health(httpd_req_t* request){httpd_resp_set_type(request,"application/json");return httpd_resp_sendstr(request,"{\"ok\":true,\"browser\":\"stage7\"}");}
  void startServer(){
    instance()=this;httpd_config_t config=HTTPD_DEFAULT_CONFIG();config.server_port=80;config.stack_size=8192;config.max_open_sockets=7;
    if(httpd_start(&server_,&config)!=ESP_OK){server_=nullptr;return;}
    httpd_uri_t routes[]={{.uri="/",.method=HTTP_GET,.handler=page,.user_ctx=nullptr},{.uri="/state",.method=HTTP_GET,.handler=state,.user_ctx=nullptr},{.uri="/noticeboard",.method=HTTP_GET,.handler=notice,.user_ctx=nullptr},{.uri="/fact",.method=HTTP_GET,.handler=fact,.user_ctx=nullptr},{.uri="/health",.method=HTTP_GET,.handler=health,.user_ctx=nullptr}};
    for(auto& route:routes)if(httpd_register_uri_handler(server_,&route)!=ESP_OK){httpd_stop(server_);server_=nullptr;return;}
  }
};
} // namespace pp
